#include "flight/config.hpp"

#include "cansat/lora_airtime.hpp"
#include "cansat/telemetry.hpp"
#include "flight/sensor_timing.hpp"

#include <cmath>
#include <string>

namespace flight {
namespace {

// Small integer-only formatter. Avoids <sstream> and its allocation/flash cost on the
// vehicle; these strings are only ever built once, on a rejected configuration.
std::string to_int_string(double value) {
    long rounded = static_cast<long>(value + 0.5);
    if (rounded < 0) rounded = 0;
    return std::to_string(rounded);
}

}  // namespace

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
    // Sampling faster than the barometer converts re-reads the previous conversion. The
    // altitude-rate estimator differentiates altitude, so duplicated samples read as zero
    // climb rate — worst exactly when the vehicle is moving fastest.
    const double baro_min_period =
        sensors::baro_min_sample_period_ms(config.baro_osrs_t, config.baro_osrs_p,
                                           config.baro_standby_ms);
    if (static_cast<double>(config.sensor_period_ms) < baro_min_period) {
        why = "sensor_period_ms " + std::to_string(config.sensor_period_ms) +
              " is shorter than the barometer's worst-case conversion time (" +
              to_int_string(baro_min_period) +
              " ms at the configured oversampling); reduce the oversampling or slow the "
              "acquisition rate (see documentation/design/sensor-rates.md)";
        return false;
    }
    if (!(config.baro_standby_ms >= 0.0)) {
        why = "baro_standby_ms must be non-negative";
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

    // ---- Radio airtime ------------------------------------------------------
    // A telemetry period the radio physically cannot keep up with produces a link that
    // silently under-runs its schedule. Catch it on the pad, not in flight.
    const auto& radio = config.radio;
    if (radio.spreading_factor < 6 || radio.spreading_factor > 12) {
        why = "radio.spreading_factor must be in 6..12";
        return false;
    }
    if (radio.coding_rate < 5 || radio.coding_rate > 8) {
        why = "radio.coding_rate must be in 5..8 (denominator of 4/5..4/8)";
        return false;
    }
    if (radio.bandwidth_hz == 0) {
        why = "radio.bandwidth_hz must be non-zero";
        return false;
    }
    if (radio.preamble_length < 6) {
        why = "radio.preamble_length must be >= 6 symbols (SX127x minimum)";
        return false;
    }
    if (config.worst_case_packet_bytes == 0 ||
        config.worst_case_packet_bytes > cansat::kMaxLoraPayloadBytes) {
        why = "worst_case_packet_bytes must be in 1..255 (LoRa FIFO limit)";
        return false;
    }
    if (!(config.max_channel_duty > 0.0) || !(config.max_channel_duty <= 1.0)) {
        why = "max_channel_duty must be in (0, 1]";
        return false;
    }

    const double airtime_ms = worst_case_airtime_ms(config);
    if (!std::isfinite(airtime_ms) || airtime_ms <= 0.0) {
        why = "radio parameters produce a non-physical packet airtime";
        return false;
    }
    if (channel_duty(config) > config.max_channel_duty) {
        const double min_period = airtime_ms / config.max_channel_duty;
        why = "telemetry_period_ms " + std::to_string(config.telemetry_period_ms) +
              " is shorter than the radio can sustain: a " +
              std::to_string(config.worst_case_packet_bytes) + "-byte packet at SF" +
              std::to_string(static_cast<int>(radio.spreading_factor)) + "/" +
              std::to_string(radio.bandwidth_hz / 1000) + "kHz takes " +
              to_int_string(airtime_ms) + " ms of airtime; need >= " +
              to_int_string(min_period) + " ms at the configured duty limit " +
              to_int_string(config.max_channel_duty * 100.0) +
              "% (see documentation/design/link-budget.md)";
        return false;
    }

    why.clear();
    return true;
}

}  // namespace flight
