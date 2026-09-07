#pragma once

#include <string>

namespace cansat {

// Ground-to-vehicle maintenance commands.
//
// This vehicle flies with no uplink. The mission is autonomous end to end -- it powers on,
// calibrates, arms, detects its own launch and landing, and never waits to be told
// anything -- and `Configuration::allow_ground_commands` defaults to **false** so a flight
// build genuinely cannot receive one. What follows exists for the bench, where erasing the
// log between test runs otherwise means unplugging the vehicle and pulling the card.
//
// The format is deliberately the telemetry format's shape: semicolon-separated tagged
// fields, so one parser style covers both directions and a command can never be mistaken
// for a packet by either end.
//
//     CAN-Team-25; CMD-ERASE_LOG; KEY-7A3F;
//
// **The key is not security and must not be described as such.** It is four characters in
// clear text over an unauthenticated radio link. Its job is to make an *accidental*
// trigger implausible -- a corrupted telemetry frame, a fragment of another team's traffic,
// or noise that happens to decode. Anyone who wants to send this command can read it here.
// The real protection is elsewhere and is structural: the vehicle only listens in READY
// with ARM-0, on the ground, before flight.
inline constexpr const char* kCommandKey = "7A3F";

enum class CommandKind {
    none,       // not a command, or not one this vehicle will act on
    erase_log,  // reset the onboard log to empty
};

// Renders a command for the given team. Returns an empty string for CommandKind::none.
std::string format_command(const std::string& team_id, CommandKind kind);

// Parses a received line. Returns `none` unless the team matches exactly, the key matches,
// and the command is one this build knows -- so a telemetry packet, a partial frame, or a
// command addressed to another team all return `none` rather than anything actionable.
CommandKind parse_command(const std::string& text, const std::string& expected_team);

}  // namespace cansat
