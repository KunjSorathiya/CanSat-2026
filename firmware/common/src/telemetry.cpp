#include "cansat/telemetry.hpp"

#include <cmath>
#include <iomanip>
#include <regex>
#include <sstream>

namespace cansat {

bool TelemetryValidity::mandatory_valid() const {
    return altitude && pressure && temperature && roll && pitch && yaw &&
           acceleration_x && acceleration_y && acceleration_z;
}

namespace {

std::string number(double value, int precision) {
    std::ostringstream output;
    output << std::fixed << std::setprecision(precision) << value;
    return output.str();
}

bool finite(double value) { return std::isfinite(value); }

bool parse_number(const std::string& text, double& value) {
    try {
        std::size_t consumed = 0;
        value = std::stod(text, &consumed);
        return consumed == text.size() && finite(value);
    } catch (...) {
        return false;
    }
}

bool field_prefix(const std::string& field, const std::string& prefix, std::string& value) {
    if (field.rfind(prefix, 0) != 0) {
        return false;
    }
    value = field.substr(prefix.size());
    return !value.empty();
}

bool exact_precision(const std::string& value, int precision) {
    const std::string pattern = "-?[0-9]+\\\\.[0-9]{" + std::to_string(precision) + "}";
    return std::regex_match(value, std::regex(pattern));
}

}  // namespace

std::string format_timestamp(std::uint64_t timestamp_ms) {
    const auto milliseconds = timestamp_ms % 1000;
    const auto total_seconds = timestamp_ms / 1000;
    const auto seconds = total_seconds % 60;
    const auto total_minutes = total_seconds / 60;
    const auto minutes = total_minutes % 60;
    const auto hours = total_minutes / 60;

    std::ostringstream output;
    output << std::setfill('0') << std::setw(2) << hours << ':'
           << std::setw(2) << minutes << ':' << std::setw(2) << seconds << ':'
           << std::setw(3) << milliseconds;
    return output.str();
}

bool is_valid_team_id(const std::string& team_id) {
    static const std::regex pattern(R"(CAN-Team-[A-Za-z0-9]+)");
    return std::regex_match(team_id, pattern) && team_id != "CAN-Team-XX";
}

std::optional<std::string> format_packet(
    const TelemetryRecord& record,
    const std::vector<std::string>& optional_fields) {
    if (!is_valid_team_id(record.team_id) || record.packet_number == 0 ||
        !record.validity.mandatory_valid() ||
        !finite(record.altitude_m) || !finite(record.pressure_pa) ||
        !finite(record.temperature_c) || !finite(record.roll_deg) ||
        !finite(record.pitch_deg) || !finite(record.yaw_deg) ||
        !finite(record.acceleration_x_mps2) ||
        !finite(record.acceleration_y_mps2) ||
        !finite(record.acceleration_z_mps2)) {
        return std::nullopt;
    }

    const std::string packet = record.team_id + "; P-" +
        [&record]() {
            std::ostringstream number;
            number << std::setfill('0') << std::setw(3) << record.packet_number;
            return number.str();
        }() + "; Ti-" + format_timestamp(record.timestamp_ms) +
        "; A-" + number(record.altitude_m, 1) +
        "; Pr-" + number(record.pressure_pa, 2) +
        "; T-" + number(record.temperature_c, 1) +
        "; Ro-" + number(record.roll_deg, 1) +
        "; Pi-" + number(record.pitch_deg, 1) +
        "; Ya-" + number(record.yaw_deg, 1) +
        "; AX-" + number(record.acceleration_x_mps2, 2) +
        "; AY-" + number(record.acceleration_y_mps2, 2) +
        "; AZ-" + number(record.acceleration_z_mps2, 2) + ";";

    std::string result = packet;
    for (const auto& optional_field : optional_fields) {
        if (!optional_field.empty()) {
            result += " " + optional_field + ";";
        }
    }
    return result;
}

ParseResult parse_packet(const std::string& packet) {
    ParseResult result;
    std::vector<std::string> fields;
    std::stringstream stream(packet);
    std::string field;
    while (std::getline(stream, field, ';')) {
        while (!field.empty() && field.front() == ' ') field.erase(field.begin());
        while (!field.empty() && field.back() == ' ') field.pop_back();
        if (!field.empty()) fields.push_back(field);
    }

    if (fields.size() < 12) {
        result.error = "missing mandatory fields";
        return result;
    }
    if (!is_valid_team_id(fields[0])) {
        result.error = "invalid team identifier";
        return result;
    }

    TelemetryRecord record;
    record.team_id = fields[0];
    if (fields[1].rfind("P-", 0) != 0) {
        result.error = "invalid packet number prefix";
        return result;
    }
    try {
        record.packet_number = std::stoul(fields[1].substr(2));
    } catch (...) {
        result.error = "invalid packet number";
        return result;
    }

    std::string value;
    if (!field_prefix(fields[2], "Ti-", value)) {
        result.error = "invalid timestamp";
        return result;
    }
    unsigned hours = 0, minutes = 0, seconds = 0, milliseconds = 0;
    if (std::sscanf(value.c_str(), "%u:%u:%u:%u", &hours, &minutes, &seconds,
                    &milliseconds) != 4 || minutes > 59 || seconds > 59 ||
        milliseconds > 999) {
        result.error = "invalid timestamp";
        return result;
    }
    record.timestamp_ms = (((static_cast<std::uint64_t>(hours) * 60 + minutes) * 60 + seconds) * 1000) + milliseconds;

    const char* prefixes[] = {"A-", "Pr-", "T-", "Ro-", "Pi-", "Ya-", "AX-", "AY-", "AZ-"};
    const int precisions[] = {1, 2, 1, 1, 1, 1, 2, 2, 2};
    double* values[] = {&record.altitude_m, &record.pressure_pa, &record.temperature_c,
                        &record.roll_deg, &record.pitch_deg, &record.yaw_deg,
                        &record.acceleration_x_mps2, &record.acceleration_y_mps2,
                        &record.acceleration_z_mps2};
    bool* valid[] = {&record.validity.altitude, &record.validity.pressure,
                     &record.validity.temperature, &record.validity.roll,
                     &record.validity.pitch, &record.validity.yaw,
                     &record.validity.acceleration_x, &record.validity.acceleration_y,
                     &record.validity.acceleration_z};

    for (std::size_t index = 0; index < 9; ++index) {
        if (!field_prefix(fields[index + 3], prefixes[index], value) ||
            !exact_precision(value, precisions[index]) ||
            !parse_number(value, *values[index])) {
            result.error = "invalid mandatory numeric field";
            return result;
        }
        *valid[index] = true;
    }

    result.record = record;
    return result;
}

}  // namespace cansat
