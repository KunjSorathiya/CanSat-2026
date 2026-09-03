#pragma once

#include "flight/config.hpp"
#include "flight/interfaces.hpp"

#include <cstdint>
#include <string>

namespace flight {

class StateMachine {
public:
    explicit StateMachine(const Configuration& config);
    MissionState state() const;
    void update(std::uint64_t now_ms, bool self_test_ok, bool telemetry_ok);
    void report_fault(FaultSeverity severity, const std::string& message);
    const std::string& last_fault() const;

private:
    const Configuration& config_;
    MissionState state_ = MissionState::init;
    std::string last_fault_;
};

}  // namespace flight
