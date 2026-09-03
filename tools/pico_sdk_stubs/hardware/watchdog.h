#pragma once
// Syntax-check stub. Not the real Pico SDK.
#include <cstdint>

extern "C" {
void watchdog_enable(std::uint32_t delay_ms, bool pause_on_debug);
void watchdog_update(void);
bool watchdog_caused_reboot(void);
}
