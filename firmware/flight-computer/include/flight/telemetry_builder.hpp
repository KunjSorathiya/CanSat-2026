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
    double roll_deg = 0.0;
    double pitch_deg = 0.0;
    double yaw_deg = 0.0;
    double altitude_m = 0.0;
    double pressure_pa = 0.0;
    double temperature_c = 0.0;
    bool imu_valid = false;
    bool orientation_valid = false;
    bool baro_valid = false;
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
};

}  // namespace flight
