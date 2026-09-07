#pragma once

#include <cstdint>
#include <string>

namespace cansat {

// Ground-to-vehicle maintenance commands.
//
// This vehicle flies with no uplink. `Configuration::allow_ground_commands` defaults to
// false, so a flight build never enters receive mode. What follows exists for the bench,
// where erasing the log between test runs otherwise means unplugging the vehicle and
// pulling the card.
//
//     CAN-Team-25; CMD-ERASE_LOG; PN-1234; KEY-3f9a1c04b7e25d68;
//
// **What the key is, and what it is not.**
//
// It is not the password. It is a 64-bit digest of the password *and* the packet number the
// operator was looking at when they pressed the button, so the password itself never
// travels and a frame captured off the air cannot be sent again: the vehicle refuses any
// packet number it has already accepted, or one it has not reached yet.
//
// **This is not cryptography and must never be described as such.** FNV-1a is a hash, not a
// MAC; the digest is 64 bits; the link is unencrypted and unauthenticated. Someone who
// knows the password can compute a valid token, and someone who watches enough traffic
// could attack the digest. What this defeats is the threat that actually exists on a
// shared 433 MHz test channel: an accidental trigger, a corrupted frame, another team's
// traffic, and a replay of a command they saw work.
//
// The protection that carries real weight is still structural and unchanged -- the vehicle
// acts on a command only in READY with ARM-0, on the ground, and only if it was built with
// ground commands enabled at all.
enum class CommandKind {
    none,       // not a command, or not one this vehicle will act on
    erase_log,  // reset the onboard log to empty
};

// FNV-1a over `password + "|" + packet_number`, rendered as 16 lowercase hex characters.
// Both ends compute it the same way; test-data/command-tokens.tsv holds the agreed answers
// so the C++ and JavaScript implementations cannot drift apart unnoticed.
std::string command_token(const std::string& password, std::uint32_t packet_number);

// Renders a command. Returns an empty string for CommandKind::none or an empty password --
// an unset password must never produce something that looks like a valid command.
std::string format_command(const std::string& team_id, CommandKind kind,
                           const std::string& password, std::uint32_t packet_number);

// Parses a received line. Returns `none` unless the team matches exactly, the token matches
// the one this password and packet number produce, and the command is one this build knows.
// `packet_number` is filled with the number the command carried, for the caller's replay
// check -- this function has no memory of its own.
CommandKind parse_command(const std::string& text, const std::string& expected_team,
                          const std::string& password, std::uint32_t& packet_number);

}  // namespace cansat
