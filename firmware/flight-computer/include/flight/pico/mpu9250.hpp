#pragma once

#include "flight/interfaces.hpp"
#include "flight/pico/pico_types.hpp"
#include "flight/sensor_math.hpp"
#include "flight/sensor_timing.hpp"

#include <cstdint>

namespace flight::pico {

// MPU-9250 nine-axis driver over I2C: an MPU-6500-class accelerometer and gyroscope on
// one die, and an AKM AK8963 magnetometer on a second die in the same package.
//
// The two dies are reached differently. The accelerometer and gyroscope answer at the
// module's own address (0x68, or 0x69 with AD0 strapped high). The magnetometer is a
// separate I2C slave at 0x0C which is NOT visible from outside until the MPU is told to
// bridge to it. This driver enables the pass-through bridge (INT_PIN_CFG.BYPASS_EN)
// rather than driving the MPU's internal I2C master, because the bypass makes the AK8963
// an ordinary device on the bus the Pico already owns: no slave-register choreography, no
// duplicated copies of the magnetometer data inside the MPU, and a magnetometer failure
// that is visible as a failure rather than as stale bytes in a mirror register.
//
// Defaults chosen for a CanSat: +-16 g and +-2000 deg/s so launch acceleration and spin
// cannot clip, filter bandwidth and rate taken from the flight configuration.
class Mpu9250 {
public:
    // WHO_AM_I values this driver accepts. 0x71 is the MPU-9250 and 0x73 the MPU-9255,
    // both of which carry the magnetometer. 0x70 is an MPU-6500: mechanically and
    // electrically compatible, sold on modules labelled MPU-9250, and with no
    // magnetometer at all. It is accepted as a six-axis part rather than rejected,
    // because a vehicle that flies with degraded attitude beats one that refuses to boot.
    static constexpr std::uint8_t kWhoAmIMpu9250 = 0x71;
    static constexpr std::uint8_t kWhoAmIMpu9255 = 0x73;
    static constexpr std::uint8_t kWhoAmIMpu6500 = 0x70;
    static constexpr std::uint8_t kWhoAmIAk8963 = 0x48;
    static constexpr std::uint8_t kMagAddress = 0x0C;

    struct Options {
        std::uint8_t address = 0x68;  // 0x69 if AD0 is high
        sensors::AccelRange accel_range = sensors::AccelRange::g16;
        sensors::GyroRange gyro_range = sensors::GyroRange::dps2000;
        std::uint8_t gyro_dlpf = 4;   // CONFIG.DLPF_CFG: 20 Hz
        std::uint8_t accel_dlpf = 4;  // ACCEL_CONFIG 2.A_DLPF_CFG: 21.2 Hz
        std::uint8_t sample_rate_div = 4;  // 1 kHz / (1 + div) = 200 Hz
        sensors::MagResolution mag_resolution = sensors::MagResolution::bits16;
        sensors::MagMode mag_mode = sensors::MagMode::continuous_100hz;
    };

    bool begin(i2c_bus_t bus, const Options& options);
    // Fills accelerometer, gyroscope and temperature always; magnetometer fields only
    // when a fresh unsaturated sample was available, flagged by ImuSample::mag_valid.
    bool read(ImuSample& out, std::uint64_t now_ms);

    bool ok() const { return ok_; }
    bool has_magnetometer() const { return mag_ok_; }
    std::uint8_t who_am_i() const { return who_am_i_; }
    // Fuse-ROM sensitivity adjustment actually read from this AK8963, per axis.
    const double* mag_asa() const { return mag_asa_; }

private:
    i2c_bus_t bus_ = nullptr;
    Options options_{};
    sensors::ImuScales scales_{};
    double mag_ut_per_lsb_ = 0.0;
    double mag_asa_[3] = {1.0, 1.0, 1.0};
    std::uint8_t who_am_i_ = 0;
    bool ok_ = false;
    bool mag_ok_ = false;
};

// Rotate a raw AK8963 reading into the MPU-9250's accelerometer/gyroscope body frame.
//
// This is the single most important line in the whole magnetometer path. The AK8963 die
// is mounted rotated inside the package: its X axis lies along the MPU's Y, its Y along
// the MPU's X, and its Z points opposite the MPU's Z (MPU-9250 product specification,
// section 9.1, "Orientation of Axes"). Fusing the two without this rotation produces a
// heading that is confidently, repeatably and completely wrong -- and, because it still
// moves smoothly as the vehicle turns, one that looks entirely healthy on a dashboard.
//
// Exposed as a free function so the mapping can be tested without a bus.
constexpr void mag_axes_to_body(double ak_x, double ak_y, double ak_z,
                                double& body_x, double& body_y, double& body_z) {
    body_x = ak_y;
    body_y = ak_x;
    body_z = -ak_z;
}

}  // namespace flight::pico
