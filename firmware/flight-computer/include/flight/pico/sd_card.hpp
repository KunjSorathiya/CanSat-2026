#pragma once

#include "flight/pico/pico_types.hpp"

#include <cstddef>
#include <cstdint>

namespace flight::pico {

// Hardware bindings the SD driver needs. Supplied by the Pico SDK on the vehicle and by a
// simulated card in host tests, exactly as the SX1278 driver does — the microSD reader is
// the project's highest-risk integration item, so its command sequence is worth executing
// somewhere other than a launch pad.
//
// No callback may block indefinitely.
struct SdCardHal {
    void* ctx = nullptr;
    // Assert (select = true -> CS low) or release the chip-select line.
    void (*select)(void* ctx, bool select) = nullptr;
    // Full-duplex byte exchange. Returns the byte clocked in while `value` goes out.
    std::uint8_t (*transfer)(void* ctx, std::uint8_t value) = nullptr;
    // Set the SPI clock. The card must be initialised at <= 400 kHz.
    void (*set_baudrate)(void* ctx, std::uint32_t hz) = nullptr;
    void (*delay_ms)(void* ctx, std::uint32_t ms) = nullptr;
    // Monotonic millisecond clock, used to bound the initialisation loop.
    std::uint32_t (*millis)(void* ctx) = nullptr;
};

// Minimal SD/SDHC card driver in SPI mode: CMD0/CMD8/ACMD41/CMD58/CMD16 init, then
// single-block CMD17 read and CMD24 write. 512-byte blocks only. No filesystem; pair with
// RawBlockLog for the onboard log.
class SdCard {
public:
    static constexpr std::size_t kBlockSize = 512;
    static constexpr std::uint32_t kInitBaud = 400000;
    static constexpr std::uint32_t kRunBaud = 4000000;

    // Vehicle entry point: builds the Pico SDK bindings and calls begin_with().
    bool begin(spi_bus_t spi, std::uint32_t cs_gpio);

    // Testable entry point: drives the card through the supplied bindings.
    bool begin_with(const SdCardHal& hal);

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
    // Deselect and clock one more byte so the card releases MISO. SPI0 is shared with the
    // radio: a card still driving the line corrupts the next radio transaction.
    void release();

    SdCardHal hal_{};
    bool ok_ = false;
    bool sdhc_ = false;
};

}  // namespace flight::pico
