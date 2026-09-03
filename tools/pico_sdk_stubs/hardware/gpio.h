#pragma once
// Syntax-check stub. Not the real Pico SDK.
#include <cstdint>

enum gpio_function_t {
    GPIO_FUNC_XIP, GPIO_FUNC_SPI, GPIO_FUNC_UART, GPIO_FUNC_I2C,
    GPIO_FUNC_PWM, GPIO_FUNC_SIO, GPIO_FUNC_PIO0, GPIO_FUNC_PIO1,
    GPIO_FUNC_GPCK, GPIO_FUNC_USB, GPIO_FUNC_NULL = 0x1f
};

static const bool GPIO_OUT = true;
static const bool GPIO_IN = false;

extern "C" {
void gpio_init(std::uint32_t gpio);
void gpio_set_dir(std::uint32_t gpio, bool out);
void gpio_set_function(std::uint32_t gpio, gpio_function_t fn);
void gpio_pull_up(std::uint32_t gpio);
void gpio_pull_down(std::uint32_t gpio);
void gpio_put(std::uint32_t gpio, bool value);
bool gpio_get(std::uint32_t gpio);
}
