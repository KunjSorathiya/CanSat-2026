#include "cansat/command.hpp"

#include <cstddef>
#include <cstdio>
#include <vector>

namespace cansat {
namespace {

const char* name_of(CommandKind kind) {
    switch (kind) {
        case CommandKind::erase_log: return "ERASE_LOG";
        case CommandKind::max_rate_gps: return "MAX_RATE_GPS";
        case CommandKind::max_rate_lean: return "MAX_RATE_LEAN";
        case CommandKind::none: break;
    }
    return "";
}

// The inverse, and deliberately exhaustive rather than a prefix match: MAX_RATE_LEANER is
// not MAX_RATE_LEAN, and a command this build does not know must read as none rather than
// as the nearest thing it recognises.
CommandKind kind_of(const std::string& name) {
    if (name == "ERASE_LOG") return CommandKind::erase_log;
    if (name == "MAX_RATE_GPS") return CommandKind::max_rate_gps;
    if (name == "MAX_RATE_LEAN") return CommandKind::max_rate_lean;
    return CommandKind::none;
}

// Splits on ';' and trims spaces, the same shape the telemetry parser reads.
std::vector<std::string> split_fields(const std::string& text) {
    std::vector<std::string> out;
    std::string current;
    for (const char c : text) {
        if (c == ';') {
            out.push_back(current);
            current.clear();
        } else {
            current += c;
        }
    }
    if (!current.empty()) out.push_back(current);
    for (std::string& field : out) {
        std::size_t begin = field.find_first_not_of(" \t\r\n");
        std::size_t end = field.find_last_not_of(" \t\r\n");
        field = (begin == std::string::npos) ? std::string() : field.substr(begin, end - begin + 1);
    }
    return out;
}

// Constant-time-ish comparison. Not because 64 bits of FNV deserve it, but because a
// length-and-first-difference exit is a habit worth not forming in a file whose whole job
// is deciding whether to obey a stranger.
bool tokens_match(const std::string& a, const std::string& b) {
    if (a.size() != b.size()) return false;
    unsigned char diff = 0;
    for (std::size_t i = 0; i < a.size(); ++i) {
        diff = static_cast<unsigned char>(diff | (a[i] ^ b[i]));
    }
    return diff == 0;
}

}  // namespace

std::string command_token(const std::string& password, CommandKind kind,
                          std::uint32_t packet_number) {
    if (password.empty() || kind == CommandKind::none) return {};
    std::string material = password;
    material += '|';
    material += name_of(kind);
    material += '|';
    material += std::to_string(packet_number);

    // FNV-1a, 64-bit. Chosen because it is four lines in both languages and therefore
    // cannot drift apart in a way a fixture would not catch -- not because it is strong.
    std::uint64_t hash = 14695981039346656037ULL;
    for (const char c : material) {
        hash ^= static_cast<std::uint8_t>(c);
        hash *= 1099511628211ULL;
    }
    char out[17];
    std::snprintf(out, sizeof(out), "%016llx", static_cast<unsigned long long>(hash));
    return std::string(out);
}

std::string format_command(const std::string& team_id, CommandKind kind,
                           const std::string& password, std::uint32_t packet_number) {
    if (kind == CommandKind::none || password.empty()) return {};
    return team_id + "; CMD-" + name_of(kind) + "; PN-" + std::to_string(packet_number) +
           "; KEY-" + command_token(password, kind, packet_number) + ";";
}

CommandKind parse_command(const std::string& text, const std::string& expected_team,
                          const std::string& password, std::uint32_t& packet_number) {
    packet_number = 0;
    // An unconfigured vehicle must never match. Empty either way would otherwise make every
    // command addressed to anybody look like one addressed to us.
    if (expected_team.empty() || password.empty()) return CommandKind::none;

    // The terminating ';' is mandatory. A radio delivers partial frames, and without this
    // every truncation of a valid command except the last character is still a valid
    // command -- which for an erase is the difference between a dropped byte and a lost
    // flight log.
    std::size_t end = text.find_last_not_of(" \t\r\n");
    if (end == std::string::npos || text[end] != ';') return CommandKind::none;

    const std::vector<std::string> fields = split_fields(text);
    if (fields.size() < 4) return CommandKind::none;
    if (fields[0] != expected_team) return CommandKind::none;

    std::string command;
    std::string key;
    std::string pn_text;
    for (std::size_t i = 1; i < fields.size(); ++i) {
        const std::string& field = fields[i];
        if (field.rfind("CMD-", 0) == 0) command = field.substr(4);
        else if (field.rfind("KEY-", 0) == 0) key = field.substr(4);
        else if (field.rfind("PN-", 0) == 0) pn_text = field.substr(3);
    }
    if (pn_text.empty() || pn_text.size() > 10) return CommandKind::none;
    unsigned long long pn = 0;
    for (const char c : pn_text) {
        if (c < '0' || c > '9') return CommandKind::none;
        pn = pn * 10 + static_cast<unsigned>(c - '0');
        if (pn > 4294967295ULL) return CommandKind::none;
    }

    // The name is resolved first, because the token is only meaningful against a specific
    // command: an unknown name has no token to check it with.
    const CommandKind kind = kind_of(command);
    if (kind == CommandKind::none) return CommandKind::none;
    if (!tokens_match(key, command_token(password, kind, static_cast<std::uint32_t>(pn)))) {
        return CommandKind::none;
    }

    packet_number = static_cast<std::uint32_t>(pn);
    return kind;
}

}  // namespace cansat
