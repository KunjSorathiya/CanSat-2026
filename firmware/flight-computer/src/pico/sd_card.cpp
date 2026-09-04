#include "flight/pico/sd_card.hpp"

#ifdef PICO_BUILD
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"
#include "pico/time.h"
#endif

namespace flight::pico {

namespace {
constexpr std::uint8_t R1_IDLE = 0x01;
constexpr std::uint8_t TOKEN_START_BLOCK = 0xFE;

// Bounded retry counts. Every loop here has a ceiling: a card that never answers must
// fail the operation, never stall the flight loop.
constexpr int kR1Attempts = 16;        // R1 arrives within 8 bytes on a healthy card
constexpr int kTokenAttempts = 50000;  // ~100 ms at 4 MHz
constexpr int kReadyAttempts = 50000;
constexpr std::uint32_t kInitTimeoutMs = 2000;
}  // namespace

// ---------------------------------------------------------------- primitives --

void SdCard::select(bool on) {
    if (hal_.select) hal_.select(hal_.ctx, on);
}

std::uint8_t SdCard::transfer(std::uint8_t value) {
    return hal_.transfer ? hal_.transfer(hal_.ctx, value) : 0xFF;
}

void SdCard::clock_bytes(std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) transfer(0xFF);
}

void SdCard::release() {
    select(false);
    // One more byte with CS high: the card needs a clock edge to let go of DO. Without
    // this it can keep driving MISO, which on this shared SPI0 bus corrupts the radio's
    // very next transaction — a fault that presents as a dead radio, not a dead card.
    transfer(0xFF);
}

std::uint8_t SdCard::wait_ready() {
    for (int i = 0; i < kReadyAttempts; ++i) {
        if (transfer(0xFF) == 0xFF) return 0xFF;
    }
    return 0x00;
}

std::uint8_t SdCard::command(std::uint8_t cmd, std::uint32_t arg, std::uint8_t crc) {
    transfer(0xFF);
    transfer(static_cast<std::uint8_t>(0x40 | cmd));
    transfer(static_cast<std::uint8_t>(arg >> 24));
    transfer(static_cast<std::uint8_t>(arg >> 16));
    transfer(static_cast<std::uint8_t>(arg >> 8));
    transfer(static_cast<std::uint8_t>(arg));
    transfer(crc);
    std::uint8_t r1 = 0xFF;
    for (int i = 0; i < kR1Attempts; ++i) {
        r1 = transfer(0xFF);
        if ((r1 & 0x80) == 0) break;
    }
    return r1;
}

// ---------------------------------------------------------------- lifecycle --

bool SdCard::begin_with(const SdCardHal& hal) {
    hal_ = hal;
    ok_ = false;
    sdhc_ = false;
    if (!hal_.select || !hal_.transfer) {
        return false;
    }

    if (hal_.set_baudrate) hal_.set_baudrate(hal_.ctx, kInitBaud);

    // >= 74 clocks with CS high to enter native SPI mode.
    select(false);
    clock_bytes(10);

    select(true);
    std::uint8_t r1 = command(0, 0, 0x95);  // CMD0: GO_IDLE_STATE
    if (r1 != R1_IDLE) {
        release();
        return false;
    }

    r1 = command(8, 0x000001AA, 0x87);  // CMD8: SEND_IF_COND
    bool v2 = false;
    if (r1 == R1_IDLE) {
        std::uint8_t resp[4];
        for (auto& byte : resp) byte = transfer(0xFF);
        // Echo-back check: the card must return our voltage range and check pattern.
        v2 = (resp[2] == 0x01 && resp[3] == 0xAA);
    }

    // ACMD41 initialisation loop, bounded by wall-clock time rather than iterations so a
    // slow card gets its full allowance and a dead one still gives up.
    const std::uint32_t acmd41_arg = v2 ? 0x40000000u : 0u;  // HCS set for v2 cards
    const std::uint32_t start = hal_.millis ? hal_.millis(hal_.ctx) : 0;
    std::uint32_t waited = 0;
    while (true) {
        command(55, 0, 0x65);                // CMD55: APP_CMD
        r1 = command(41, acmd41_arg, 0x77);  // ACMD41: SD_SEND_OP_COND
        if (r1 == 0x00) break;
        if (hal_.delay_ms) hal_.delay_ms(hal_.ctx, 10);
        waited += 10;
        const std::uint32_t elapsed =
            hal_.millis ? (hal_.millis(hal_.ctx) - start) : waited;
        if (elapsed >= kInitTimeoutMs) break;
    }
    if (r1 != 0x00) {
        release();
        return false;
    }

    if (v2) {
        r1 = command(58, 0, 0xFD);  // CMD58: READ_OCR
        if (r1 == 0x00) {
            std::uint8_t ocr[4];
            for (auto& byte : ocr) byte = transfer(0xFF);
            sdhc_ = (ocr[0] & 0x40) != 0;  // CCS: block addressing rather than byte
        }
    }
    if (!sdhc_) {
        command(16, kBlockSize, 0xFF);  // CMD16: force 512-byte blocks (SDSC)
    }

    release();
    if (hal_.set_baudrate) hal_.set_baudrate(hal_.ctx, kRunBaud);
    ok_ = true;
    return true;
}

// ---------------------------------------------------------------- transfers --

bool SdCard::read_block(std::uint32_t lba, std::uint8_t* out512) {
    if (!ok_ || out512 == nullptr) return false;
    // SDHC/SDXC address in blocks; standard-capacity cards address in bytes.
    const std::uint32_t addr = sdhc_ ? lba : lba * kBlockSize;

    // The bus is shared with the radio, which may have changed the clock. Set the rate
    // this transfer needs rather than assuming whatever the last user left behind.
    if (hal_.set_baudrate) hal_.set_baudrate(hal_.ctx, kRunBaud);

    select(true);
    if (command(17, addr, 0xFF) != 0x00) {  // CMD17: READ_SINGLE_BLOCK
        release();
        return false;
    }
    std::uint8_t token = 0xFF;
    for (int i = 0; i < kTokenAttempts; ++i) {
        token = transfer(0xFF);
        if (token != 0xFF) break;
    }
    if (token != TOKEN_START_BLOCK) {
        release();
        return false;
    }
    for (std::size_t i = 0; i < kBlockSize; ++i) out512[i] = transfer(0xFF);
    transfer(0xFF);  // discard the 16-bit CRC
    transfer(0xFF);
    release();
    return true;
}

bool SdCard::write_block(std::uint32_t lba, const std::uint8_t* in512) {
    if (!ok_ || in512 == nullptr) return false;
    const std::uint32_t addr = sdhc_ ? lba : lba * kBlockSize;

    if (hal_.set_baudrate) hal_.set_baudrate(hal_.ctx, kRunBaud);

    select(true);
    // A card still busy from the previous write ignores commands, so wait for it first.
    if (wait_ready() != 0xFF) {
        release();
        return false;
    }
    if (command(24, addr, 0xFF) != 0x00) {  // CMD24: WRITE_BLOCK
        release();
        return false;
    }
    transfer(0xFF);  // one byte gap before the data token
    transfer(TOKEN_START_BLOCK);
    for (std::size_t i = 0; i < kBlockSize; ++i) transfer(in512[i]);
    transfer(0xFF);  // CRC, ignored in SPI mode
    transfer(0xFF);

    const std::uint8_t response = transfer(0xFF);
    if ((response & 0x1F) != 0x05) {  // 0b00101 = data accepted
        release();
        return false;
    }
    if (wait_ready() != 0xFF) {  // wait out the programming busy period
        release();
        return false;
    }
    // CMD13 (SEND_STATUS) confirms the card recorded no write error.
    const std::uint8_t status_r1 = command(13, 0, 0xFF);
    const std::uint8_t status_r2 = transfer(0xFF);
    release();
    return status_r1 == 0x00 && status_r2 == 0x00;
}

// ---------------------------------------------------------- Pico SDK bindings --

#ifdef PICO_BUILD

namespace {
struct PicoSdContext {
    spi_inst_t* spi = nullptr;
    std::uint32_t cs_gpio = 0;
};
PicoSdContext g_context;

void pico_select(void* ctx, bool on) {
    gpio_put(static_cast<PicoSdContext*>(ctx)->cs_gpio, on ? 0 : 1);
}
std::uint8_t pico_transfer(void* ctx, std::uint8_t value) {
    std::uint8_t rx = 0xFF;
    spi_write_read_blocking(static_cast<PicoSdContext*>(ctx)->spi, &value, &rx, 1);
    return rx;
}
void pico_set_baudrate(void* ctx, std::uint32_t hz) {
    spi_set_baudrate(static_cast<PicoSdContext*>(ctx)->spi, hz);
}
void pico_delay(void*, std::uint32_t ms) { sleep_ms(ms); }
std::uint32_t pico_millis(void*) { return to_ms_since_boot(get_absolute_time()); }
}  // namespace

bool SdCard::begin(spi_bus_t spi, std::uint32_t cs_gpio) {
    g_context.spi = spi;
    g_context.cs_gpio = cs_gpio;

    gpio_init(cs_gpio);
    gpio_set_dir(cs_gpio, GPIO_OUT);
    gpio_put(cs_gpio, 1);

    SdCardHal hal;
    hal.ctx = &g_context;
    hal.select = pico_select;
    hal.transfer = pico_transfer;
    hal.set_baudrate = pico_set_baudrate;
    hal.delay_ms = pico_delay;
    hal.millis = pico_millis;
    return begin_with(hal);
}

#else  // host build: no SPI peripheral, so the hardware entry point does nothing

bool SdCard::begin(spi_bus_t, std::uint32_t) {
    ok_ = false;
    return false;
}

#endif

}  // namespace flight::pico
