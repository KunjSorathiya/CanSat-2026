#include "flight/gps_parser.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace flight {

namespace {

// One international nautical mile per hour, exactly 1852 m / 3600 s. NMEA reports speed
// over ground in knots; every consumer in this project works in SI.
constexpr double kKnotToMps = 1852.0 / 3600.0;

int hex_value(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return -1;
}

// Strict double parse: whole token must be a finite number.
bool parse_double(const char* s, double& out) {
    if (s == nullptr || *s == '\0') return false;
    char* end = nullptr;
    const double value = std::strtod(s, &end);
    if (end == s || *end != '\0' || !std::isfinite(value)) return false;
    out = value;
    return true;
}

// NMEA coordinates are ddmm.mmmm (latitude) or dddmm.mmmm (longitude). `limit` is the
// mathematical bound for the axis: 90 for latitude, 180 for longitude. A sentence can pass
// its checksum and still carry an impossible position — a corrupted field, a module fault,
// or a hemisphere character that does not belong to this axis. Rejecting here keeps an
// impossible fix out of the telemetry stream rather than leaving the ground station to
// notice it.
//
// `positive` and `negative` are the only two characters this axis accepts: N/S for a
// latitude, E/W for a longitude. Taking any of the four on either axis would let a
// latitude carrying 'E' through as a northern one, and a latitude carrying 'W' through
// with its sign flipped — a position on the wrong side of the equator, from a sentence
// that was already telling us something was wrong with it.
bool parse_coordinate(const char* dm, char hemisphere, double limit,
                      char positive, char negative, double& out) {
    double raw = 0.0;
    if (!parse_double(dm, raw) || raw < 0.0) return false;

    const double degrees = std::floor(raw / 100.0);
    const double minutes = raw - degrees * 100.0;
    if (minutes >= 60.0) return false;  // minutes field out of range

    double value = degrees + minutes / 60.0;
    if (!std::isfinite(value) || value > limit) return false;

    if (hemisphere == negative) {
        value = -value;
    } else if (hemisphere != positive) {
        return false;  // missing, corrupted, or from the other axis: sign is not assumed
    }
    out = value;
    return true;
}

bool parse_utc(const char* hhmmss, double& seconds_of_day) {
    double t = 0.0;
    if (!parse_double(hhmmss, t) || t < 0.0) return false;
    const double hh = std::floor(t / 10000.0);
    const double mm = std::floor((t - hh * 10000.0) / 100.0);
    const double ss = t - hh * 10000.0 - mm * 100.0;
    if (hh > 23.0 || mm > 59.0 || ss >= 61.0) return false;
    seconds_of_day = hh * 3600.0 + mm * 60.0 + ss;
    return true;
}

}  // namespace

bool NmeaParser::consume(char character) {
    if (character == '\r') {
        return false;
    }
    if (character == '\n') {
        bool applied = false;
        if (!overflowed_ && length_ > 0) {
            applied = apply_sentence();
        }
        length_ = 0;
        overflowed_ = false;
        return applied;
    }
    if (character == '$') {
        length_ = 0;
        overflowed_ = false;
    }
    if (length_ < kMaxSentence - 1) {
        buffer_[length_++] = character;
    } else {
        overflowed_ = true;  // resync on the next newline
    }
    return false;
}

bool NmeaParser::apply_sentence() {
    buffer_[length_] = '\0';
    if (buffer_[0] != '$') {
        return false;
    }

    // Locate the checksum delimiter.
    std::size_t star = 0;
    for (star = 1; star < length_; ++star) {
        if (buffer_[star] == '*') break;
    }
    if (star >= length_ || star + 2 >= length_) {
        return false;
    }
    const int hi = hex_value(buffer_[star + 1]);
    const int lo = hex_value(buffer_[star + 2]);
    if (hi < 0 || lo < 0) {
        return false;
    }
    std::uint8_t checksum = 0;
    for (std::size_t i = 1; i < star; ++i) {
        checksum ^= static_cast<std::uint8_t>(buffer_[i]);
    }
    if (checksum != static_cast<std::uint8_t>((hi << 4) | lo)) {
        ++checksum_errors_;
        return false;
    }

    // Tokenise [1, star) on commas, in place.
    char* fields[26];
    std::size_t field_count = 0;
    fields[field_count++] = &buffer_[1];
    for (std::size_t i = 1; i < star && field_count < 26; ++i) {
        if (buffer_[i] == ',') {
            buffer_[i] = '\0';
            fields[field_count++] = &buffer_[i + 1];
        }
    }
    buffer_[star] = '\0';

    const char* type = fields[0];
    const std::size_t type_len = std::strlen(type);
    if (type_len < 5) {
        return false;
    }
    const char* code = type + (type_len - 3);  // strip the talker id (GP/GN/GL/...)

    if (std::strcmp(code, "GGA") == 0 && field_count >= 11) {
        double t = 0.0;
        if (parse_utc(fields[1], t)) {
            latest_.time_of_day_s = t;
            latest_.time_valid = true;
        }
        long quality = std::strtol(fields[6], nullptr, 10);
        if (quality <= 0) {
            latest_.valid = false;
            latest_.course_valid = false;  // no fix means no ground track either
            ++sentences_parsed_;
            return true;
        }
        double lat = 0.0;
        double lon = 0.0;
        double alt = 0.0;
        if (parse_coordinate(fields[2], fields[3][0], 90.0, 'N', 'S', lat) &&
            parse_coordinate(fields[4], fields[5][0], 180.0, 'E', 'W', lon) &&
            parse_double(fields[9], alt) &&
            alt >= kMinGpsAltitudeM && alt <= kMaxGpsAltitudeM) {
            const long sats = std::strtol(fields[7], nullptr, 10);
            const std::uint8_t satellites =
                (sats < 0) ? 0 : static_cast<std::uint8_t>(sats > 255 ? 255 : sats);
            // An absent HDOP field is not a good one. Treated as unusable rather than as
            // zero, which would be the best possible geometry and would pass every gate.
            double hdop = 0.0;
            const bool hdop_present = parse_double(fields[8], hdop) && hdop > 0.0;

            // The quality gates. Refusing a fix leaves the previous one exactly as it was:
            // a poor sentence must not erase a position the vehicle already had, and it
            // must not be mistaken for the receiver reporting no fix, which is what the
            // quality <= 0 path above means.
            if (satellites < kMinGpsSatellites || !hdop_present || hdop > kMaxGpsHdop) {
                ++fixes_rejected_;
                ++sentences_parsed_;
                return true;
            }

            latest_.latitude = lat;
            latest_.longitude = lon;
            latest_.altitude = alt;
            latest_.satellites = satellites;
            latest_.hdop = hdop;
            latest_.valid = true;
            ++sentences_parsed_;
            return true;
        }
        latest_.valid = false;
        return false;
    }

    if (std::strcmp(code, "RMC") == 0 && field_count >= 7) {
        double t = 0.0;
        if (parse_utc(fields[1], t)) {
            latest_.time_of_day_s = t;
            latest_.time_valid = true;
        }
        const bool active = fields[2][0] == 'A';
        if (!active) {
            latest_.valid = false;
            latest_.course_valid = false;
            ++sentences_parsed_;
            return true;
        }
        double lat = 0.0;
        double lon = 0.0;
        if (parse_coordinate(fields[3], fields[4][0], 90.0, 'N', 'S', lat) &&
            parse_coordinate(fields[5], fields[6][0], 180.0, 'E', 'W', lon)) {
            latest_.latitude = lat;
            latest_.longitude = lon;
            latest_.valid = true;

            // Ground track. Both fields are legitimately empty on a receiver that has a
            // fix but is not moving, so absence is not an error -- it just means there is
            // no course to report, and the previous one must not be left standing.
            latest_.course_valid = false;
            latest_.speed_mps = 0.0;
            if (field_count >= 9) {
                double knots = 0.0;
                if (parse_double(fields[7], knots) && knots >= 0.0) {
                    latest_.speed_mps = knots * kKnotToMps;
                }
                double course = 0.0;
                if (parse_double(fields[8], course) && course >= 0.0 && course < 360.0) {
                    latest_.course_deg = course;
                    latest_.course_valid = true;
                }
            }
            ++sentences_parsed_;
            return true;
        }
        latest_.valid = false;
        latest_.course_valid = false;
        return false;
    }

    return false;
}

}  // namespace flight
