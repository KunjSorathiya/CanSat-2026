#pragma once
// Syntax-check stub. Not the real Pico SDK.
#include <cstdint>
#include "pico/time.h"

extern "C" {
void stdio_init_all(void);
void sleep_ms(std::uint32_t ms);
void sleep_us(std::uint64_t us);
}

// stdio helpers used by the bring-up diagnostic's antenna-safety prompt.
#define PICO_ERROR_TIMEOUT (-1)
extern "C" {
int getchar_timeout_us(std::uint32_t us);
}
