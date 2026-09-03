#pragma once

#include <cstdint>

namespace flight::sensors {

inline constexpr double kStandardGravity = 9.80665;  // m/s^2

// ------------------------------ MPU6050 ------------------------------------

enum class AccelRange { g2, g4, g8, g16 };
enum class GyroRange { dps250, dps500, dps1000, dps2000 };

struct ImuScales {
    double accel_mps2_per_lsb;
    double gyro_dps_per_lsb;
};

// Full-scale LSB sensitivity from the MPU-6000/6050 datasheet.
double accel_lsb_per_g(AccelRange range);
double gyro_lsb_per_dps(GyroRange range);

ImuScales imu_scales(AccelRange accel, GyroRange gyro);

double accel_raw_to_mps2(std::int16_t raw, const ImuScales& scales);
double gyro_raw_to_dps(std::int16_t raw, const ImuScales& scales);

// MPU6050 on-die temperature, datasheet transfer function (degC).
double mpu_temperature_c(std::int16_t raw);

double vector_magnitude(double x, double y, double z);

// ------------------------------ BMP280 -------------------------------------

// Trimming parameters read from the BMP280 calibration registers (0x88..0xA1).
struct Bmp280Calib {
    std::uint16_t dig_T1 = 0;
    std::int16_t dig_T2 = 0;
    std::int16_t dig_T3 = 0;
    std::uint16_t dig_P1 = 0;
    std::int16_t dig_P2 = 0;
    std::int16_t dig_P3 = 0;
    std::int16_t dig_P4 = 0;
    std::int16_t dig_P5 = 0;
    std::int16_t dig_P6 = 0;
    std::int16_t dig_P7 = 0;
    std::int16_t dig_P8 = 0;
    std::int16_t dig_P9 = 0;
};

struct Bmp280Result {
    double temperature_c = 0.0;
    double pressure_pa = 0.0;
    bool valid = false;
};

// Bosch BMP280 fixed-point compensation (datasheet 3.11.3), converted to floating point.
// adc_T / adc_P are the 20-bit raw values from registers 0xF7..0xFC.
Bmp280Result bmp280_compensate(const Bmp280Calib& calib, std::int32_t adc_T, std::int32_t adc_P);

// International barometric formula. reference_pressure_pa is the pressure at the desired
// zero-altitude datum (sea level, or the local ground baseline).
double pressure_altitude_m(double pressure_pa, double reference_pressure_pa);

}  // namespace flight::sensors
