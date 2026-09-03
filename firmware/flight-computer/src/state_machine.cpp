#include "flight/state_machine.hpp"

namespace flight {

StateMachine::StateMachine(const Configuration& config) : config_(config) {}

MissionState StateMachine::state() const { return state_; }

void StateMachine::update(std::uint64_t now_ms, bool self_test_ok, bool telemetry_ok) {
    switch (state_) {
    case MissionState::init:
        state_ = MissionState::self_test;
        break;
    case MissionState::self_test:
        state_ = self_test_ok ? MissionState::ready : MissionState::fault;
        break;
    case MissionState::ready:
        if (!telemetry_ok) state_ = MissionState::fault;
        break;
    case MissionState::flight:
    case MissionState::landed:
    case MissionState::recovery:
    case MissionState::fault:
        break;
    }
    (void)now_ms;
}

void StateMachine::report_fault(FaultSeverity severity, const std::string& message) {
    last_fault_ = message;
    if (severity == FaultSeverity::critical) state_ = MissionState::fault;
}

const std::string& StateMachine::last_fault() const { return last_fault_; }

}  // namespace flight
