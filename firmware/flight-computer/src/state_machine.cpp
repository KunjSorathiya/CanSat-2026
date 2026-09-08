#include "flight/state_machine.hpp"

#include "flight/sensor_math.hpp"

#include <cmath>

namespace flight {

StateMachine::StateMachine(const Configuration& config) : config_(config) {}

void StateMachine::enter(MissionState next, std::uint64_t now_ms) {
    if (next == state_) {
        return;
    }
    state_ = next;
    entered_ms_ = now_ms;
    launch_condition_since_ms_ = 0;
    rest_condition_since_ms_ = 0;
    // The descent gate belongs to one FLIGHT, not to the vehicle. Clearing it here means
    // a descent seen before this state began cannot authorise a landing inside it.
    descent_since_ms_ = 0;
    descent_observed_ = false;
}

void StateMachine::begin_self_test(std::uint64_t now_ms) {
    if (state_ == MissionState::init) {
        enter(MissionState::self_test, now_ms);
    }
}

void StateMachine::update(std::uint64_t now_ms, const DetectionInputs& in) {
    // A latched critical fault forces FAULT from any operational state. Telemetry keeps
    // running; the controller does not stop emitting in FAULT.
    if (in.critical_fault && state_ != MissionState::fault) {
        enter(MissionState::fault, now_ms);
        return;
    }

    switch (state_) {
        case MissionState::init:
            enter(MissionState::self_test, now_ms);
            break;

        case MissionState::self_test:
            enter(in.self_test_ok ? MissionState::ready : MissionState::fault, now_ms);
            break;

        case MissionState::ready: {
            if (!in.armed) {
                launch_condition_since_ms_ = 0;  // ignore any motion until armed
                break;
            }
            const bool boost = in.accel_magnitude_mps2 > config_.launch_accel_mps2;
            const bool climb = in.altitude_agl_m > config_.launch_altitude_gain_m;
            if (boost || climb) {
                if (launch_condition_since_ms_ == 0) {
                    launch_condition_since_ms_ = now_ms;
                }
                if (now_ms - launch_condition_since_ms_ >= config_.launch_confirm_ms) {
                    enter(MissionState::flight, now_ms);
                }
            } else {
                launch_condition_since_ms_ = 0;
            }
            break;
        }

        case MissionState::flight: {
            // ---- The descent gate (F-20) ------------------------------------------
            // A vehicle cannot land without descending first. This runs before the
            // min_flight_ms guard so the gate keeps arming during that window; a descent
            // that begins early still counts.
            //
            // It exists because "at rest" — about 1 g, and no vertical motion — is also a
            // perfect description of a vehicle **hovering under the drone that is about to
            // release it**. Held for landing_confirm_ms, that was a landing: measured at
            // 18.0 s on a 3 m/s lift with a 20 s hover, twelve seconds before release,
            // after which the whole real descent happened in RECOVERY and the mandatory
            // post-impact window had already been spent in the air.
            //
            // min_flight_ms does not cover it. That guard is aimed at a boost-then-coast
            // profile where the vehicle is genuinely moving; on a drone lift FLIGHT is
            // entered at launch_altitude_gain_m *during the ascent*, so its three seconds
            // are gone long before the hover starts.
            //
            // The gate is physical rather than threshold-tuned, which is the point: no
            // hover, however long or however gentle the lift, can satisfy it. The rate
            // threshold sits well below the mission's 5 m/s descent cap and well above
            // anything a hovering drone or barometric noise produces.
            if (in.altitude_rate_mps < -config_.landing_descent_rate_mps) {
                if (descent_since_ms_ == 0) {
                    descent_since_ms_ = now_ms;
                }
                if (now_ms - descent_since_ms_ >= config_.landing_descent_confirm_ms) {
                    descent_observed_ = true;
                }
            } else {
                descent_since_ms_ = 0;
            }

            if (now_ms - entered_ms_ < config_.min_flight_ms) {
                break;
            }
            // A vehicle descending under a parachute at a steady rate has no net
            // acceleration: the accelerometer reads about 1 g, exactly as it does on the
            // ground. This test alone would declare a landing seconds after the parachute
            // opened, so the vertical rate below is the real discriminator — see
            // documentation/design/software-architecture.md.
            const double accel_error =
                std::fabs(in.accel_magnitude_mps2 - sensors::kStandardGravity);
            const bool at_rest = accel_error < config_.landing_accel_epsilon_mps2 &&
                                 std::fabs(in.altitude_rate_mps) < config_.landing_altitude_rate_max_mps;
            // Being at rest is necessary and no longer sufficient. Without an observed
            // descent the rest timer does not even start, so a hover cannot accumulate
            // towards a landing and then fire the moment the gate happens to open.
            if (at_rest && descent_observed_) {
                if (rest_condition_since_ms_ == 0) {
                    rest_condition_since_ms_ = now_ms;
                }
                if (now_ms - rest_condition_since_ms_ >= config_.landing_confirm_ms) {
                    enter(MissionState::landed, now_ms);
                }
            } else {
                rest_condition_since_ms_ = 0;
            }
            break;
        }

        case MissionState::landed:
            if (now_ms - entered_ms_ >= config_.post_impact_transmission_ms) {
                enter(MissionState::recovery, now_ms);
            }
            break;

        case MissionState::recovery:
        case MissionState::fault:
            // Terminal for state purposes; telemetry continues.
            break;
    }
}

bool StateMachine::post_impact_window_active(std::uint64_t now_ms) const {
    return state_ == MissionState::landed &&
           (now_ms - entered_ms_) < config_.post_impact_transmission_ms;
}

}  // namespace flight
