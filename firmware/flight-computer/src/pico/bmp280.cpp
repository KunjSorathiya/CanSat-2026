#include "flight/pico/bmp280.hpp"

#include <cmath>
#include <cstdint>

#ifdef PICO_BUILD
#include "hardware/i2c.h"
#include "pico/time.h"
#endif

namespace flight::pico {

namespace {
constexpr std::uint8_t REG_ID = 0xD0;        // -> 0x58
constexpr std::uint8_t REG_RESET = 0xE0;
constexpr std::uint8_t REG_STATUS = 0xF3;
constexpr std::uint8_t REG_CTRL_MEAS = 0xF4;
constexpr std::uint8_t REG_CONFIG = 0xF5;
constexpr std::uint8_t REG_PRESS_MSB = 0xF7;
constexpr std::uint8_t REG_CALIB = 0x88;  // 24 bytes 0x88..0x9F

// CTRL_MEAS and CONFIG are built from the configured oversampling and filter by
// flight/sensor_timing.hpp, so the register encoding and the timing model that validates
// the sampling rate can never disagree.
}  // namespace

#ifdef PICO_BUILD

namespace {
bool write_reg(i2c_inst_t* bus, std::uint8_t addr, std::uint8_t reg, std::uint8_t value) {
    const std::uint8_t buf[2] = {reg, value};
    return i2c_write_blocking(bus, addr, buf, 2, false) == 2;
}
bool read_regs(i2c_inst_t* bus, std::uint8_t addr, std::uint8_t reg, std::uint8_t* out,
               std::size_t len) {
    if (i2c_write_blocking(bus, addr, &reg, 1, true) != 1) return false;
    return i2c_read_blocking(bus, addr, out, len, false) == static_cast<int>(len);
}
}  // namespace

bool Bmp280::begin(i2c_bus_t bus, const Options& options) {
    bus_ = bus;
    options_ = options;
    ok_ = false;

    std::uint8_t id = 0;
    if (!read_regs(bus_, options_.address, REG_ID, &id, 1) || id != 0x58) {
        return false;
    }
    // A failed reset leaves the device in an unknown configuration, which is worse than
    // no device at all: the calibration read below might still succeed and the driver
    // would report a healthy sensor it never actually configured.
    if (!write_reg(bus_, options_.address, REG_RESET, 0xB6)) {
        return false;
    }
    sleep_ms(5);

    std::uint8_t c[24];
    if (!read_regs(bus_, options_.address, REG_CALIB, c, sizeof(c))) {
        return false;
    }
    auto u16 = [](std::uint8_t lo, std::uint8_t hi) {
        return static_cast<std::uint16_t>(lo | (hi << 8));
    };
    auto s16 = [](std::uint8_t lo, std::uint8_t hi) {
        return static_cast<std::int16_t>(lo | (hi << 8));
    };
    calib_.dig_T1 = u16(c[0], c[1]);
    calib_.dig_T2 = s16(c[2], c[3]);
    calib_.dig_T3 = s16(c[4], c[5]);
    calib_.dig_P1 = u16(c[6], c[7]);
    calib_.dig_P2 = s16(c[8], c[9]);
    calib_.dig_P3 = s16(c[10], c[11]);
    calib_.dig_P4 = s16(c[12], c[13]);
    calib_.dig_P5 = s16(c[14], c[15]);
    calib_.dig_P6 = s16(c[16], c[17]);
    calib_.dig_P7 = s16(c[18], c[19]);
    calib_.dig_P8 = s16(c[20], c[21]);
    calib_.dig_P9 = s16(c[22], c[23]);
    if (calib_.dig_T1 == 0 || calib_.dig_P1 == 0) {
        return false;
    }

    if (!write_reg(bus_, options_.address, REG_CONFIG,
                   sensors::baro_config(options_.filter))) {
        return false;
    }
    if (!write_reg(bus_, options_.address, REG_CTRL_MEAS,
                   sensors::baro_ctrl_meas(options_.osrs_t, options_.osrs_p))) {
        return false;
    }
    sleep_ms(50);

    ok_ = true;
    return true;
}

bool Bmp280::read(BaroSample& out, std::uint64_t now_ms) {
    if (!ok_) return false;
    std::uint8_t d[6];
    if (!read_regs(bus_, options_.address, REG_PRESS_MSB, d, sizeof(d))) {
        return false;
    }
    const std::int32_t adc_P =
        (static_cast<std::int32_t>(d[0]) << 12) | (static_cast<std::int32_t>(d[1]) << 4) | (d[2] >> 4);
    const std::int32_t adc_T =
        (static_cast<std::int32_t>(d[3]) << 12) | (static_cast<std::int32_t>(d[4]) << 4) | (d[5] >> 4);

    const auto r = sensors::bmp280_compensate(calib_, adc_T, adc_P);
    if (!r.valid) {
        return false;
    }
    out.pressure_pa = r.pressure_pa;
    out.temperature_c = r.temperature_c;
    out.altitude_m = sensors::pressure_altitude_m(r.pressure_pa, options_.reference_pressure_pa);
    out.timestamp_ms = now_ms;
    out.valid = std::isfinite(out.altitude_m);
    return out.valid;
}

#else  // host stub

bool Bmp280::begin(i2c_bus_t, const Options& options) {
    options_ = options;
    ok_ = false;
    return false;
}
bool Bmp280::read(BaroSample&, std::uint64_t) { return false; }

#endif

}  // namespace flight::pico
