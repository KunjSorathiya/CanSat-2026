#pragma once

#include "flight/interfaces.hpp"
#include "flight/pico/pico_types.hpp"
#include "flight/sensor_math.hpp"

#include <cstdint>

namespace flight::pico {

// MPU6050 accelerometer + gyroscope driver over I2C. Defaults chosen for a CanSat:
// wide +-16 g / +-2000 deg/s so launch acceleration and spin do not clip, 44 Hz DLPF.
class Mpu6050 {
public:
    struct Options {
        std::uint8_t address = 0x68;  // 0x69 if AD0 is high
        sensors::AccelRange accel_range = sensors::AccelRange::g16;
        sensors::GyroRange gyro_range = sensors::GyroRange::dps2000;
        std::uint8_t dlpf = 0x03;      // ~44 Hz accel / 42 Hz gyro
        std::uint8_t sample_rate_div = 0x04;  // 1 kHz / (1 + div) = 200 Hz
    };

    bool begin(i2c_bus_t bus, const Options& options);
    bool read(ImuSample& out, std::uint64_t now_ms);
    bool ok() const { return ok_; }

private:
    i2c_bus_t bus_ = nullptr;
    Options options_{};
    sensors::ImuScales scales_{};
    bool ok_ = false;
};

}  // namespace flight::pico
