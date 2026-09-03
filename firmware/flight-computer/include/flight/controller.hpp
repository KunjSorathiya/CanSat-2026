#pragma once

#include "flight/config.hpp"
#include "flight/interfaces.hpp"
#include "flight/state_machine.hpp"

#include <cstdint>

namespace flight {

class Controller {
public:
    Controller(Configuration config, Imu& imu, Barometer& barometer, Gps& gps,
               Radio& radio, SdLogger& logger, BoardIo& board);
    bool initialize();
    void poll(std::uint64_t now_ms);
    MissionState state() const;
    const char* last_error() const;

private:
    bool self_test();
    bool emit_telemetry(std::uint64_t now_ms);

    Configuration config_;
    Imu& imu_;
    Barometer& barometer_;
    Gps& gps_;
    Radio& radio_;
    SdLogger& logger_;
    BoardIo& board_;
    StateMachine state_machine_;
    std::uint64_t next_telemetry_ms_ = 0;
    std::uint32_t packet_number_ = 0;
    const char* last_error_ = "";
};

}  // namespace flight
