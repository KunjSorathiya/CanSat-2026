#pragma once

#include "flight/config.hpp"

#include <cstdint>

namespace flight {

// Snapshot of vehicle health for local status use and optional downlink.
struct HealthSnapshot {
    MissionState state = MissionState::init;
    std::uint64_t mission_ms = 0;
    std::uint32_t packets_sent = 0;
    std::uint32_t packets_suppressed = 0;
    std::uint32_t packets_tx_failed = 0;
    std::uint32_t fault_total = 0;
    std::uint32_t fault_active = 0;
    bool imu_ok = false;
    bool baro_ok = false;
    bool orientation_ok = false;
    // Magnetometer state, in the order an operator needs it: is the part there at all,
    // is it currently producing usable samples, has it been calibrated for this airframe,
    // and is yaw therefore an absolute magnetic angle rather than a relative one.
    bool mag_present = false;
    bool mag_ok = false;
    bool mag_calibrated = false;
    bool yaw_is_magnetic = false;
    bool gps_fix = false;
    bool radio_ok = false;
    bool sd_ok = false;
    // Additional sensor. False also means "not fitted", which is a legitimate build of this
    // vehicle rather than a fault, so nothing mandatory keys off it.
    bool sound_ok = false;
    bool calibrated = false;         // startup calibration completed cleanly
    bool calibration_settled = false;// completed or timed out (best-effort applied)
    bool armed = false;              // launch detection is enabled
    bool watchdog_reboot = false;   // this power session began with a watchdog reset
    float battery_voltage = 0.0f;
    // False while `battery_divider_ratio` is unset: the value above is then the raw ADC
    // pin voltage, not the cell voltage. An operator reading "1.6 V" off a 3.7 V cell
    // needs to know which of the two they are looking at.
    bool battery_voltage_is_scaled = false;
    double altitude_agl_m = 0.0;      // above the power-on ground baseline
    double altitude_rate_mps = 0.0;   // filtered vertical speed, positive is climbing
    double gyro_bias_dps[3] = {0.0, 0.0, 0.0};
    double heading_deg = 0.0;      // magnetic heading, meaningless unless yaw_is_magnetic
    double mag_field_ut = 0.0;     // total measured field; the earth's is 25 to 65 uT
    // Per-axis sweep the runtime magnetometer calibration has seen so far. This is how an
    // operator on the pad knows whether the figure-of-eight has covered enough attitudes
    // for the calibration to be accepted.
    double mag_cal_span_ut[3] = {0.0, 0.0, 0.0};
    double ground_pressure_pa = 0.0;
    std::uint32_t gps_checksum_errors = 0;
};

const char* to_string(MissionState state);

}  // namespace flight
