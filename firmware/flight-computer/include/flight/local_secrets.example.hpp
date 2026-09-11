#pragma once

// Template for the ground-command password. Copy this file to local_secrets.hpp beside it
// and set your own password there. local_secrets.hpp is gitignored: the password must never
// reach the repository.
//
// With local_secrets.hpp present, the flight build has the uplink and its five-minute pre-arm
// command window. Without it, the build has no uplink at all and arms straight after
// power-on. main.cpp refuses to compile a local_secrets.hpp that still says SET-ME, or
// whose password is shorter than 8 characters.
//
// The same password is typed into the web console when a command is sent. It never travels:
// the console sends a digest bound to the command and the packet number.

namespace cansat_local {
inline constexpr const char* kCommandPassword = "SET-ME";
}  // namespace cansat_local
