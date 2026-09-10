#pragma once

#include "cansat/telemetry.hpp"
#include "flight/config.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace flight {

// Engineering-unit view of the vehicle at one instant, in the project body frame.
struct SensorSnapshot {
    double ax_mps2 = 0.0;
    double ay_mps2 = 0.0;
    double az_mps2 = 0.0;
    // Calibrated magnetic flux density, microtesla, body frame.
    double mx_ut = 0.0;
    double my_ut = 0.0;
    double mz_ut = 0.0;
    double roll_deg = 0.0;
    double pitch_deg = 0.0;
    // Z-Y-X Euler yaw, right-handed about the body up axis. Absolute magnetic yaw only
    // while `yaw_is_magnetic`; otherwise a relative, free-running gyro integration.
    double yaw_deg = 0.0;
    // The same angle as a compass bearing, 0..360 clockwise from magnetic north.
    double heading_deg = 0.0;
    double altitude_m = 0.0;
    double pressure_pa = 0.0;
    double temperature_c = 0.0;
    bool imu_valid = false;
    bool mag_valid = false;
    bool orientation_valid = false;
    bool yaw_is_magnetic = false;
    bool baro_valid = false;
    // Analogue microphone, peak-to-peak millivolts over the last window. A relative level,
    // not a sound pressure level -- see flight/sound_level.hpp. Logged, never transmitted.
    double sound_mv_pp = 0.0;
    bool sound_clipped = false;
    bool sound_valid = false;
    // Percentage of the same window above the module's comparator threshold. The only
    // channel a three-pin LM393 board can provide, and an independent one on a
    // four-pin board.
    double sound_gate_pct = 0.0;
    bool sound_gate_valid = false;
    cansat::GpsData gps{};  // gps.valid == has fix
};

// Assembles the canonical telemetry record + rulebook packet string. Optional GPS fields
// are appended only after all mandatory fields, and only when a fix is present.
class TelemetryBuilder {
public:
    explicit TelemetryBuilder(Configuration config);

    struct Built {
        cansat::TelemetryRecord record;
        std::string packet;
        // Additional-sensor data that is logged but never transmitted. It is carried here
        // rather than in `record` on purpose: TelemetryRecord is the shared contract with
        // the ground station and describes what goes over the air, and putting a field in
        // it that is never sent would misrepresent the packet to every reader of that
        // header. The SD log is a wider record than the link, and this is where the two
        // differ.
        double sound_mv_pp = 0.0;
        bool sound_clipped = false;
        bool sound_valid = false;
        double sound_gate_pct = 0.0;
        bool sound_gate_valid = false;
    };

    // Returns nullopt when mandatory data is invalid -> no telemetry point is produced.
    // `extra_optional` fields are appended after the mandatory block and any GPS fields.
    std::optional<Built> build(std::uint32_t packet_number,
                               std::uint64_t mission_ms,
                               const SensorSnapshot& snapshot,
                               const std::vector<std::string>& extra_optional = {}) const;

    // One CSV row for the onboard SD log (superset of the packet + local metadata).
    static std::string sd_header();
    std::string sd_line(const Built& built, MissionState state,
                        std::uint32_t fault_total) const;

private:
    Configuration config_;

  public:
    // The packet shape is fixed at construction except for this: a ground command can put
    // the position on the air for the rest of the power cycle. The builder holds its own
    // COPY of the configuration, so the controller changing its own copy would never reach
    // here -- which is exactly the kind of half-applied change that looks right in a review
    // and transmits the old packet on the bench.
    void set_transmit_gps(bool on) { config_.transmit_gps = on; }

  private:
};

}  // namespace flight
