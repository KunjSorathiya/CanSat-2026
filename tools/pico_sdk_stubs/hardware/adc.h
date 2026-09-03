#pragma once
// Syntax-check stub. Not the real Pico SDK.
#include <cstdint>

extern "C" {
void adc_init(void);
void adc_gpio_init(std::uint32_t gpio);
void adc_select_input(std::uint32_t input);
std::uint16_t adc_read(void);
}
