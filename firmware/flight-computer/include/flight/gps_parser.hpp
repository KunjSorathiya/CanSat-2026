#pragma once

#include "cansat/telemetry.hpp"

#include <cstdint>
#include <string>

namespace flight {

class NmeaParser {
public:
    bool consume(char character);
    const cansat::GpsData& latest() const;
    bool has_fix() const;
    std::uint32_t checksum_errors() const;

private:
    bool parse_sentence(const std::string& sentence);
    std::string sentence_;
    cansat::GpsData latest_;
    std::uint32_t checksum_errors_ = 0;
};

}  // namespace flight
