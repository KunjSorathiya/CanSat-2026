#include "flight/telemetry_builder.hpp"

#include "flight/health.hpp"

#include <cmath>
#include <cstdio>
#include <utility>
#include <vector>

namespace flight {

namespace {

// snprintf rather than <sstream>: this is the flight image, and iostreams bring locale
// machinery and a static initialiser for what is only ever "%.*f".
std::string fixed(double value, int decimals) {
    char buffer[64];
    const int written = std::snprintf(buffer, sizeof(buffer), "%.*f", decimals, value);
    if (written <= 0) return std::string();
    const std::size_t length = static_cast<std::size_t>(written) < sizeof(buffer)
                                   ? static_cast<std::size_t>(written)
                                   : sizeof(buffer) - 1;
    return std::string(buffer, length);
}

std::string optional_field(const char* prefix, double value, int decimals) {
    return std::string(prefix) + fixed(value, decimals);
}

}  // namespace

TelemetryBuilder::TelemetryBuilder(Configuration config) : config_(std::move(config)) {}

std::optional<TelemetryBuilder::Built> TelemetryBuilder::build(
    std::uint32_t packet_number, std::uint64_t mission_ms,
    const SensorSnapshot& s, const std::vector<std::string>& extra_optional) const {
    cansat::TelemetryRecord record;
    record.team_id = config_.team_id;
    record.packet_number = packet_number;
    record.timestamp_ms = mission_ms;

    record.altitude_m = s.altitude_m;
    record.pressure_pa = s.pressure_pa;
    record.temperature_c = s.temperature_c;
    record.roll_deg = s.roll_deg;
    record.pitch_deg = s.pitch_deg;
    record.yaw_deg = s.yaw_deg;
    record.acceleration_x_mps2 = s.ax_mps2;
    record.acceleration_y_mps2 = s.ay_mps2;
    record.acceleration_z_mps2 = s.az_mps2;

    record.validity.altitude = s.baro_valid;
    record.validity.pressure = s.baro_valid;
    record.validity.temperature = s.baro_valid;
    record.validity.roll = s.orientation_valid;
    record.validity.pitch = s.orientation_valid;
    record.validity.yaw = s.orientation_valid;
    record.validity.acceleration_x = s.imu_valid;
    record.validity.acceleration_y = s.imu_valid;
    record.validity.acceleration_z = s.imu_valid;

    std::vector<std::string> optional;
    if (s.gps.valid && std::isfinite(s.gps.latitude) && std::isfinite(s.gps.longitude) &&
        std::isfinite(s.gps.altitude)) {
        // The record always carries the fix: it is what the SD row is rendered from, and
        // SEN-011 is satisfied by data that is "transmitted or logged". Whether it also
        // goes on the air is a separate decision, because the three GP- fields are 56 of
        // the packet's 255 bytes and the whole telemetry rate is computed from the worst
        // case. See Configuration::transmit_gps.
        record.gps = s.gps;
        if (config_.transmit_gps) {
            optional.push_back(optional_field("GP-Lat-", s.gps.latitude, config_.gps_latlon_decimals));
            optional.push_back(optional_field("GP-Lon-", s.gps.longitude, config_.gps_latlon_decimals));
            optional.push_back(optional_field("GP-Alt-", s.gps.altitude, config_.gps_alt_decimals));
        }
    }
    for (const auto& extra : extra_optional) {
        if (!extra.empty()) {
            optional.push_back(extra);
        }
    }

    auto packet = cansat::format_packet(record, optional);
    if (!packet) {
        return std::nullopt;
    }
    Built built{record, *packet, 0.0, false, false, 0.0, false};
    built.sound_mv_pp = s.sound_mv_pp;
    built.sound_clipped = s.sound_clipped;
    built.sound_valid = s.sound_valid;
    built.sound_gate_pct = s.sound_gate_pct;
    built.sound_gate_valid = s.sound_gate_valid;
    return built;
}

std::string TelemetryBuilder::sd_header() {
    // sound_mv_pp and sound_clipped sit before `packet` so the packet string stays the last
    // column: it contains no commas but it is by far the widest field, and a reader opening
    // the CSV wants the numbers before it.
    return "mission_ms,packet_number,state,fault_total,altitude_m,pressure_pa,temperature_c,"
           "roll_deg,pitch_deg,yaw_deg,ax_mps2,ay_mps2,az_mps2,gps_valid,gps_lat,gps_lon,"
           "gps_alt,gps_satellites,gps_hdop,sound_mv_pp,sound_clipped,sound_gate_pct,packet";
}

std::string TelemetryBuilder::sd_line(const Built& b, MissionState state,
                                      std::uint32_t fault_total) const {
    const auto& r = b.record;
    std::string out;
    out.reserve(384);  // typical row; avoids repeated reallocation on the logging path

    out += std::to_string(r.timestamp_ms);
    out += ',';
    out += std::to_string(r.packet_number);
    out += ',';
    out += to_string(state);
    out += ',';
    out += std::to_string(fault_total);
    for (const std::string& field : {fixed(r.altitude_m, 1),
                                     fixed(r.pressure_pa, 2),
                                     fixed(r.temperature_c, 1),
                                     fixed(r.roll_deg, 1),
                                     fixed(r.pitch_deg, 1),
                                     fixed(r.yaw_deg, 1),
                                     fixed(r.acceleration_x_mps2, 2),
                                     fixed(r.acceleration_y_mps2, 2),
                                     fixed(r.acceleration_z_mps2, 2)}) {
        out += ',';
        out += field;
    }
    out += ',';
    out += (r.gps ? '1' : '0');
    out += ',';
    if (r.gps) {
        out += fixed(r.gps->latitude, config_.gps_latlon_decimals);
        out += ',';
        out += fixed(r.gps->longitude, config_.gps_latlon_decimals);
        out += ',';
        out += fixed(r.gps->altitude, config_.gps_alt_decimals);
        out += ',';
        // The two numbers the fix gate judged on, recorded beside the position it let
        // through. [F-18] could not be diagnosed from a log because these were computed,
        // acted on, and then discarded.
        out += std::to_string(static_cast<unsigned>(r.gps->satellites));
        out += ',';
        out += fixed(r.gps->hdop, 1);
    } else {
        // No fix means no quality either, and both blanks are load-bearing: 0 satellites
        // is a reading a receiver produces, and HDOP 0.0 is the best geometry there is.
        // Written as zeros they would describe a perfect fix that never happened.
        out += ",,,,";  // gps_lat, gps_lon, gps_alt, gps_satellites, gps_hdop
    }
    // An absent or unfitted microphone leaves both columns empty rather than writing a
    // zero. Zero is a level a working sensor can report -- silence -- and a column that
    // cannot distinguish "silent" from "not measured" is worse than a blank one.
    out += ',';
    if (b.sound_valid) out += fixed(b.sound_mv_pp, 1);
    out += ',';
    if (b.sound_valid) out += (b.sound_clipped ? '1' : '0');
    out += ',';
    if (b.sound_gate_valid) out += fixed(b.sound_gate_pct, 1);
    out += ',';
    out += b.packet;
    return out;
}

}  // namespace flight
