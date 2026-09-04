#pragma once

#include <cstdint>

// Sensor timing model, from the manufacturers' datasheets.
//
// Sampling a sensor faster than it produces data does not produce more data: it re-reads
// the same registers. That is worse than harmless here, because the vertical-speed
// estimate differentiates altitude — duplicated samples read as zero climb rate and drag
// the estimate toward zero exactly when the vehicle is moving fastest.
//
// So the acquisition period is checked against what the sensors can actually deliver, the
// same way the telemetry period is checked against LoRa airtime.
//
// Sources:
//   BMP280 datasheet (BST-BMP280-DS001, rev 1.19), section 3.8 "Measurement time" and
//   table 14 "Filter settings and measurement rates".
//   MPU-6000/6050 Register Map and Descriptions (rev 4.2), registers 25 and 26.

namespace flight::sensors {

// ---------------------------------------------------------------- BMP280 --

// Oversampling settings, expressed as the multiplier the datasheet uses. 0 = skipped.
enum class Oversampling : std::uint8_t { skipped = 0, x1 = 1, x2 = 2, x4 = 4, x8 = 8, x16 = 16 };

// IIR filter coefficient. Higher values suppress short-term pressure disturbances at the
// cost of step-response settling time.
enum class BaroFilter : std::uint8_t { off = 0, x2 = 2, x4 = 4, x8 = 8, x16 = 16 };

// Standby duration between conversions in normal mode, in milliseconds.
// The register accepts a fixed set; 0.5 ms is the shortest and is stored here as its
// tenths so the constant stays integral.
inline constexpr std::uint32_t kBaroStandbyTenthsMsMin = 5;  // 0.5 ms

// Register field encodings.
constexpr std::uint8_t baro_osrs_bits(Oversampling o) {
    switch (o) {
        case Oversampling::skipped: return 0;
        case Oversampling::x1: return 1;
        case Oversampling::x2: return 2;
        case Oversampling::x4: return 3;
        case Oversampling::x8: return 4;
        case Oversampling::x16: return 5;
    }
    return 0;
}

constexpr std::uint8_t baro_filter_bits(BaroFilter f) {
    switch (f) {
        case BaroFilter::off: return 0;
        case BaroFilter::x2: return 1;
        case BaroFilter::x4: return 2;
        case BaroFilter::x8: return 3;
        case BaroFilter::x16: return 4;
    }
    return 0;
}

// CTRL_MEAS (0xF4): osrs_t[7:5] | osrs_p[4:2] | mode[1:0]. mode 11 = normal.
constexpr std::uint8_t baro_ctrl_meas(Oversampling osrs_t, Oversampling osrs_p) {
    return static_cast<std::uint8_t>((baro_osrs_bits(osrs_t) << 5) |
                                     (baro_osrs_bits(osrs_p) << 2) | 0x03);
}

// CONFIG (0xF5): t_sb[7:5] | filter[4:2] | spi3w_en[0]. t_sb 000 = 0.5 ms.
constexpr std::uint8_t baro_config(BaroFilter filter) {
    return static_cast<std::uint8_t>(baro_filter_bits(filter) << 2);
}

// Typical conversion time, datasheet section 3.8.1:
//   t_typ = 1.0 + 2 * osrs_t + (2 * osrs_p + 0.5)   [pressure term omitted when skipped]
// Reproduces the datasheet's published presets: osrs_t x1 / osrs_p x4 gives 11.5 ms,
// which with the 0.5 ms standby is the 83 Hz "handheld device, dynamic" preset.
constexpr double baro_measure_ms_typ(Oversampling osrs_t, Oversampling osrs_p) {
    double t = 1.0;
    if (osrs_t != Oversampling::skipped) t += 2.0 * static_cast<double>(osrs_t);
    if (osrs_p != Oversampling::skipped) t += 2.0 * static_cast<double>(osrs_p) + 0.5;
    return t;
}

// Worst-case conversion time, datasheet section 3.8.1:
//   t_max = 1.25 + 2.3 * osrs_t + (2.3 * osrs_p + 0.575)
// Reproduces the datasheet's 43.2 ms figure for osrs_t x2 / osrs_p x16.
constexpr double baro_measure_ms_max(Oversampling osrs_t, Oversampling osrs_p) {
    double t = 1.25;
    if (osrs_t != Oversampling::skipped) t += 2.3 * static_cast<double>(osrs_t);
    if (osrs_p != Oversampling::skipped) t += 2.3 * static_cast<double>(osrs_p) + 0.575;
    return t;
}

// Output data rate in normal mode: one conversion plus the standby interval.
constexpr double baro_output_rate_hz(Oversampling osrs_t, Oversampling osrs_p,
                                     double standby_ms = 0.5) {
    const double period = baro_measure_ms_typ(osrs_t, osrs_p) + standby_ms;
    return period > 0.0 ? 1000.0 / period : 0.0;
}

// Shortest sampling period at which every read is guaranteed to return a fresh
// conversion, even in the datasheet's worst case.
constexpr double baro_min_sample_period_ms(Oversampling osrs_t, Oversampling osrs_p,
                                           double standby_ms = 0.5) {
    return baro_measure_ms_max(osrs_t, osrs_p) + standby_ms;
}

// ---------------------------------------------------------------- MPU6050 --

// With DLPF_CFG in 1..6 the gyro output rate is 1 kHz; with 0 or 7 it is 8 kHz.
// Sample rate = gyro output rate / (1 + SMPLRT_DIV).  Register map, register 25.
constexpr double imu_sample_rate_hz(std::uint8_t dlpf_cfg, std::uint8_t smplrt_div) {
    const double base = (dlpf_cfg >= 1 && dlpf_cfg <= 6) ? 1000.0 : 8000.0;
    return base / (1.0 + static_cast<double>(smplrt_div));
}

// Accelerometer bandwidth for each DLPF_CFG setting, register map register 26.
constexpr double imu_accel_bandwidth_hz(std::uint8_t dlpf_cfg) {
    switch (dlpf_cfg) {
        case 0: return 260.0;
        case 1: return 184.0;
        case 2: return 94.0;
        case 3: return 44.0;
        case 4: return 21.0;
        case 5: return 10.0;
        case 6: return 5.0;
        default: return 260.0;
    }
}

// Gyroscope bandwidth for each DLPF_CFG setting, register map register 26.
constexpr double imu_gyro_bandwidth_hz(std::uint8_t dlpf_cfg) {
    switch (dlpf_cfg) {
        case 0: return 256.0;
        case 1: return 188.0;
        case 2: return 98.0;
        case 3: return 42.0;
        case 4: return 20.0;
        case 5: return 10.0;
        case 6: return 5.0;
        default: return 256.0;
    }
}

}  // namespace flight::sensors
