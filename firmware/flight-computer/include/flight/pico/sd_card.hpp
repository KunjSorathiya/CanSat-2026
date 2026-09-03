#pragma once

#include "flight/pico/pico_types.hpp"

#include <cstddef>
#include <cstdint>

namespace flight::pico {

// Minimal SD/SDHC card driver in SPI mode: CMD0/CMD8/ACMD41/CMD58/CMD16 init, then
// single-block CMD17 read and CMD24 write. 512-byte blocks only. No filesystem; pair with
// RawBlockLog for the onboard log.
class SdCard {
public:
    static constexpr std::size_t kBlockSize = 512;

    bool begin(spi_bus_t spi, std::uint32_t cs_gpio);
    bool read_block(std::uint32_t lba, std::uint8_t* out512);
    bool write_block(std::uint32_t lba, const std::uint8_t* in512);
    bool ok() const { return ok_; }
    bool high_capacity() const { return sdhc_; }

private:
    void select(bool on);
    std::uint8_t transfer(std::uint8_t value);
    void clock_bytes(std::size_t n);
    std::uint8_t command(std::uint8_t cmd, std::uint32_t arg, std::uint8_t crc);
    std::uint8_t wait_ready();

    spi_bus_t spi_ = nullptr;
    std::uint32_t cs_gpio_ = 0;
    bool ok_ = false;
    bool sdhc_ = false;
};

}  // namespace flight::pico
