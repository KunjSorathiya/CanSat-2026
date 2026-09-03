#pragma once

#include "flight/config.hpp"

#include <cstdint>

namespace flight {

// Per-tick inputs the mission state machine uses to detect launch, landing and faults.
struct DetectionInputs {
    bool self_test_ok = false;          // mandatory sensors produced valid data during self-test
    bool sensors_ok = false;            // mandatory sensors currently producing valid data
    bool armed = false;                 // arming delay elapsed + calibration settled: launch may be detected
    double accel_magnitude_mps2 = 0.0;  // |specific force|
    double altitude_agl_m = 0.0;        // altitude above the power-on ground baseline
    double altitude_rate_mps = 0.0;     // vertical speed estimate (+ up)
    bool critical_fault = false;        // a latched critical fault is active
};

// INIT -> SELF_TEST -> READY -> FLIGHT -> LANDED -> RECOVERY, with FAULT reachable from
// any operational state. Telemetry continues in every state, including FAULT.
class StateMachine {
public:
    explicit StateMachine(const Configuration& config);

    void begin_self_test(std::uint64_t now_ms);
    void update(std::uint64_t now_ms, const DetectionInputs& inputs);

    MissionState state() const { return state_; }
    std::uint64_t state_entered_ms() const { return entered_ms_; }

    // True while LANDED and still inside the mandatory post-impact transmission window.
    bool post_impact_window_active(std::uint64_t now_ms) const;

private:
    void enter(MissionState next, std::uint64_t now_ms);

    const Configuration& config_;
    MissionState state_ = MissionState::init;
    std::uint64_t entered_ms_ = 0;
    std::uint64_t launch_condition_since_ms_ = 0;
    std::uint64_t rest_condition_since_ms_ = 0;
};

}  // namespace flight
