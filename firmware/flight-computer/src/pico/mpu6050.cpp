#include "flight/pico/mpu6050.hpp"

#include <cmath>
#include <cstdint>

#ifdef PICO_BUILD
#include "hardware/i2c.h"
#include "pico/time.h"
#endif

namespace flight::pico {

namespace {
constexpr std::uint8_t REG_SMPLRT_DIV = 0x19;
constexpr std::uint8_t REG_CONFIG = 0x1A;
constexpr std::uint8_t REG_GYRO_CONFIG = 0x1B;
constexpr std::uint8_t REG_ACCEL_CONFIG = 0x1C;
constexpr std::uint8_t REG_ACCEL_XOUT_H = 0x3B;
constexpr std::uint8_t REG_PWR_MGMT_1 = 0x6B;
constexpr std::uint8_t REG_WHO_AM_I = 0x75;
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

bool Mpu6050::begin(i2c_bus_t bus, const Options& options) {
    bus_ = bus;
    options_ = options;
    ok_ = false;
    scales_ = sensors::imu_scales(options.accel_range, options.gyro_range);

    std::uint8_t who = 0;
    if (!read_regs(bus_, options_.address, REG_WHO_AM_I, &who, 1) || who != 0x68) {
        return false;
    }
    // Wake, select the gyro X PLL as clock source for stability.
    if (!write_reg(bus_, options_.address, REG_PWR_MGMT_1, 0x01)) return false;
    sleep_ms(50);
    if (!write_reg(bus_, options_.address, REG_SMPLRT_DIV, options_.sample_rate_div)) return false;
    if (!write_reg(bus_, options_.address, REG_CONFIG, options_.dlpf)) return false;
    if (!write_reg(bus_, options_.address, REG_GYRO_CONFIG, sensors::gyro_range_bits(options_.gyro_range)))
        return false;
    if (!write_reg(bus_, options_.address, REG_ACCEL_CONFIG, sensors::accel_range_bits(options_.accel_range)))
        return false;

    ok_ = true;
    return true;
}

bool Mpu6050::read(ImuSample& out, std::uint64_t now_ms) {
    if (!ok_) return false;
    std::uint8_t raw[14];
    if (!read_regs(bus_, options_.address, REG_ACCEL_XOUT_H, raw, sizeof(raw))) {
        return false;
    }
    auto s16 = [](std::uint8_t hi, std::uint8_t lo) {
        return static_cast<std::int16_t>((static_cast<std::uint16_t>(hi) << 8) | lo);
    };
    const std::int16_t ax = s16(raw[0], raw[1]);
    const std::int16_t ay = s16(raw[2], raw[3]);
    const std::int16_t az = s16(raw[4], raw[5]);
    const std::int16_t temp = s16(raw[6], raw[7]);
    const std::int16_t gx = s16(raw[8], raw[9]);
    const std::int16_t gy = s16(raw[10], raw[11]);
    const std::int16_t gz = s16(raw[12], raw[13]);

    out.ax_mps2 = sensors::accel_raw_to_mps2(ax, scales_);
    out.ay_mps2 = sensors::accel_raw_to_mps2(ay, scales_);
    out.az_mps2 = sensors::accel_raw_to_mps2(az, scales_);
    out.gx_dps = sensors::gyro_raw_to_dps(gx, scales_);
    out.gy_dps = sensors::gyro_raw_to_dps(gy, scales_);
    out.gz_dps = sensors::gyro_raw_to_dps(gz, scales_);
    out.die_temperature_c = sensors::mpu_temperature_c(temp);
    out.timestamp_ms = now_ms;

    // Reject the all-zero / all-ones bus-fault patterns.
    const bool all_zero = (ax | ay | az | gx | gy | gz) == 0;
    out.valid = !all_zero && std::isfinite(out.ax_mps2) && std::isfinite(out.gz_dps);
    return out.valid;
}

#else  // host stub

bool Mpu6050::begin(i2c_bus_t, const Options& options) {
    options_ = options;
    scales_ = sensors::imu_scales(options.accel_range, options.gyro_range);
    ok_ = false;
    return false;
}
bool Mpu6050::read(ImuSample&, std::uint64_t) { return false; }

#endif

}  // namespace flight::pico
