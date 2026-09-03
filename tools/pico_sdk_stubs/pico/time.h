#pragma once
// Syntax-check stub. Not the real Pico SDK.
#include <cstdint>

struct absolute_time_t { std::uint64_t _v; };

extern "C" {
absolute_time_t get_absolute_time(void);
std::uint32_t to_ms_since_boot(absolute_time_t t);
absolute_time_t make_timeout_time_ms(std::uint32_t ms);
bool time_reached(absolute_time_t t);
void sleep_ms(std::uint32_t ms);
}
