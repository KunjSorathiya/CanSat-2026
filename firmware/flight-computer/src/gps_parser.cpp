#include "flight/gps_parser.hpp"

#include <cstdio>
#include <sstream>
#include <vector>

namespace flight {

namespace {
std::vector<std::string> split(const std::string& input, char delimiter) {
    std::vector<std::string> result;
    std::stringstream stream(input);
    std::string part;
    while (std::getline(stream, part, delimiter)) result.push_back(part);
    return result;
}

double coordinate(const std::string& value, const std::string& hemisphere) {
    double raw = std::stod(value);
    const double degrees = static_cast<int>(raw / 100.0);
    const double minutes = raw - degrees * 100.0;
    const double result = degrees + minutes / 60.0;
    return (hemisphere == "S" || hemisphere == "W") ? -result : result;
}
}  // namespace

bool NmeaParser::consume(char character) {
    if (sentence_.size() >= 128) sentence_.clear();
    sentence_ += character;
    if (character != '\n') return false;
    const bool parsed = parse_sentence(sentence_);
    sentence_.clear();
    return parsed;
}

const cansat::GpsData& NmeaParser::latest() const { return latest_; }
bool NmeaParser::has_fix() const { return latest_.valid; }
std::uint32_t NmeaParser::checksum_errors() const { return checksum_errors_; }

bool NmeaParser::parse_sentence(const std::string& sentence) {
    if (sentence.size() < 10 || sentence.front() != '$') return false;
    const auto star = sentence.find('*');
    if (star == std::string::npos || star + 2 >= sentence.size()) return false;
    std::uint8_t checksum = 0;
    for (std::size_t index = 1; index < star; ++index) checksum ^= static_cast<std::uint8_t>(sentence[index]);
    unsigned supplied = 0;
    if (std::sscanf(sentence.c_str() + star + 1, "%2x", &supplied) != 1 || checksum != supplied) {
        ++checksum_errors_;
        return false;
    }
    const auto fields = split(sentence.substr(1, star - 1), ',');
    if (fields.size() < 10 || (fields[0] != "GPGGA" && fields[0] != "GNGGA")) return false;
    try {
        const int fix_quality = std::stoi(fields[6]);
        if (fix_quality == 0) { latest_.valid = false; return true; }
        latest_.latitude = coordinate(fields[2], fields[3]);
        latest_.longitude = coordinate(fields[4], fields[5]);
        latest_.altitude = std::stod(fields[9]);
        latest_.valid = true;
        return true;
    } catch (...) {
        latest_.valid = false;
        return false;
    }
}

}  // namespace flight
