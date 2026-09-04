#include "flight/gps_parser.hpp"

#include <cmath>
#include <cstdlib>
#include <cstring>

namespace flight {

namespace {

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
// or a hemisphere character that is neither N/S nor E/W. Rejecting here keeps an
// impossible fix out of the telemetry stream rather than leaving the ground station to
// notice it.
bool parse_coordinate(const char* dm, char hemisphere, double limit, double& out) {
    double raw = 0.0;
    if (!parse_double(dm, raw) || raw < 0.0) return false;

    const double degrees = std::floor(raw / 100.0);
    const double minutes = raw - degrees * 100.0;
    if (minutes >= 60.0) return false;  // minutes field out of range

    double value = degrees + minutes / 60.0;
    if (!std::isfinite(value) || value > limit) return false;

    if (hemisphere == 'S' || hemisphere == 'W') {
        value = -value;
    } else if (hemisphere != 'N' && hemisphere != 'E') {
        return false;  // missing or corrupted hemisphere: sign is unknown, not assumed
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
            ++sentences_parsed_;
            return true;
        }
        double lat = 0.0;
        double lon = 0.0;
        double alt = 0.0;
        if (parse_coordinate(fields[2], fields[3][0], 90.0, lat) &&
            parse_coordinate(fields[4], fields[5][0], 180.0, lon) &&
            parse_double(fields[9], alt)) {
            latest_.latitude = lat;
            latest_.longitude = lon;
            latest_.altitude = alt;
            long sats = std::strtol(fields[7], nullptr, 10);
            latest_.satellites = (sats < 0) ? 0 : static_cast<std::uint8_t>(sats > 255 ? 255 : sats);
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
            ++sentences_parsed_;
            return true;
        }
        double lat = 0.0;
        double lon = 0.0;
        if (parse_coordinate(fields[3], fields[4][0], 90.0, lat) &&
            parse_coordinate(fields[5], fields[6][0], 180.0, lon)) {
            latest_.latitude = lat;
            latest_.longitude = lon;
            latest_.valid = true;
            ++sentences_parsed_;
            return true;
        }
        latest_.valid = false;
        return false;
    }

    return false;
}

}  // namespace flight
