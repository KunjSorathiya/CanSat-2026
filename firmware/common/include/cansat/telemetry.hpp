#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

namespace cansat {

struct TelemetryValidity {
    bool altitude = false;
    bool pressure = false;
    bool temperature = false;
    bool roll = false;
    bool pitch = false;
    bool yaw = false;
    bool acceleration_x = false;
    bool acceleration_y = false;
    bool acceleration_z = false;

    bool mandatory_valid() const;
};

struct GpsData {
    double latitude = 0.0;
    double longitude = 0.0;
    double altitude = 0.0;
    bool valid = false;               // has a position fix
    double time_of_day_s = 0.0;       // UTC seconds since midnight, when time_valid
    bool time_valid = false;
    std::uint8_t satellites = 0;
    // Ground track, from RMC. Speed over ground in m/s, and course over ground in
    // degrees clockwise from TRUE north.
    //
    // Course over ground is not the vehicle's yaw and must never be used as though it
    // were: it describes where the ground track is going, not where the body is pointing.
    // A payload descending under a parachute in wind is crabbing, so the two differ by
    // the drift angle; a payload that is spinning has a body yaw that sweeps through 360
    // degrees while its course stays constant; and a stationary receiver reports a course
    // that is pure noise. It is carried here for cross-checking and for the ground track,
    // nothing else.
    double speed_mps = 0.0;
    double course_deg = 0.0;
    bool course_valid = false;
};

struct TelemetryRecord {
    std::string team_id = "CAN-Team-XX";
    std::uint32_t packet_number = 0;
    std::uint64_t timestamp_ms = 0;
    double altitude_m = 0.0;
    double pressure_pa = 0.0;
    double temperature_c = 0.0;
    double roll_deg = 0.0;
    double pitch_deg = 0.0;
    double yaw_deg = 0.0;
    double acceleration_x_mps2 = 0.0;
    double acceleration_y_mps2 = 0.0;
    double acceleration_z_mps2 = 0.0;
    TelemetryValidity validity;
    std::optional<GpsData> gps;
};

struct ParseResult {
    std::optional<TelemetryRecord> record;
    std::string error;

    explicit operator bool() const { return record.has_value(); }
};

std::string format_timestamp(std::uint64_t timestamp_ms);
std::optional<std::string> format_packet(
    const TelemetryRecord& record,
    const std::vector<std::string>& optional_fields = {});
ParseResult parse_packet(const std::string& packet);
bool is_valid_team_id(const std::string& team_id);

}  // namespace cansat
