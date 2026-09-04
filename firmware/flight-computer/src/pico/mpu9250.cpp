#include "flight/pico/mpu9250.hpp"

#include <cmath>
#include <cstdint>

#ifdef PICO_BUILD
#include "hardware/i2c.h"
#include "pico/time.h"
#endif

namespace flight::pico {

namespace {
// ---- MPU-9250 register map (RM-MPU-9250A-00) ----
constexpr std::uint8_t REG_SMPLRT_DIV = 0x19;
constexpr std::uint8_t REG_CONFIG = 0x1A;
constexpr std::uint8_t REG_GYRO_CONFIG = 0x1B;
constexpr std::uint8_t REG_ACCEL_CONFIG = 0x1C;
constexpr std::uint8_t REG_ACCEL_CONFIG2 = 0x1D;
constexpr std::uint8_t REG_INT_PIN_CFG = 0x37;
constexpr std::uint8_t REG_ACCEL_XOUT_H = 0x3B;
constexpr std::uint8_t REG_USER_CTRL = 0x6A;
constexpr std::uint8_t REG_PWR_MGMT_1 = 0x6B;
constexpr std::uint8_t REG_PWR_MGMT_2 = 0x6C;
constexpr std::uint8_t REG_WHO_AM_I = 0x75;

// ---- AK8963 register map ----
constexpr std::uint8_t AK_WIA = 0x00;
constexpr std::uint8_t AK_ST1 = 0x02;   // bit 0 DRDY
constexpr std::uint8_t AK_CNTL1 = 0x0A;
constexpr std::uint8_t AK_CNTL2 = 0x0B;  // bit 0 SRST
constexpr std::uint8_t AK_ASAX = 0x10;

constexpr std::uint8_t AK_ST2_HOFL = 0x08;  // magnetic sensor overflow
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

std::int16_t be16(std::uint8_t hi, std::uint8_t lo) {
    return static_cast<std::int16_t>((static_cast<std::uint16_t>(hi) << 8) | lo);
}

// The AK8963 is the one little-endian device in this vehicle: its data registers are
// laid out low byte first, the opposite of every MPU register pair beside them.
std::int16_t le16(std::uint8_t lo, std::uint8_t hi) {
    return static_cast<std::int16_t>((static_cast<std::uint16_t>(hi) << 8) | lo);
}
}  // namespace

bool Mpu9250::begin(i2c_bus_t bus, const Options& options) {
    bus_ = bus;
    options_ = options;
    ok_ = false;
    mag_ok_ = false;
    who_am_i_ = 0;
    scales_ = sensors::imu_scales(options.accel_range, options.gyro_range);
    mag_ut_per_lsb_ = sensors::mag_ut_per_lsb(options.mag_resolution);
    mag_asa_[0] = mag_asa_[1] = mag_asa_[2] = 1.0;

    if (!read_regs(bus_, options_.address, REG_WHO_AM_I, &who_am_i_, 1)) {
        return false;
    }
    const bool nine_axis_part =
        who_am_i_ == kWhoAmIMpu9250 || who_am_i_ == kWhoAmIMpu9255;
    if (!nine_axis_part && who_am_i_ != kWhoAmIMpu6500) {
        return false;  // not a part this driver knows how to configure
    }

    // Reset, then wait out the datasheet's 100 ms start-up before touching anything else.
    // Skipping this leaves configuration registers holding whatever survived a warm boot.
    if (!write_reg(bus_, options_.address, REG_PWR_MGMT_1, 0x80)) return false;
    sleep_ms(100);
    // Clock source 1: auto-select the best available (the PLL when it is up). Leaving the
    // internal 20 MHz oscillator selected costs both accuracy and temperature stability.
    if (!write_reg(bus_, options_.address, REG_PWR_MGMT_1, 0x01)) return false;
    sleep_ms(10);
    if (!write_reg(bus_, options_.address, REG_PWR_MGMT_2, 0x00)) return false;  // all axes on

    // Gyro filter. FCHOICE_B (GYRO_CONFIG bits 1:0) must be 00 or DLPF_CFG is bypassed
    // entirely and the part stays on its 8 kHz path -- the single easiest way to
    // configure a bandwidth on this device and silently not get it.
    if (!write_reg(bus_, options_.address, REG_CONFIG,
                   static_cast<std::uint8_t>(options_.gyro_dlpf & 0x07))) {
        return false;
    }
    if (!write_reg(bus_, options_.address, REG_GYRO_CONFIG,
                   sensors::gyro_range_bits(options_.gyro_range))) {
        return false;
    }
    if (!write_reg(bus_, options_.address, REG_ACCEL_CONFIG,
                   sensors::accel_range_bits(options_.accel_range))) {
        return false;
    }
    // Accelerometer filter lives in its own register on this part, with accel_fchoice_b
    // (bit 3) cleared to enable it.
    if (!write_reg(bus_, options_.address, REG_ACCEL_CONFIG2,
                   static_cast<std::uint8_t>(options_.accel_dlpf & 0x07))) {
        return false;
    }
    if (!write_reg(bus_, options_.address, REG_SMPLRT_DIV, options_.sample_rate_div)) {
        return false;
    }

    ok_ = true;

    if (!nine_axis_part) {
        return true;  // an MPU-6500: six axes, and honest about it
    }

    // ---- AK8963 over the pass-through bridge ----
    // I2C_MST_EN must be off for BYPASS_EN to take effect: the MPU cannot be master of
    // the auxiliary bus and bridge it to the primary one at the same time.
    if (!write_reg(bus_, options_.address, REG_USER_CTRL, 0x00)) return ok_;
    if (!write_reg(bus_, options_.address, REG_INT_PIN_CFG, 0x02)) return ok_;
    sleep_ms(10);

    std::uint8_t wia = 0;
    if (!read_regs(bus_, kMagAddress, AK_WIA, &wia, 1) || wia != kWhoAmIAk8963) {
        return ok_;  // six-axis operation; the caller sees has_magnetometer() == false
    }

    if (!write_reg(bus_, kMagAddress, AK_CNTL2, 0x01)) return ok_;  // soft reset
    sleep_ms(10);
    if (!write_reg(bus_, kMagAddress, AK_CNTL1, 0x00)) return ok_;  // power down
    sleep_ms(10);

    // Fuse ROM access is the only mode in which the per-axis sensitivity adjustment is
    // readable, and the part must be powered down between every mode change.
    if (!write_reg(bus_, kMagAddress, AK_CNTL1,
                   static_cast<std::uint8_t>(sensors::MagMode::fuse_rom))) {
        return ok_;
    }
    sleep_ms(10);
    std::uint8_t asa[3] = {128, 128, 128};
    const bool asa_ok = read_regs(bus_, kMagAddress, AK_ASAX, asa, 3);
    if (!write_reg(bus_, kMagAddress, AK_CNTL1, 0x00)) return ok_;
    sleep_ms(10);
    if (!asa_ok) return ok_;
    for (int i = 0; i < 3; ++i) {
        mag_asa_[i] = sensors::mag_asa_adjust(asa[i]);
    }

    const std::uint8_t cntl1 =
        static_cast<std::uint8_t>(static_cast<std::uint8_t>(options_.mag_mode) |
                                  sensors::mag_resolution_bits(options_.mag_resolution));
    if (!write_reg(bus_, kMagAddress, AK_CNTL1, cntl1)) return ok_;
    sleep_ms(10);

    mag_ok_ = true;
    return true;
}

bool Mpu9250::read(ImuSample& out, std::uint64_t now_ms) {
    if (!ok_) return false;
    std::uint8_t raw[14];
    if (!read_regs(bus_, options_.address, REG_ACCEL_XOUT_H, raw, sizeof(raw))) {
        return false;
    }
    const std::int16_t ax = be16(raw[0], raw[1]);
    const std::int16_t ay = be16(raw[2], raw[3]);
    const std::int16_t az = be16(raw[4], raw[5]);
    const std::int16_t temp = be16(raw[6], raw[7]);
    const std::int16_t gx = be16(raw[8], raw[9]);
    const std::int16_t gy = be16(raw[10], raw[11]);
    const std::int16_t gz = be16(raw[12], raw[13]);

    out.ax_mps2 = sensors::accel_raw_to_mps2(ax, scales_);
    out.ay_mps2 = sensors::accel_raw_to_mps2(ay, scales_);
    out.az_mps2 = sensors::accel_raw_to_mps2(az, scales_);
    out.gx_dps = sensors::gyro_raw_to_dps(gx, scales_);
    out.gy_dps = sensors::gyro_raw_to_dps(gy, scales_);
    out.gz_dps = sensors::gyro_raw_to_dps(gz, scales_);
    out.die_temperature_c = sensors::mpu9250_temperature_c(temp);
    out.timestamp_ms = now_ms;

    // Reject the all-zero / all-ones bus-fault patterns.
    const bool all_zero = (ax | ay | az | gx | gy | gz) == 0;
    out.valid = !all_zero && std::isfinite(out.ax_mps2) && std::isfinite(out.gz_dps);

    out.mag_valid = false;
    if (mag_ok_) {
        // One burst covers ST1, the six data bytes and ST2. ST2 must be read to complete
        // the measurement cycle -- an AK8963 whose ST2 is never read stops updating, so
        // reading only the data registers yields a magnetometer that works once.
        std::uint8_t mag_raw[8];
        if (read_regs(bus_, kMagAddress, AK_ST1, mag_raw, sizeof(mag_raw))) {
            const bool data_ready = (mag_raw[0] & 0x01) != 0;
            const bool overflowed = (mag_raw[7] & AK_ST2_HOFL) != 0;
            if (data_ready && !overflowed) {
                const std::int16_t mx = le16(mag_raw[1], mag_raw[2]);
                const std::int16_t my = le16(mag_raw[3], mag_raw[4]);
                const std::int16_t mz = le16(mag_raw[5], mag_raw[6]);
                const double ak_x = sensors::mag_raw_to_ut(mx, mag_ut_per_lsb_, mag_asa_[0]);
                const double ak_y = sensors::mag_raw_to_ut(my, mag_ut_per_lsb_, mag_asa_[1]);
                const double ak_z = sensors::mag_raw_to_ut(mz, mag_ut_per_lsb_, mag_asa_[2]);
                mag_axes_to_body(ak_x, ak_y, ak_z, out.mx_ut, out.my_ut, out.mz_ut);
                out.mag_valid = (mx | my | mz) != 0;
            }
        }
    }
    return out.valid;
}

#else  // host stub

bool Mpu9250::begin(i2c_bus_t, const Options& options) {
    options_ = options;
    scales_ = sensors::imu_scales(options.accel_range, options.gyro_range);
    mag_ut_per_lsb_ = sensors::mag_ut_per_lsb(options.mag_resolution);
    ok_ = false;
    mag_ok_ = false;
    return false;
}
bool Mpu9250::read(ImuSample&, std::uint64_t) { return false; }

#endif

}  // namespace flight::pico
