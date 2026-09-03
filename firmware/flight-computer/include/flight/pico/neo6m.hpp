#pragma once

#include "cansat/telemetry.hpp"
#include "flight/gps_parser.hpp"
#include "flight/pico/pico_types.hpp"

#include <cstdint>

namespace flight::pico {

// NEO-6M GPS driver: initialises UART0 and drains received bytes into the NMEA parser.
// poll() reads at most kMaxBytesPerPoll bytes so a flooded UART can never stall the
// flight loop.
class Neo6mGps {
public:
    static constexpr int kMaxBytesPerPoll = 512;

    bool begin(uart_bus_t uart, std::uint32_t tx_gpio, std::uint32_t rx_gpio,
               std::uint32_t baud = 9600);
    void poll(std::uint64_t now_ms);
    bool latest(cansat::GpsData& data) const;
    std::uint32_t checksum_errors() const { return parser_.checksum_errors(); }
    std::uint64_t last_byte_ms() const { return last_byte_ms_; }

private:
    uart_bus_t uart_ = nullptr;
    NmeaParser parser_;
    std::uint64_t last_byte_ms_ = 0;
    bool ok_ = false;
};

}  // namespace flight::pico
