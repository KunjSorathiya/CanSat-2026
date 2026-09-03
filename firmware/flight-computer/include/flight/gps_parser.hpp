#pragma once

#include "cansat/telemetry.hpp"

#include <cstddef>
#include <cstdint>

namespace flight {

// Streaming NMEA-0183 parser for the NEO-6M. Fixed-size line buffer, no exceptions and
// no dynamic allocation. Parses GGA (fix, position, altitude, satellites) and RMC
// (UTC time). A malformed or overlong sentence is discarded without disturbing the last
// good fix.
class NmeaParser {
public:
    static constexpr std::size_t kMaxSentence = 100;  // NMEA sentences are <= 82 chars + margin

    // Feed one received byte. Returns true when a complete, checksum-valid sentence of a
    // recognised type was applied.
    bool consume(char character);

    const cansat::GpsData& latest() const { return latest_; }
    bool has_fix() const { return latest_.valid; }
    std::uint32_t checksum_errors() const { return checksum_errors_; }
    std::uint32_t sentences_parsed() const { return sentences_parsed_; }

private:
    bool apply_sentence();  // operates on buffer_[0 .. length_)

    char buffer_[kMaxSentence] = {};
    std::size_t length_ = 0;
    bool overflowed_ = false;
    cansat::GpsData latest_{};
    std::uint32_t checksum_errors_ = 0;
    std::uint32_t sentences_parsed_ = 0;
};

}  // namespace flight
