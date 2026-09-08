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
    // Strictly faster than 1 Hz, not merely "at least" it. The rulebook's 1 packet per
    // second is a floor the vehicle must never be found below, and a period of exactly
    // 1000 ms is on the line rather than above it -- any jitter in the loop, the radio or
    // the receiver puts a measured interval past a second. The margin and the reasoning
    // are in cansat/link_profile.hpp; this is the runtime half of enforcing it, and it is
    // the one that catches a period set by hand at a call site rather than taken from the
    // profile.
    if (config.telemetry_period_ms == 0 ||
        config.telemetry_period_ms > cansat::link::kMaxTelemetryPeriodMs) {
        const double rate_hz = config.telemetry_period_ms == 0
                                   ? 0.0
                                   : 1000.0 / static_cast<double>(config.telemetry_period_ms);
        why = "telemetry_period_ms " + std::to_string(config.telemetry_period_ms) +
              " gives " + to_int_string(rate_hz * 100.0) +
              " centi-Hz; the rulebook minimum is 1 Hz and this vehicle must transmit "
              "strictly faster than it, so the period must be in 1.." +
              std::to_string(cansat::link::kMaxTelemetryPeriodMs) +
              " ms (see documentation/design/link-budget.md)";
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
    if (config.loop_tick_ms == 0) {
        why = "loop_tick_ms must be non-zero";
        return false;
    }
    // The GPS is drained once per loop tick from a hardware FIFO that keeps filling. A tick
    // longer than the FIFO takes to fill loses NMEA bytes before anything reads them, which
    // shows up as truncated sentences and checksum errors rather than as an obvious fault.
    // Half the fill time leaves margin for a late tick.
    const double fifo_ms = uart_fifo_fill_ms(config.gps_baud, config.gps_uart_fifo_bytes);
    if (fifo_ms > 0.0 && static_cast<double>(config.loop_tick_ms) > fifo_ms / 2.0) {
        why = "loop_tick_ms " + std::to_string(config.loop_tick_ms) +
              " is too slow to drain the GPS UART: its " +
              std::to_string(config.gps_uart_fifo_bytes) + "-byte FIFO fills in " +
              to_int_string(fifo_ms) + " ms at " + std::to_string(config.gps_baud) +
              " baud (see documentation/design/sensor-rates.md)";
        return false;
    }
    if (config.sensor_period_ms < config.loop_tick_ms) {
        why = "sensor_period_ms cannot be shorter than loop_tick_ms";
        return false;
    }
    // A NEO-6M at its default settings renews the fix once a second. A timeout shorter
    // than one navigation period would expire a perfectly live fix between updates and
    // drop GPS out of telemetry for a fault that does not exist.
    if (config.gps_fix_timeout_ms < 1000) {
        why = "gps_fix_timeout_ms must be >= 1000 (one NEO-6M navigation period)";
        return false;
    }
    if (config.gps_silence_after_ms < 1000) {
        why = "gps_silence_after_ms must be >= 1000 (one NEO-6M navigation period)";
        return false;
    }
    if (config.post_impact_transmission_ms < 5000) {
        why = "post_impact_transmission_ms must be >= 5000 (rulebook post-impact minimum)";
        return false;
    }
    // The descent gate and the at-rest test read the same quantity from opposite ends, so
    // their thresholds must not overlap. If the rate that counts as descending were at or
    // below the rate that counts as stopped, one sample could satisfy both -- which is
    // precisely the confusion the gate exists to remove.
    if (!(config.landing_descent_rate_mps > config.landing_altitude_rate_max_mps)) {
        why = "landing_descent_rate_mps must be greater than landing_altitude_rate_max_mps: "
              "the gate that says the vehicle descended and the test that says it stopped "
              "moving would otherwise both accept the same sample";
        return false;
    }
    if (config.landing_descent_confirm_ms == 0) {
        why = "landing_descent_confirm_ms must be non-zero: a single noisy barometer sample "
              "would otherwise open the descent gate and re-admit the hover that F-20 "
              "describes (documentation/mission/concept-of-operations.md)";
        return false;
    }
    if (!(config.reference_pressure_pa > 0.0)) {
        why = "reference_pressure_pa must be positive";
        return false;
    }
    if (!(config.orientation_kp_accel >= 0.0) || !(config.orientation_kp_mag >= 0.0) ||
        !(config.orientation_ki_bias >= 0.0)) {
        why = "orientation feedback gains must be non-negative";
        return false;
    }
    if (!(config.orientation_bias_limit_dps > 0.0)) {
        why = "orientation_bias_limit_dps must be positive";
        return false;
    }
    if (config.orientation_kp_accel == 0.0 && config.orientation_kp_mag == 0.0) {
        why = "orientation_kp_accel and orientation_kp_mag cannot both be zero: attitude "
              "would be a free-running gyro integration with no reference at all";
        return false;
    }

    // ---- Inertial sensor timing ---------------------------------------------
    // Same reasoning as the barometer below: reading the IMU faster than it produces
    // samples returns the previous conversion, and a repeated gyro sample integrates as
    // real motion rather than as the nothing it actually is.
    if (config.imu_gyro_dlpf_cfg > 7 || config.imu_accel_dlpf_cfg > 7) {
        why = "imu_gyro_dlpf_cfg and imu_accel_dlpf_cfg must be in 0..7 (3-bit fields)";
        return false;
    }
    const double imu_rate_hz =
        sensors::imu_sample_rate_hz(config.imu_gyro_dlpf_cfg, config.imu_sample_rate_div);
    const double acquisition_hz = 1000.0 / static_cast<double>(config.sensor_period_ms);
    if (imu_rate_hz < acquisition_hz) {
        why = "the IMU's internal sample rate (" + to_int_string(imu_rate_hz) +
              " Hz at the configured DLPF and SMPLRT_DIV) is below the " +
              to_int_string(acquisition_hz) + " Hz acquisition rate";
        return false;
    }
    // The anti-alias bandwidth is deliberately NOT rejected here. DLPF 4 puts the gyro
    // at 20 Hz and the accelerometer at 21.2 Hz against a 30 Hz acquisition, which is
    // above the 15 Hz Nyquist limit: content between 15 and 21 Hz folds back into the
    // attitude estimate. That is a known, documented trade -- the next filter down is
    // 10 Hz, which blurs the launch transient the state machine detects on -- and it is
    // recorded in documentation/design/sensor-rates.md rather than enforced as a rule,
    // because the right setting depends on how much the airframe actually vibrates and
    // that is measured on a shake table, not asserted in software.

    // ---- Magnetometer -------------------------------------------------------
    const double mag_rate_hz = sensors::mag_output_rate_hz(config.mag_mode);
    if (mag_rate_hz > 0.0 && mag_rate_hz < acquisition_hz) {
        why = "the magnetometer's continuous mode runs at " + to_int_string(mag_rate_hz) +
              " Hz, below the " + to_int_string(acquisition_hz) +
              " Hz acquisition rate; use continuous mode 2 (100 Hz)";
        return false;
    }
    if (!(config.mag_cal_min_span_ut > 0.0)) {
        why = "mag_cal_min_span_ut must be positive";
        return false;
    }
    if (config.mag_cal_in_flight && config.mag_cal_min_samples == 0) {
        why = "mag_cal_min_samples must be non-zero when mag_cal_in_flight is set";
        return false;
    }
    if (!(config.yaw_cog_tolerance_deg > 0.0) || !(config.yaw_cog_tolerance_deg <= 180.0)) {
        why = "yaw_cog_tolerance_deg must be in (0, 180]";
        return false;
    }
    if (!(config.yaw_cog_min_speed_mps >= 0.0)) {
        why = "yaw_cog_min_speed_mps must be non-negative";
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
    // transmit_gps and the budget have to move together. Left apart, the vehicle would
    // build packets 56 bytes longer than the airtime budget assumes -- so the duty check
    // below would pass on a number the radio never sends, and the controller would quietly
    // drop MODE/FAULTS/CAL/ARM to squeeze each packet back under a cap that was set for a
    // configuration this no longer is. Diagnostics would vanish exactly when a flight got
    // interesting, and nothing would say why.
    if (config.transmit_gps &&
        config.worst_case_packet_bytes < cansat::link::kWorstCasePacketBytesWithGps) {
        why = "transmit_gps is set but worst_case_packet_bytes is " +
              std::to_string(config.worst_case_packet_bytes) +
              "; the GPS fields take the longest packet to " +
              std::to_string(cansat::link::kWorstCasePacketBytesWithGps) +
              " bytes, so the budget must be at least that (and the telemetry period "
              "re-derived from it)";
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
