#include "cansat/telemetry.hpp"

#include <cmath>
#include <cstddef>
#include <iomanip>
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

bool digit(char c) { return c >= '0' && c <= '9'; }

// Strict unsigned parse of a packet number: digits only, no sign, no whitespace, no
// overflow. std::stoul would accept "-1" (wrapping to 4294967295), " 7" and "7abc"; the
// Python and JavaScript ground-station parsers accept none of those, and a parser that
// disagrees with its own transmitter about what a packet number is cannot be trusted to
// count packet loss.
bool parse_packet_number(const std::string& text, std::uint32_t& value) {
    if (text.empty() || text.size() > 10) return false;
    std::uint64_t accumulated = 0;
    for (const char c : text) {
        if (!digit(c)) return false;
        accumulated = accumulated * 10 + static_cast<std::uint64_t>(c - '0');
        if (accumulated > 0xFFFFFFFFull) return false;
    }
    // The rulebook numbers packets from P-001; the formatter refuses to emit zero, so the
    // parser must refuse to accept it.
    if (accumulated == 0) return false;
    value = static_cast<std::uint32_t>(accumulated);
    return true;
}

// Equivalent to the anchored regex "-?[0-9]+\.[0-9]{precision}", written by hand so the
// shared telemetry library carries no <regex> dependency. On the Pico that removes a
// large amount of flash and the per-call regex construction cost; this runs nine times
// for every parsed packet.
bool exact_precision(const std::string& value, int precision) {
    std::size_t i = 0;
    if (i < value.size() && value[i] == '-') ++i;

    const std::size_t integer_start = i;
    while (i < value.size() && digit(value[i])) ++i;
    if (i == integer_start) return false;  // no integer digits

    if (i >= value.size() || value[i] != '.') return false;
    ++i;

    const std::size_t fraction_start = i;
    while (i < value.size() && digit(value[i])) ++i;
    if (i != value.size()) return false;  // trailing characters after the fraction
    return static_cast<int>(i - fraction_start) == precision;
}

// "HH:MM:SS:MS": exactly two/two/two/three digits separated by colons.
bool timestamp_shape(const std::string& value) {
    if (value.size() != 12) return false;
    if (value[2] != ':' || value[5] != ':' || value[8] != ':') return false;
    for (std::size_t i = 0; i < value.size(); ++i) {
        if (i == 2 || i == 5 || i == 8) continue;
        if (!digit(value[i])) return false;
    }
    return true;
}

unsigned digits_to_uint(const std::string& value, std::size_t offset, std::size_t count) {
    unsigned out = 0;
    for (std::size_t i = 0; i < count; ++i) {
        out = out * 10 + static_cast<unsigned>(value[offset + i] - '0');
    }
    return out;
}

}  // namespace

std::string format_timestamp(std::uint64_t timestamp_ms) {
    const auto milliseconds = timestamp_ms % 1000;
    const auto total_seconds = timestamp_ms / 1000;
    const auto seconds = total_seconds % 60;
    const auto total_minutes = total_seconds / 60;
    const auto minutes = total_minutes % 60;
    // The rulebook format is Ti-HH:MM:SS:MS with a two-digit hour field, and the parser
    // enforces exactly that. Hours are therefore taken modulo 100: past 99:59:59:999 —
    // about 4.2 days of continuous uptime — the field wraps rather than widening to three
    // digits, which would produce a packet this library's own parser rejects. A mission
    // lasts minutes; only a bench rig left powered can reach the wrap.
    const auto hours = (total_minutes / 60) % 100;

    std::ostringstream output;
    output << std::setfill('0') << std::setw(2) << hours << ':'
           << std::setw(2) << minutes << ':' << std::setw(2) << seconds << ':'
           << std::setw(3) << milliseconds;
    return output.str();
}

bool is_valid_team_id(const std::string& team_id) {
    // "CAN-Team-" followed by at least one alphanumeric character, and never the
    // "CAN-Team-XX" placeholder from the rulebook example.
    static const char kPrefix[] = "CAN-Team-";
    const std::size_t prefix_length = sizeof(kPrefix) - 1;
    if (team_id.size() <= prefix_length) return false;
    if (team_id.compare(0, prefix_length, kPrefix) != 0) return false;
    for (std::size_t i = prefix_length; i < team_id.size(); ++i) {
        const char c = team_id[i];
        const bool alphanumeric = (c >= '0' && c <= '9') || (c >= 'A' && c <= 'Z') ||
                                  (c >= 'a' && c <= 'z');
        if (!alphanumeric) return false;
    }
    return team_id != "CAN-Team-XX";
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
    if (!parse_packet_number(fields[1].substr(2), record.packet_number)) {
        result.error = "invalid packet number";
        return result;
    }

    std::string value;
    if (!field_prefix(fields[2], "Ti-", value)) {
        result.error = "invalid timestamp";
        return result;
    }
    if (!timestamp_shape(value)) {
        result.error = "invalid timestamp";
        return result;
    }
    const unsigned hours = digits_to_uint(value, 0, 2);
    const unsigned minutes = digits_to_uint(value, 3, 2);
    const unsigned seconds = digits_to_uint(value, 6, 2);
    const unsigned milliseconds = digits_to_uint(value, 9, 3);
    if (minutes > 59 || seconds > 59) {
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
