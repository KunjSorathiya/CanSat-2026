#include "cansat/command.hpp"

#include <cstddef>
#include <vector>

namespace cansat {
namespace {

const char* name_of(CommandKind kind) {
    switch (kind) {
        case CommandKind::erase_log: return "ERASE_LOG";
        case CommandKind::none: break;
    }
    return "";
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

}  // namespace

std::string format_command(const std::string& team_id, CommandKind kind) {
    if (kind == CommandKind::none) return {};
    return team_id + "; CMD-" + name_of(kind) + "; KEY-" + kCommandKey + ";";
}

CommandKind parse_command(const std::string& text, const std::string& expected_team) {
    // An unconfigured vehicle must never match. Empty expected_team would otherwise make
    // every command addressed to anybody look like one addressed to us.
    if (expected_team.empty()) return CommandKind::none;

    // The terminating ';' is mandatory. A radio delivers partial frames, and without this
    // every truncation of a valid command except the last character is still a valid
    // command -- which for an erase is the difference between a dropped byte and a lost
    // flight log. Telemetry packets end the same way, so this costs nothing.
    std::size_t end = text.find_last_not_of(" \t\r\n");
    if (end == std::string::npos || text[end] != ';') return CommandKind::none;

    const std::vector<std::string> fields = split_fields(text);
    if (fields.size() < 3) return CommandKind::none;
    if (fields[0] != expected_team) return CommandKind::none;

    std::string command;
    std::string key;
    for (std::size_t i = 1; i < fields.size(); ++i) {
        const std::string& field = fields[i];
        if (field.rfind("CMD-", 0) == 0) command = field.substr(4);
        else if (field.rfind("KEY-", 0) == 0) key = field.substr(4);
    }
    if (key != kCommandKey) return CommandKind::none;
    if (command == name_of(CommandKind::erase_log)) return CommandKind::erase_log;
    return CommandKind::none;
}

}  // namespace cansat
