#pragma once

#include <cstdint>

namespace flight::sensors {

inline constexpr double kStandardGravity = 9.80665;  // m/s^2

// ------------------------- MPU9250: accelerometer + gyroscope ----------------
//
// Source: InvenSense MPU-9250 Register Map and Register Descriptions (RM-MPU-9250A-00,
// rev 1.6) and the MPU-9250 Product Specification (PS-MPU-9250A-01, rev 1.1).
//
// The accelerometer and gyroscope full-scale sensitivities are unchanged from the
// MPU-6050 the vehicle previously carried, but nothing else about the part is: the
// WHO_AM_I value, the temperature transfer function, the accelerometer's own DLPF
// register and the on-die AK8963 magnetometer are all new. Sharing the sensitivity
// tables is a fact about the two parts, not an assumption carried over.

enum class AccelRange { g2, g4, g8, g16 };
enum class GyroRange { dps250, dps500, dps1000, dps2000 };

struct ImuScales {
    double accel_mps2_per_lsb;
    double gyro_dps_per_lsb;
};

// Full-scale LSB sensitivity, MPU-9250 product specification tables 4.2 and 4.3.
double accel_lsb_per_g(AccelRange range);
double gyro_lsb_per_dps(GyroRange range);

ImuScales imu_scales(AccelRange accel, GyroRange gyro);

// ACCEL_CONFIG / GYRO_CONFIG full-scale select bits (registers 0x1C and 0x1B).
// These live beside the sensitivities on purpose: the range written to the sensor and the
// scale used to convert its output must come from the same enum, or every acceleration is
// wrong by a factor of two, four or eight and nothing in the data looks obviously broken.
constexpr std::uint8_t accel_range_bits(AccelRange range) {
    switch (range) {
        case AccelRange::g2: return 0x00;
        case AccelRange::g4: return 0x08;
        case AccelRange::g8: return 0x10;
        case AccelRange::g16: return 0x18;
    }
    return 0x18;
}

// GYRO_CONFIG also carries FCHOICE_B in bits 1:0. Both bits must be zero for DLPF_CFG in
// CONFIG to take effect at all -- an MPU-9250 written with the MPU-6050's value of this
// register keeps its 8 kHz path and the configured bandwidth is silently ignored.
constexpr std::uint8_t gyro_range_bits(GyroRange range) {
    switch (range) {
        case GyroRange::dps250: return 0x00;
        case GyroRange::dps500: return 0x08;
        case GyroRange::dps1000: return 0x10;
        case GyroRange::dps2000: return 0x18;
    }
    return 0x18;
}

double accel_raw_to_mps2(std::int16_t raw, const ImuScales& scales);
double gyro_raw_to_dps(std::int16_t raw, const ImuScales& scales);

// MPU-9250 on-die temperature, product specification section 3.4.2:
//   degC = (TEMP_OUT - RoomTemp_Offset) / Temp_Sensitivity + 21
// with Temp_Sensitivity 333.87 LSB/degC and a zero room-temperature offset. This is NOT
// the MPU-6050's raw/340 + 36.53 -- the same raw value means a different temperature on
// the two parts, by about 15 degC at room temperature.
double mpu9250_temperature_c(std::int16_t raw);

double vector_magnitude(double x, double y, double z);

// ------------------------- AK8963 magnetometer -------------------------------
//
// Source: AKM AK8963 datasheet, and MPU-9250 register map section 5 (the magnetometer is
// a separate die in the same package, reached over its own I2C address).

enum class MagResolution { bits14, bits16 };

// Measurement range is +-4912 uT in both modes; only the quantisation differs.
inline constexpr double kMagFullScaleUt = 4912.0;

// Earth's total field is between roughly 25 and 65 uT over the inhabited surface. A
// reading outside a margin around that is a magnetic disturbance, a saturated sensor or a
// dead bus -- never a heading worth fusing.
inline constexpr double kEarthFieldMinUt = 20.0;
inline constexpr double kEarthFieldMaxUt = 70.0;

// uT per LSB: 4912 / 8190 (14-bit) and 4912 / 32760 (16-bit).
double mag_ut_per_lsb(MagResolution resolution);

// CNTL1 bit 4 selects the output width. Kept beside the sensitivity for the same reason
// as the accelerometer range bits above.
constexpr std::uint8_t mag_resolution_bits(MagResolution resolution) {
    return resolution == MagResolution::bits16 ? 0x10 : 0x00;
}

// Per-axis sensitivity adjustment from the AK8963 fuse ROM (registers 0x10..0x12):
//   adjusted = raw * ((ASA - 128) * 0.5 / 128 + 1)
// Skipping this leaves a fixed few-percent scale error per axis, which reads as a
// direction-dependent heading error rather than as an obvious fault.
double mag_asa_adjust(std::uint8_t asa_raw);

double mag_raw_to_ut(std::int16_t raw, double ut_per_lsb, double asa_adjust);

// Hard-iron offset (uT, subtracted) and soft-iron diagonal scale (dimensionless,
// multiplied afterwards). A diagonal correction is the practical limit of what a
// hand-rotated figure-of-eight can identify; a full 3x3 soft-iron matrix needs a
// turntable this project does not have.
struct MagCalibration {
    double offset_ut[3] = {0.0, 0.0, 0.0};
    double scale[3] = {1.0, 1.0, 1.0};
    bool valid = false;  // false until a calibration has actually been performed
};

// Apply `calibration` in place. A calibration marked invalid is not applied at all:
// applying identity is the same arithmetic, but leaving it explicit keeps "uncalibrated"
// visible to the caller instead of looking like a calibrated sensor with zero offsets.
void apply_mag_calibration(const MagCalibration& calibration,
                           double& x_ut, double& y_ut, double& z_ut);

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
