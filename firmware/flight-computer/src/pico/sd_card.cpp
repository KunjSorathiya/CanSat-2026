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
// CMD0 is retried, because the first one after power-up is frequently not clean. R1's error
// bits -- illegal command, CRC error, erase sequence, erase reset -- are STICKY: they latch
// onto whatever the card made of the noise on an undriven bus while the rail came up, and
// they stay latched until a response is read. So a first CMD0 can legitimately return idle
// with several error bits set, which is what 0x1F is, and the act of reading it clears them.
// A single attempt reads that as a dead card and gives up on a healthy one.
constexpr int kCmd0Attempts = 10;
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
    stage_ = Stage::not_started;
    last_r1_ = 0xFF;
    init_wait_ms_ = 0;
    cmd0_attempts_ = 0;
    if (!hal_.select || !hal_.transfer) {
        stage_ = Stage::bad_hal;
        return false;
    }

    if (hal_.set_baudrate) hal_.set_baudrate(hal_.ctx, kInitBaud);

    // The specification asks for at least 74 clocks with CS high to enter native SPI mode.
    // 80 was the old figure and is the bare minimum; a card whose internal supply is still
    // settling wants more, and extra idle clocks cost microseconds.
    select(false);
    clock_bytes(20);

    select(true);
    stage_ = Stage::cmd0_idle;
    std::uint8_t r1 = 0xFF;
    for (cmd0_attempts_ = 1; cmd0_attempts_ <= kCmd0Attempts; ++cmd0_attempts_) {
        r1 = command(0, 0, 0x95);  // CMD0: GO_IDLE_STATE
        if (r1 == R1_IDLE) break;
        // Not clean yet. If the card answered at all -- bit 7 clear makes it a real R1
        // rather than an undriven line -- then reading that response has just cleared the
        // sticky error bits, and the next attempt gets a clean answer. If it did not
        // answer, a few more idle clocks give a slow card longer to wake.
        select(false);
        clock_bytes(2);
        select(true);
        if (hal_.delay_ms) hal_.delay_ms(hal_.ctx, 1);
    }
    last_r1_ = r1;
    if (r1 != R1_IDLE) {
        release();
        return false;
    }

    stage_ = Stage::cmd8_ifcond;
    r1 = command(8, 0x000001AA, 0x87);  // CMD8: SEND_IF_COND
    last_r1_ = r1;
    bool v2 = false;
    if (r1 == R1_IDLE) {
        std::uint8_t resp[4];
        for (auto& byte : resp) byte = transfer(0xFF);
        // Echo-back check: the card must return our voltage range and check pattern.
        v2 = (resp[2] == 0x01 && resp[3] == 0xAA);
    }

    // ACMD41 initialisation loop, bounded by wall-clock time rather than iterations so a
    // slow card gets its full allowance and a dead one still gives up.
    stage_ = Stage::acmd41_ready;
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
        init_wait_ms_ = elapsed;
        if (elapsed >= kInitTimeoutMs) break;
    }
    last_r1_ = r1;
    if (r1 != 0x00) {
        release();
        return false;
    }

    if (v2) {
        stage_ = Stage::cmd58_ocr;
        r1 = command(58, 0, 0xFD);  // CMD58: READ_OCR
        if (r1 == 0x00) {
            std::uint8_t ocr[4];
            for (auto& byte : ocr) byte = transfer(0xFF);
            sdhc_ = (ocr[0] & 0x40) != 0;  // CCS: block addressing rather than byte
        }
    }
    if (!sdhc_) {
        // CMD16: force 512-byte blocks on a standard-capacity card. If this fails the
        // card may be using a different block length, and every read and write after it
        // would be silently wrong.
        stage_ = Stage::cmd16_blocklen;
        last_r1_ = command(16, kBlockSize, 0xFF);
        if (last_r1_ != 0x00) {
            release();
            return false;
        }
    }

    release();
    if (hal_.set_baudrate) hal_.set_baudrate(hal_.ctx, kRunBaud);
    stage_ = Stage::complete;
    ok_ = true;
    return true;
}

const char* SdCard::describe(Stage stage) {
    switch (stage) {
        case Stage::not_started:
            return "nothing was attempted";
        case Stage::bad_hal:
            return "the caller supplied an incomplete hardware binding - a firmware fault";
        case Stage::cmd0_idle:
            return "CMD0 got no idle response: the card never answered at all. Wiring, "
                   "power, or a card that is not seated";
        case Stage::cmd8_ifcond:
            return "CMD0 worked but CMD8 did not: the bus is alive and the card is "
                   "talking, so this is the card rather than the wiring";
        case Stage::acmd41_ready:
            return "the card answered but never left idle: ACMD41 timed out. A card fault, "
                   "or a supply that sags when the card draws current";
        case Stage::cmd58_ocr:
            return "READ_OCR failed after the card was ready - unusual, suspect the bus";
        case Stage::cmd16_blocklen:
            return "SET_BLOCKLEN was refused on a standard-capacity card";
        case Stage::complete:
            return "complete";
    }
    return "unknown";
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

    write_stage_ = WriteStage::none;
    select(true);
    // A card still busy from the previous write ignores commands, so wait for it first.
    if (wait_ready() != 0xFF) {
        write_stage_ = WriteStage::busy_before;
        release();
        return false;
    }
    last_write_r1_ = command(24, addr, 0xFF);  // CMD24: WRITE_BLOCK
    if (last_write_r1_ != 0x00) {
        write_stage_ = WriteStage::cmd24_rejected;
        release();
        return false;
    }
    transfer(0xFF);  // one byte gap before the data token
    transfer(TOKEN_START_BLOCK);
    for (std::size_t i = 0; i < kBlockSize; ++i) transfer(in512[i]);
    transfer(0xFF);  // CRC, ignored in SPI mode
    transfer(0xFF);

    last_data_response_ = transfer(0xFF);
    if ((last_data_response_ & 0x1F) != 0x05) {  // 0b00101 = data accepted
        write_stage_ = WriteStage::data_rejected;
        release();
        return false;
    }
    if (wait_ready() != 0xFF) {  // wait out the programming busy period
        write_stage_ = WriteStage::programming_timeout;
        release();
        return false;
    }
    // CMD13 (SEND_STATUS) confirms the card recorded no write error. This is the only
    // place a card admits to write protection, so the R2 byte is kept whatever happens.
    last_write_r1_ = command(13, 0, 0xFF);
    last_write_r2_ = transfer(0xFF);
    release();
    if (last_write_r1_ != 0x00 || last_write_r2_ != 0x00) {
        write_stage_ = WriteStage::status_error;
        return false;
    }
    write_stage_ = WriteStage::complete;
    return true;
}

const char* SdCard::describe(WriteStage stage) {
    switch (stage) {
        case WriteStage::none: return "no write attempted";
        case WriteStage::busy_before:
            return "the card was still busy from the previous write and never came ready";
        case WriteStage::cmd24_rejected:
            return "WRITE_BLOCK was refused outright - the card would not even start";
        case WriteStage::data_rejected:
            return "the card refused the data block itself";
        case WriteStage::programming_timeout:
            return "the data was accepted but programming never finished";
        case WriteStage::status_error:
            return "the write appeared to work and CMD13 then reported an error";
        case WriteStage::complete: return "complete";
    }
    return "unknown";
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
