#pragma once

#include <cstdint>
#include <string>

namespace ground {

class RadioBridge {
public:
    virtual ~RadioBridge() = default;
    virtual bool initialize(std::uint8_t sync_word) = 0;
    virtual bool receive(std::string& packet) = 0;
    virtual bool healthy() const = 0;
};

}  // namespace ground
