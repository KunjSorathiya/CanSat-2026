#include "flight/config.hpp"

#include "cansat/telemetry.hpp"

namespace flight {

bool validate_config(const Configuration& config, std::string& why) {
    if (!cansat::is_valid_team_id(config.team_id)) {
        why = "team_id must be a registered CAN-Team-<id> (not the CAN-Team-XX placeholder)";
        return false;
    }
    if (config.telemetry_period_ms == 0 || config.telemetry_period_ms > 1000) {
        why = "telemetry_period_ms must be in 1..1000 (>= 1 Hz rulebook minimum)";
        return false;
    }
    if (config.sensor_period_ms == 0) {
        why = "sensor_period_ms must be non-zero";
        return false;
    }
    if (config.post_impact_transmission_ms < 5000) {
        why = "post_impact_transmission_ms must be >= 5000 (rulebook post-impact minimum)";
        return false;
    }
    if (!(config.reference_pressure_pa > 0.0)) {
        why = "reference_pressure_pa must be positive";
        return false;
    }
    if (!(config.orientation_alpha >= 0.0) || !(config.orientation_alpha <= 1.0)) {
        why = "orientation_alpha must be in [0, 1]";
        return false;
    }
    why.clear();
    return true;
}

}  // namespace flight
