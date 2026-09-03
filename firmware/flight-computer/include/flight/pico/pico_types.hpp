#pragma once

// Small compatibility shims so the Pico driver headers parse both with and without the
// Raspberry Pi Pico SDK on the include path. Only src/pico/*.cpp use these headers, and
// only the PICO_BUILD target links the SDK.

#ifdef PICO_BUILD
#include "hardware/i2c.h"
#include "hardware/spi.h"
#include "hardware/uart.h"
namespace flight::pico {
using i2c_bus_t = i2c_inst_t*;
using spi_bus_t = spi_inst_t*;
using uart_bus_t = uart_inst_t*;
}  // namespace flight::pico
#else
namespace flight::pico {
using i2c_bus_t = void*;
using spi_bus_t = void*;
using uart_bus_t = void*;
}  // namespace flight::pico
#endif
