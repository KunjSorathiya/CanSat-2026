#pragma once

#include "flight/interfaces.hpp"
#include "flight/pico/pico_types.hpp"
#include "flight/sensor_math.hpp"
#include "flight/sensor_timing.hpp"

#include <cstdint>

namespace flight::pico {

// BMP280 pressure + temperature driver over I2C, using the Bosch fixed-point
// compensation in sensor_math. Runs in normal mode; the oversampling and filter come
// from the flight configuration, because they set the output data rate and therefore
// bound how fast the flight loop may sample (see flight/sensor_timing.hpp).
class Bmp280 {
public:
    struct Options {
        std::uint8_t address = 0x76;  // 0x77 if SDO is high
        double reference_pressure_pa = 101325.0;
        // Datasheet "handheld device, dynamic" preset: 83 Hz output, low noise.
        sensors::Oversampling osrs_t = sensors::Oversampling::x1;
        sensors::Oversampling osrs_p = sensors::Oversampling::x4;
        sensors::BaroFilter filter = sensors::BaroFilter::x16;
    };

    bool begin(i2c_bus_t bus, const Options& options);
    bool read(BaroSample& out, std::uint64_t now_ms);
    bool ok() const { return ok_; }
    const sensors::Bmp280Calib& calibration() const { return calib_; }

private:
    i2c_bus_t bus_ = nullptr;
    Options options_{};
    sensors::Bmp280Calib calib_{};
    bool ok_ = false;
};

}  // namespace flight::pico
