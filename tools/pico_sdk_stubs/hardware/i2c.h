#pragma once
// Syntax-check stub. Not the real Pico SDK.
#include <cstddef>
#include <cstdint>

struct i2c_inst_t;
extern i2c_inst_t* i2c0;
extern i2c_inst_t* i2c1;

extern "C" {
std::uint32_t i2c_init(i2c_inst_t* i2c, std::uint32_t baudrate);
int i2c_write_blocking(i2c_inst_t* i2c, std::uint8_t addr, const std::uint8_t* src,
                       std::size_t len, bool nostop);
int i2c_read_blocking(i2c_inst_t* i2c, std::uint8_t addr, std::uint8_t* dst, std::size_t len,
                      bool nostop);
}
