#include "flight/pico/sd_card.hpp"

#ifdef PICO_BUILD
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/time.h"
#endif

namespace flight::pico {

#ifdef PICO_BUILD

namespace {
constexpr std::uint8_t R1_IDLE = 0x01;
constexpr std::uint8_t TOKEN_START_BLOCK = 0xFE;
constexpr std::uint32_t kInitBaud = 400000;
constexpr std::uint32_t kRunBaud = 4000000;
}  // namespace

void SdCard::select(bool on) {
    gpio_put(cs_gpio_, on ? 0 : 1);
}

std::uint8_t SdCard::transfer(std::uint8_t value) {
    std::uint8_t rx = 0xFF;
    spi_write_read_blocking(spi_, &value, &rx, 1);
    return rx;
}

void SdCard::clock_bytes(std::size_t n) {
    for (std::size_t i = 0; i < n; ++i) transfer(0xFF);
}

std::uint8_t SdCard::wait_ready() {
    for (int i = 0; i < 50000; ++i) {
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
    for (int i = 0; i < 16; ++i) {
        r1 = transfer(0xFF);
        if ((r1 & 0x80) == 0) break;
    }
    return r1;
}

bool SdCard::begin(spi_bus_t spi, std::uint32_t cs_gpio) {
    spi_ = spi;
    cs_gpio_ = cs_gpio;
    ok_ = false;
    sdhc_ = false;

    gpio_init(cs_gpio_);
    gpio_set_dir(cs_gpio_, GPIO_OUT);
    gpio_put(cs_gpio_, 1);

    spi_set_baudrate(spi_, kInitBaud);
    // >= 74 clocks with CS high to enter native SPI mode.
    select(false);
    clock_bytes(10);

    select(true);
    std::uint8_t r1 = command(0, 0, 0x95);  // CMD0: GO_IDLE_STATE
    if (r1 != R1_IDLE) {
        select(false);
        return false;
    }

    r1 = command(8, 0x000001AA, 0x87);  // CMD8: SEND_IF_COND
    bool v2 = false;
    if (r1 == R1_IDLE) {
        std::uint8_t resp[4];
        for (auto& b : resp) b = transfer(0xFF);
        v2 = (resp[2] == 0x01 && resp[3] == 0xAA);
    }

    // ACMD41 init loop.
    const std::uint32_t acmd41_arg = v2 ? 0x40000000u : 0u;
    absolute_time_t deadline = make_timeout_time_ms(2000);
    do {
        command(55, 0, 0x65);              // CMD55: APP_CMD
        r1 = command(41, acmd41_arg, 0x77);  // ACMD41: SD_SEND_OP_COND
        if (r1 == 0x00) break;
        sleep_ms(10);
    } while (!time_reached(deadline));
    if (r1 != 0x00) {
        select(false);
        return false;
    }

    if (v2) {
        r1 = command(58, 0, 0xFD);  // CMD58: READ_OCR
        if (r1 == 0x00) {
            std::uint8_t ocr[4];
            for (auto& b : ocr) b = transfer(0xFF);
            sdhc_ = (ocr[0] & 0x40) != 0;  // CCS bit
        }
    }
    if (!sdhc_) {
        command(16, kBlockSize, 0xFF);  // CMD16: force 512-byte blocks (SDSC)
    }

    select(false);
    spi_set_baudrate(spi_, kRunBaud);
    ok_ = true;
    return true;
}

bool SdCard::read_block(std::uint32_t lba, std::uint8_t* out512) {
    if (!ok_ || out512 == nullptr) return false;
    const std::uint32_t addr = sdhc_ ? lba : lba * kBlockSize;

    select(true);
    if (command(17, addr, 0xFF) != 0x00) {
        select(false);
        return false;
    }
    std::uint8_t token = 0xFF;
    for (int i = 0; i < 50000; ++i) {
        token = transfer(0xFF);
        if (token != 0xFF) break;
    }
    if (token != TOKEN_START_BLOCK) {
        select(false);
        return false;
    }
    for (std::size_t i = 0; i < kBlockSize; ++i) out512[i] = transfer(0xFF);
    transfer(0xFF);  // discard CRC
    transfer(0xFF);
    select(false);
    return true;
}

bool SdCard::write_block(std::uint32_t lba, const std::uint8_t* in512) {
    if (!ok_ || in512 == nullptr) return false;
    const std::uint32_t addr = sdhc_ ? lba : lba * kBlockSize;

    select(true);
    if (command(24, addr, 0xFF) != 0x00) {
        select(false);
        return false;
    }
    transfer(0xFF);
    transfer(TOKEN_START_BLOCK);
    for (std::size_t i = 0; i < kBlockSize; ++i) transfer(in512[i]);
    transfer(0xFF);  // CRC (ignored in SPI mode)
    transfer(0xFF);

    const std::uint8_t resp = transfer(0xFF);
    if ((resp & 0x1F) != 0x05) {  // data accepted token
        select(false);
        return false;
    }
    if (wait_ready() != 0xFF) {  // wait out the busy period
        select(false);
        return false;
    }
    // Confirm no write error via CMD13 (SEND_STATUS).
    const std::uint8_t st1 = command(13, 0, 0xFF);
    const std::uint8_t st2 = transfer(0xFF);
    select(false);
    return st1 == 0x00 && st2 == 0x00;
}

#else  // host stub

bool SdCard::begin(spi_bus_t, std::uint32_t) {
    ok_ = false;
    return false;
}
bool SdCard::read_block(std::uint32_t, std::uint8_t*) { return false; }
bool SdCard::write_block(std::uint32_t, const std::uint8_t*) { return false; }
void SdCard::select(bool) {}
std::uint8_t SdCard::transfer(std::uint8_t) { return 0xFF; }
void SdCard::clock_bytes(std::size_t) {}
std::uint8_t SdCard::command(std::uint8_t, std::uint32_t, std::uint8_t) { return 0xFF; }
std::uint8_t SdCard::wait_ready() { return 0x00; }

#endif

}  // namespace flight::pico
