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
    bool gps_fix = false;
    bool radio_ok = false;
    bool sd_ok = false;
    bool calibrated = false;         // startup calibration completed cleanly
    bool calibration_settled = false;// completed or timed out (best-effort applied)
    bool armed = false;              // launch detection is enabled
    bool watchdog_reboot = false;   // this power session began with a watchdog reset
    float battery_voltage = 0.0f;
    double gyro_bias_dps[3] = {0.0, 0.0, 0.0};
    double ground_pressure_pa = 0.0;
    std::uint32_t gps_checksum_errors = 0;
};

const char* to_string(MissionState state);

}  // namespace flight
