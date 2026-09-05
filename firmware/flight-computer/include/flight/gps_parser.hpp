#pragma once

#include "cansat/telemetry.hpp"

#include <cstddef>
#include <cstdint>

namespace flight {

// The altitude an NMEA fix may carry, in metres above mean sea level.
//
// Latitude and longitude have mathematical bounds -- 90 and 180 -- and the parser has
// always enforced them. Altitude has no such bound, and so had none: a sentence whose
// altitude field was corrupted into anything finite was accepted, transmitted as a
// reading, and written to both logs. NMEA carries an 8-bit checksum, so roughly one
// corruption in 256 reaches the parser looking valid.
//
// These bounds are deliberately loose. Their job is to catch corruption, not to
// second-guess the receiver: the lowest dry land on Earth is about -430 m, and no GPS
// receiver reports a fix anywhere near the top of this range. A value outside it is not a
// place.
inline constexpr double kMinGpsAltitudeM = -1000.0;
inline constexpr double kMaxGpsAltitudeM = 80000.0;

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
