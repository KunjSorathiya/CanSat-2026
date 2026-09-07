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

// Fix quality, as opposed to fix presence. A receiver reports quality 1 the moment it has
// any solution at all, including a 2D one from three satellites and a 3D one whose
// satellites are bunched into one part of the sky. [F-18] measured what that costs: a
// receiver that never moved produced fixes 55.6 m apart in consecutive seconds and an
// altitude spanning 49.8 m, and every one of them was accepted, transmitted and logged.
//
// These two gates are deliberately about *geometry and count*, never about motion. A
// speed or step limit tuned on a bench would reject the real flight, which is falling
// under a parachute and drifting downwind at the moment position matters most; satellite
// count and HDOP mean the same thing whether the vehicle is on a table or descending.
//
// Four satellites is the minimum for a 3D solution. HDOP 5 is the boundary between
// "good" and "moderate" on the conventional scale -- loose on purpose, because a gate
// that rejects usable fixes during descent is a worse failure than one that passes a few
// poor ones.
inline constexpr std::uint8_t kMinGpsSatellites = 4;
inline constexpr double kMaxGpsHdop = 5.0;

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
    // Fixes the receiver offered and the gates refused. Counted rather than merely
    // dropped: a position that quietly stops updating and one that was never good
    // enough look identical from the ground otherwise.
    std::uint32_t fixes_rejected() const { return fixes_rejected_; }

private:
    bool apply_sentence();  // operates on buffer_[0 .. length_)

    char buffer_[kMaxSentence] = {};
    std::size_t length_ = 0;
    bool overflowed_ = false;
    cansat::GpsData latest_{};
    std::uint32_t checksum_errors_ = 0;
    std::uint32_t sentences_parsed_ = 0;
    std::uint32_t fixes_rejected_ = 0;
};

}  // namespace flight
