#include "flight/sensor_math.hpp"

#include <cmath>

namespace flight::sensors {

double accel_lsb_per_g(AccelRange range) {
    switch (range) {
        case AccelRange::g2: return 16384.0;
        case AccelRange::g4: return 8192.0;
        case AccelRange::g8: return 4096.0;
        case AccelRange::g16: return 2048.0;
    }
    return 16384.0;
}

double gyro_lsb_per_dps(GyroRange range) {
    switch (range) {
        case GyroRange::dps250: return 131.0;
        case GyroRange::dps500: return 65.5;
        case GyroRange::dps1000: return 32.8;
        case GyroRange::dps2000: return 16.4;
    }
    return 131.0;
}

ImuScales imu_scales(AccelRange accel, GyroRange gyro) {
    ImuScales scales;
    scales.accel_mps2_per_lsb = kStandardGravity / accel_lsb_per_g(accel);
    scales.gyro_dps_per_lsb = 1.0 / gyro_lsb_per_dps(gyro);
    return scales;
}

double accel_raw_to_mps2(std::int16_t raw, const ImuScales& scales) {
    return static_cast<double>(raw) * scales.accel_mps2_per_lsb;
}

double gyro_raw_to_dps(std::int16_t raw, const ImuScales& scales) {
    return static_cast<double>(raw) * scales.gyro_dps_per_lsb;
}

double mpu9250_temperature_c(std::int16_t raw) {
    // PS-MPU-9250A-01 section 3.4.2. RoomTemp_Offset is zero for the MPU-9250, so the
    // MPU-6050's 36.53 degC intercept does not appear here at all.
    return static_cast<double>(raw) / 333.87 + 21.0;
}

double mag_ut_per_lsb(MagResolution resolution) {
    // 4912 uT across 8190 counts (14-bit) or 32760 counts (16-bit): 0.6 and 0.15 uT/LSB.
    return resolution == MagResolution::bits16 ? kMagFullScaleUt / 32760.0
                                               : kMagFullScaleUt / 8190.0;
}

double mag_asa_adjust(std::uint8_t asa_raw) {
    return (static_cast<double>(asa_raw) - 128.0) * 0.5 / 128.0 + 1.0;
}

double mag_raw_to_ut(std::int16_t raw, double ut_per_lsb, double asa_adjust) {
    return static_cast<double>(raw) * ut_per_lsb * asa_adjust;
}

void apply_mag_calibration(const MagCalibration& calibration,
                           double& x_ut, double& y_ut, double& z_ut) {
    if (!calibration.valid) {
        return;
    }
    x_ut = (x_ut - calibration.offset_ut[0]) * calibration.scale[0];
    y_ut = (y_ut - calibration.offset_ut[1]) * calibration.scale[1];
    z_ut = (z_ut - calibration.offset_ut[2]) * calibration.scale[2];
}

double vector_magnitude(double x, double y, double z) {
    return std::sqrt(x * x + y * y + z * z);
}

Bmp280Result bmp280_compensate(const Bmp280Calib& c, std::int32_t adc_T, std::int32_t adc_P) {
    Bmp280Result out;
    // 0x80000 is the reset / "measurement skipped" raw value.
    if (c.dig_P1 == 0 || adc_T == 0x80000 || adc_P == 0x80000) {
        return out;
    }

    // ---- temperature (datasheet: returns 0.01 degC, sets t_fine) ----
    std::int32_t var1t = ((((adc_T >> 3) - (static_cast<std::int32_t>(c.dig_T1) << 1))) *
                          static_cast<std::int32_t>(c.dig_T2)) >>
                         11;
    std::int32_t var2t = (((((adc_T >> 4) - static_cast<std::int32_t>(c.dig_T1)) *
                            ((adc_T >> 4) - static_cast<std::int32_t>(c.dig_T1))) >>
                           12) *
                          static_cast<std::int32_t>(c.dig_T3)) >>
                         14;
    std::int32_t t_fine = var1t + var2t;
    std::int32_t temperature = (t_fine * 5 + 128) >> 8;

    // ---- pressure (datasheet 64-bit path: returns Pa in Q24.8) ----
    std::int64_t var1 = static_cast<std::int64_t>(t_fine) - 128000;
    std::int64_t var2 = var1 * var1 * static_cast<std::int64_t>(c.dig_P6);
    var2 += (var1 * static_cast<std::int64_t>(c.dig_P5)) << 17;
    var2 += static_cast<std::int64_t>(c.dig_P4) << 35;
    var1 = ((var1 * var1 * static_cast<std::int64_t>(c.dig_P3)) >> 8) +
           ((var1 * static_cast<std::int64_t>(c.dig_P2)) << 12);
    var1 = (((static_cast<std::int64_t>(1) << 47) + var1) * static_cast<std::int64_t>(c.dig_P1)) >> 33;
    if (var1 == 0) {
        return out;  // avoid divide-by-zero
    }
    std::int64_t p = 1048576 - adc_P;
    p = (((p << 31) - var2) * 3125) / var1;
    var1 = (static_cast<std::int64_t>(c.dig_P9) * (p >> 13) * (p >> 13)) >> 25;
    var2 = (static_cast<std::int64_t>(c.dig_P8) * p) >> 19;
    p = ((p + var1 + var2) >> 8) + (static_cast<std::int64_t>(c.dig_P7) << 4);

    out.temperature_c = static_cast<double>(temperature) / 100.0;
    out.pressure_pa = static_cast<double>(p) / 256.0;
    out.valid = std::isfinite(out.pressure_pa) && out.pressure_pa > 0.0;
    return out;
}

double pressure_altitude_m(double pressure_pa, double reference_pressure_pa) {
    if (!(pressure_pa > 0.0) || !(reference_pressure_pa > 0.0)) {
        return 0.0;
    }
    return 44330.0 * (1.0 - std::pow(pressure_pa / reference_pressure_pa, 1.0 / 5.255));
}

}  // namespace flight::sensors
