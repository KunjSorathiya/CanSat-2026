// Comprehensive host test suite for the CanSat flight core. No hardware required.
#include "cansat/link_profile.hpp"
#include "cansat/lora_airtime.hpp"
#include "cansat/sx1278.hpp"
#include "cansat/command.hpp"
#include "cansat/telemetry.hpp"
#include "flight/config.hpp"
#include "flight/controller.hpp"
#include "flight/fault_manager.hpp"
#include "flight/gps_parser.hpp"
#include "flight/orientation.hpp"
#include "flight/pico/mpu9250.hpp"
#include "flight/raw_block_log.hpp"
#include "flight/scheduler.hpp"
#include "flight/sensor_math.hpp"
#include "flight/sensor_timing.hpp"
#include "flight/startup_calibration.hpp"
#include "flight/state_machine.hpp"
#include "flight/sound_level.hpp"
#include "flight/telemetry_builder.hpp"
#include "mock_hardware.hpp"

#include <array>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <fstream>
#include <memory>
#include <initializer_list>
#include <iostream>
#include <string>
#include <vector>

namespace {

int g_failures = 0;
int g_checks = 0;

#define CHECK(cond)                                                              \
    do {                                                                        \
        ++g_checks;                                                             \
        if (!(cond)) {                                                          \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
            ++g_failures;                                                       \
        }                                                                       \
    } while (0)

bool approx(double a, double b, double eps) { return std::fabs(a - b) <= eps; }

cansat::TelemetryRecord make_valid_record() {
    cansat::TelemetryRecord r;
    r.team_id = "CAN-Team-07";
    r.packet_number = 1;
    r.timestamp_ms = 1000;
    r.altitude_m = 10.0;
    r.pressure_pa = 101325.0;
    r.temperature_c = 25.0;
    r.roll_deg = 1.0;
    r.pitch_deg = 2.0;
    r.yaw_deg = 3.0;
    r.acceleration_x_mps2 = 0.1;
    r.acceleration_y_mps2 = 0.2;
    r.acceleration_z_mps2 = 9.8;
    r.validity.altitude = r.validity.pressure = r.validity.temperature = true;
    r.validity.roll = r.validity.pitch = r.validity.yaw = true;
    r.validity.acceleration_x = r.validity.acceleration_y = r.validity.acceleration_z = true;
    return r;
}

// ----------------------------------------------------------------------------
// mandatory_valid() is the predicate that decides whether a record may be transmitted at
// all: format_packet() refuses a record it rejects. It is a nine-term AND over a struct of
// nine flags, and until now nothing exercised it. Drop any one flag and the record must be
// rejected -- a reading the vehicle never took must not travel as though it had.
//
// The matching structural check, that the struct and the AND name the same number of
// flags, lives in tools/check_doc_claims.py: it is the half that catches a tenth flag
// added to the header and forgotten in the function.
void test_mandatory_validity_covers_every_flag() {
    using cansat::TelemetryValidity;
    bool TelemetryValidity::*const flags[] = {
        &TelemetryValidity::altitude,       &TelemetryValidity::pressure,
        &TelemetryValidity::temperature,    &TelemetryValidity::roll,
        &TelemetryValidity::pitch,          &TelemetryValidity::yaw,
        &TelemetryValidity::acceleration_x, &TelemetryValidity::acceleration_y,
        &TelemetryValidity::acceleration_z,
    };
    const std::size_t flag_count = sizeof(flags) / sizeof(flags[0]);

    TelemetryValidity all;
    CHECK(!all.mandatory_valid());  // default-constructed: nothing measured yet
    for (std::size_t i = 0; i < flag_count; ++i) all.*flags[i] = true;
    CHECK(all.mandatory_valid());

    for (std::size_t i = 0; i < flag_count; ++i) {
        TelemetryValidity one_missing = all;
        one_missing.*flags[i] = false;
        CHECK(!one_missing.mandatory_valid());

        // ...and a record carrying that gap must not become a packet.
        auto record = make_valid_record();
        record.validity = one_missing;
        CHECK(!cansat::format_packet(record).has_value());
    }
}

// ----------------------------------------------------------------------------
void test_telemetry_format_exact() {
    const auto packet = cansat::format_packet(make_valid_record());
    CHECK(packet.has_value());
    CHECK(*packet ==
          "CAN-Team-07; P-001; Ti-00:00:01:000; A-10.0; Pr-101325.00; T-25.0; "
          "Ro-1.0; Pi-2.0; Ya-3.0; AX-0.10; AY-0.20; AZ-9.80;");

    // Round-trips through the (bug-fixed) parser.
    const auto parsed = cansat::parse_packet(*packet);
    CHECK(static_cast<bool>(parsed));
    CHECK(parsed.record->packet_number == 1);
    CHECK(approx(parsed.record->pressure_pa, 101325.0, 1e-6));
    CHECK(parsed.record->team_id == "CAN-Team-07");
}

void test_packet_numbering_and_padding() {
    auto r = make_valid_record();
    r.packet_number = 1;
    CHECK(cansat::format_packet(r)->find("; P-001; ") != std::string::npos);
    r.packet_number = 42;
    CHECK(cansat::format_packet(r)->find("; P-042; ") != std::string::npos);
    r.packet_number = 1234;
    CHECK(cansat::format_packet(r)->find("; P-1234; ") != std::string::npos);
    r.packet_number = 0;  // rulebook counter starts at 1
    CHECK(!cansat::format_packet(r).has_value());
}

void test_parser_rejects_precision_and_order() {
    const std::string good =
        "CAN-Team-07; P-001; Ti-00:00:01:000; A-10.0; Pr-101325.00; T-25.0; "
        "Ro-1.0; Pi-2.0; Ya-3.0; AX-0.10; AY-0.20; AZ-9.80;";
    CHECK(static_cast<bool>(cansat::parse_packet(good)));

    std::string bad_prec = good;
    bad_prec.replace(bad_prec.find("A-10.0"), 6, "A-10");
    CHECK(!static_cast<bool>(cansat::parse_packet(bad_prec)));

    std::string bad_order = good;
    bad_order.replace(bad_order.find("; T-25.0"), 8, "; AX-25.0");
    CHECK(!static_cast<bool>(cansat::parse_packet(bad_order)));

    std::string placeholder = good;
    placeholder.replace(0, 11, "CAN-Team-XX");
    CHECK(!static_cast<bool>(cansat::parse_packet(placeholder)));

    std::string bad_ts = good;
    bad_ts.replace(bad_ts.find("Ti-00:00:01:000"), 15, "Ti-00:99:01:000");
    CHECK(!static_cast<bool>(cansat::parse_packet(bad_ts)));
}

// ----------------------------------------------------------------------------
void test_imu_scaling() {
    using namespace flight::sensors;
    const ImuScales s = imu_scales(AccelRange::g16, GyroRange::dps2000);
    CHECK(approx(accel_raw_to_mps2(2048, s), kStandardGravity, 1e-6));
    CHECK(approx(accel_raw_to_mps2(-2048, s), -kStandardGravity, 1e-6));
    CHECK(approx(gyro_raw_to_dps(164, s), 10.0, 0.05));

    const ImuScales s2 = imu_scales(AccelRange::g2, GyroRange::dps250);
    CHECK(approx(accel_raw_to_mps2(16384, s2), kStandardGravity, 1e-6));
    CHECK(approx(gyro_raw_to_dps(131, s2), 1.0, 0.01));

    // MPU-9250 temperature, NOT the MPU-6050's raw/340 + 36.53. Carrying the old
    // transfer function across would have reported room temperature as ~36 degC.
    CHECK(approx(mpu9250_temperature_c(0), 21.0, 1e-6));
    CHECK(approx(mpu9250_temperature_c(333), 21.0 + 333.0 / 333.87, 1e-9));
    CHECK(mpu9250_temperature_c(0) < 30.0);
}

// The magnetometer conversion chain: quantisation, the fuse-ROM sensitivity adjustment,
// and the hard/soft-iron correction applied on top of both.
void test_magnetometer_conversions() {
    using namespace flight::sensors;

    // 4912 uT across the full 16-bit range, and a quarter of the resolution at 14 bits.
    CHECK(approx(mag_ut_per_lsb(MagResolution::bits16), 4912.0 / 32760.0, 1e-12));
    CHECK(approx(mag_ut_per_lsb(MagResolution::bits14), 4912.0 / 8190.0, 1e-12));
    CHECK(approx(mag_ut_per_lsb(MagResolution::bits14),
                 4.0 * mag_ut_per_lsb(MagResolution::bits16), 1e-12));
    CHECK(mag_resolution_bits(MagResolution::bits16) == 0x10);
    CHECK(mag_resolution_bits(MagResolution::bits14) == 0x00);

    // ASA 128 is unity; the adjustment spans 0.5 to ~1.5 across the byte.
    CHECK(approx(mag_asa_adjust(128), 1.0, 1e-12));
    CHECK(approx(mag_asa_adjust(0), 0.5, 1e-12));
    CHECK(approx(mag_asa_adjust(255), 1.0 + 127.0 * 0.5 / 128.0, 1e-12));

    // A full-scale count is full scale, and the ASA multiplies it.
    const double lsb = mag_ut_per_lsb(MagResolution::bits16);
    CHECK(approx(mag_raw_to_ut(32760, lsb, 1.0), 4912.0, 1e-6));
    CHECK(approx(mag_raw_to_ut(1000, lsb, mag_asa_adjust(0)),
                 0.5 * mag_raw_to_ut(1000, lsb, 1.0), 1e-9));

    // Hard iron is subtracted, soft iron scales what is left -- and an invalid
    // calibration is not applied at all, rather than silently applied as identity.
    MagCalibration cal;
    cal.offset_ut[0] = 10.0; cal.offset_ut[1] = -5.0; cal.offset_ut[2] = 2.0;
    cal.scale[0] = 1.1; cal.scale[1] = 0.9; cal.scale[2] = 1.0;
    double x = 30.0, y = 30.0, z = 30.0;
    apply_mag_calibration(cal, x, y, z);
    CHECK(approx(x, 30.0, 1e-12));  // still invalid -> untouched
    cal.valid = true;
    apply_mag_calibration(cal, x, y, z);
    CHECK(approx(x, (30.0 - 10.0) * 1.1, 1e-12));
    CHECK(approx(y, (30.0 + 5.0) * 0.9, 1e-12));
    CHECK(approx(z, (30.0 - 2.0) * 1.0, 1e-12));
}

// The AK8963 die sits rotated inside the MPU-9250 package. Getting this wrong produces a
// heading that moves smoothly and is completely wrong, so it is worth its own test.
void test_magnetometer_axes_are_rotated_into_the_body_frame() {
    double bx = 0.0, by = 0.0, bz = 0.0;
    flight::pico::mag_axes_to_body(1.0, 2.0, 3.0, bx, by, bz);
    CHECK(approx(bx, 2.0, 1e-12));   // body X comes from the AK8963's Y
    CHECK(approx(by, 1.0, 1e-12));   // body Y comes from the AK8963's X
    CHECK(approx(bz, -3.0, 1e-12));  // body Z is the AK8963's Z, inverted

    // The mapping is a reflection-free swap of two axes plus a sign flip, so it preserves
    // the field magnitude. A mapping that did not would corrupt the earth-field gate.
    const double before = flight::sensors::vector_magnitude(1.0, 2.0, 3.0);
    CHECK(approx(flight::sensors::vector_magnitude(bx, by, bz), before, 1e-12));

    // Applying it twice returns the X/Y swap to where it started, which is the cheap way
    // to notice a mapping that was written as a rotation by mistake.
    double rx = 0.0, ry = 0.0, rz = 0.0;
    flight::pico::mag_axes_to_body(bx, by, bz, rx, ry, rz);
    CHECK(approx(rx, 1.0, 1e-12));
    CHECK(approx(ry, 2.0, 1e-12));
    CHECK(approx(rz, 3.0, 1e-12));
}

// The range written to the IMU and the scale used to convert its output must agree. A
// mismatch multiplies every acceleration by two, four or eight, and the data still looks
// entirely plausible — which is what makes it worth a dedicated test.
void test_imu_range_bits_match_their_sensitivities() {
    using namespace flight::sensors;

    struct Row {
        AccelRange range;
        std::uint8_t bits;
        double lsb_per_g;
        double full_scale_g;
    };
    const Row accel_rows[] = {
        {AccelRange::g2, 0x00, 16384.0, 2.0},
        {AccelRange::g4, 0x08, 8192.0, 4.0},
        {AccelRange::g8, 0x10, 4096.0, 8.0},
        {AccelRange::g16, 0x18, 2048.0, 16.0},
    };
    for (const Row& row : accel_rows) {
        CHECK(accel_range_bits(row.range) == row.bits);
        CHECK(approx(accel_lsb_per_g(row.range), row.lsb_per_g, 1e-9));
        // Full scale must land at the 16-bit limit: 32768 counts = full-scale g.
        CHECK(approx(row.lsb_per_g * row.full_scale_g, 32768.0, 1.0));
        // And the conversion must return that full scale in m/s^2 at the limit.
        const ImuScales scales = imu_scales(row.range, GyroRange::dps2000);
        CHECK(approx(accel_raw_to_mps2(32767, scales),
                     row.full_scale_g * kStandardGravity, 0.01));
    }

    struct GyroRow {
        GyroRange range;
        std::uint8_t bits;
        double lsb_per_dps;
        double full_scale_dps;
    };
    const GyroRow gyro_rows[] = {
        {GyroRange::dps250, 0x00, 131.0, 250.0},
        {GyroRange::dps500, 0x08, 65.5, 500.0},
        {GyroRange::dps1000, 0x10, 32.8, 1000.0},
        {GyroRange::dps2000, 0x18, 16.4, 2000.0},
    };
    for (const GyroRow& row : gyro_rows) {
        CHECK(gyro_range_bits(row.range) == row.bits);
        CHECK(approx(gyro_lsb_per_dps(row.range), row.lsb_per_dps, 1e-9));
        // The datasheet's gyro sensitivities are rounded to three or four significant
        // figures, so full scale lands within a couple of degrees per second of nominal
        // rather than exactly on it.
        CHECK(approx(row.lsb_per_dps * row.full_scale_dps, 32768.0, 40.0));
        const ImuScales scales = imu_scales(AccelRange::g16, row.range);
        CHECK(approx(gyro_raw_to_dps(32767, scales), row.full_scale_dps, 3.0));
    }

    // The bits occupy FS_SEL / AFS_SEL (bits 4:3) and nothing else.
    for (const Row& row : accel_rows) CHECK((accel_range_bits(row.range) & ~0x18) == 0);
    for (const GyroRow& row : gyro_rows) CHECK((gyro_range_bits(row.range) & ~0x18) == 0);

    // The flight configuration's own choice: +-16 g and +-2000 dps, so launch
    // acceleration and spin cannot clip. The plausibility gates must sit outside them.
    flight::Configuration config;
    CHECK(config.accel_clip_mps2 > 16.0 * kStandardGravity);
    CHECK(config.gyro_clip_dps > 2000.0);
}

void test_bmp280_compensation_datasheet_vector() {
    flight::sensors::Bmp280Calib c;
    c.dig_T1 = 27504; c.dig_T2 = 26435; c.dig_T3 = -1000;
    c.dig_P1 = 36477; c.dig_P2 = -10685; c.dig_P3 = 3024;
    c.dig_P4 = 2855; c.dig_P5 = 140; c.dig_P6 = -7;
    c.dig_P7 = 15500; c.dig_P8 = -14600; c.dig_P9 = 6000;

    const auto r = flight::sensors::bmp280_compensate(c, 519888, 415148);
    CHECK(r.valid);
    CHECK(approx(r.temperature_c, 25.08, 0.02));
    CHECK(approx(r.pressure_pa, 100653.0, 5.0));

    // dig_P1 == 0 -> invalid
    c.dig_P1 = 0;
    CHECK(!flight::sensors::bmp280_compensate(c, 519888, 415148).valid);
}

void test_pressure_altitude() {
    using flight::sensors::pressure_altitude_m;
    CHECK(approx(pressure_altitude_m(101325.0, 101325.0), 0.0, 1e-6));
    const double a = pressure_altitude_m(100000.0, 101325.0);
    CHECK(a > 100.0 && a < 125.0);  // ~111 m
    CHECK(pressure_altitude_m(-1.0, 101325.0) == 0.0);
}

// ----------------------------------------------------------------------------
// Deterministic field for a northern-hemisphere site, in the level frame: 40 uT north
// (body +Y at zero yaw) and 17 uT downward (body -Z), total 43.5 uT.
constexpr double kFieldNorthUt = 40.0;
constexpr double kFieldDownUt = 17.0;

// Rotate the level-frame field into the body frame for a given attitude, so a test can
// feed the estimator exactly what a magnetometer would read there. This is R^T applied to
// the level-frame field, written out longhand on purpose: if it shared code with the
// estimator, a sign error in the estimator would cancel itself here.
void body_field(double roll_deg, double pitch_deg, double yaw_deg,
                double& mx, double& my, double& mz) {
    const double d = 3.14159265358979323846 / 180.0;
    const double cr = std::cos(roll_deg * d), sr = std::sin(roll_deg * d);
    const double cp = std::cos(pitch_deg * d), sp = std::sin(pitch_deg * d);
    const double cy = std::cos(yaw_deg * d), sy = std::sin(yaw_deg * d);
    const double ex = 0.0, ey = kFieldNorthUt, ez = -kFieldDownUt;
    // R = Rz(yaw) Ry(pitch) Rx(roll); body = R^T * level.
    const double x1 = cy * ex + sy * ey;
    const double y1 = -sy * ex + cy * ey;
    const double z1 = ez;
    const double x2 = cp * x1 - sp * z1;
    const double y2 = y1;
    const double z2 = sp * x1 + cp * z1;
    mx = x2;
    my = cr * y2 + sr * z2;
    mz = -sr * y2 + cr * z2;
}

// Likewise for the specific force a stationary accelerometer reads at that attitude.
void body_gravity(double roll_deg, double pitch_deg, double& ax, double& ay, double& az) {
    const double d = 3.14159265358979323846 / 180.0;
    const double g = flight::sensors::kStandardGravity;
    const double cr = std::cos(roll_deg * d), sr = std::sin(roll_deg * d);
    const double cp = std::cos(pitch_deg * d), sp = std::sin(pitch_deg * d);
    ax = -g * sp;
    ay = g * sr * cp;
    az = g * cr * cp;
}

void test_orientation_levels_and_yaw() {
    flight::OrientationEstimator est;
    // The first accelerometer sample seeds roll and pitch directly rather than being
    // filtered towards: a vehicle powered up on its side must not spend the first second
    // of its life reporting level.
    est.update(0.0, 0.0, 9.80665, 0.0, 0.0, 0.0, 0.1);
    auto e = est.estimate();
    CHECK(e.valid);
    CHECK(approx(e.roll_deg, 0.0, 0.5));
    CHECK(approx(e.pitch_deg, 0.0, 0.5));
    CHECK(!e.yaw_is_magnetic);  // no magnetometer was supplied

    // Rolled onto its side: gravity now on +Y.
    flight::OrientationEstimator rolled;
    rolled.update(0.0, 9.80665, 0.0, 0.0, 0.0, 0.0, 0.1);
    CHECK(approx(rolled.estimate().roll_deg, 90.0, 1.0));

    // Nose pitched up 30 degrees: gravity leaks onto -X.
    flight::OrientationEstimator pitched;
    double ax = 0.0, ay = 0.0, az = 0.0;
    body_gravity(0.0, 30.0, ax, ay, az);
    pitched.update(ax, ay, az, 0.0, 0.0, 0.0, 0.1);
    CHECK(approx(pitched.estimate().pitch_deg, 30.0, 1.0));

    // Pure gyro yaw propagation with no magnetometer. The first sample seeds the filter
    // rather than propagating it, so three seconds of turn take four updates: 10 deg/s
    // for 3 s -> ~30 deg, relative, and explicitly not claimed as magnetic.
    flight::OrientationEstimator yaw;
    yaw.update(0.0, 0.0, 9.80665, 0.0, 0.0, 10.0, 1.0);  // seeds; yaw stays 0
    CHECK(approx(yaw.estimate().yaw_deg, 0.0, 1e-9));
    for (int i = 0; i < 3; ++i) {
        yaw.update(0.0, 0.0, 9.80665, 0.0, 0.0, 10.0, 1.0);
    }
    CHECK(approx(yaw.estimate().yaw_deg, 30.0, 0.5));
    CHECK(!yaw.estimate().yaw_is_magnetic);

    CHECK(approx(flight::wrap_degrees(190.0), -170.0, 1e-9));
    CHECK(approx(flight::wrap_degrees(-190.0), 170.0, 1e-9));
    CHECK(approx(flight::wrap_degrees_360(-10.0), 350.0, 1e-9));
    CHECK(approx(flight::wrap_degrees_360(370.0), 10.0, 1e-9));
    CHECK(approx(flight::angle_difference_deg(179.0, -179.0), -2.0, 1e-9));
}

// Tilt-compensated magnetic yaw, checked against fields synthesised from known attitudes.
// This is the frame-convention test: if the accelerometer, gyroscope and magnetometer
// were not all in one consistent frame, these would not close.
void test_magnetic_yaw_is_tilt_compensated() {
    for (const double yaw : {0.0, 45.0, 90.0, 179.0, -90.0, -135.0}) {
        for (const double roll : {0.0, 25.0, -40.0}) {
            for (const double pitch : {0.0, 20.0, -30.0}) {
                double ax, ay, az, mx, my, mz;
                body_gravity(roll, pitch, ax, ay, az);
                body_field(roll, pitch, yaw, mx, my, mz);
                double recovered = 0.0;
                CHECK(flight::OrientationEstimator::magnetic_yaw_deg(ax, ay, az, mx, my, mz,
                                                                     recovered));
                CHECK(approx(flight::angle_difference_deg(recovered, yaw), 0.0, 0.5));
            }
        }
    }

    // Zero-length inputs cannot define a heading and must be refused, not fitted.
    double ignored = 0.0;
    CHECK(!flight::OrientationEstimator::magnetic_yaw_deg(0.0, 0.0, 0.0, 1.0, 0.0, 0.0,
                                                          ignored));
    CHECK(!flight::OrientationEstimator::magnetic_yaw_deg(0.0, 0.0, 9.8, 0.0, 0.0, 0.0,
                                                          ignored));
}

// The whole point of the nine-axis upgrade: yaw stops drifting, and it becomes absolute.
void test_orientation_yaw_is_disciplined_by_the_magnetometer() {
    // A vehicle sitting still at a true yaw of 60 degrees, with a gyro that has a
    // 3 deg/s bias on Z the pad calibration never caught.
    const double truth = 60.0;
    double ax, ay, az, mx, my, mz;
    body_gravity(0.0, 0.0, ax, ay, az);
    body_field(0.0, 0.0, truth, mx, my, mz);

    flight::OrientationEstimator fused;
    flight::OrientationEstimator gyro_only;
    for (int i = 0; i < 600; ++i) {  // 20 s at 30 Hz
        fused.update(ax, ay, az, 0.0, 0.0, 3.0, mx, my, mz, true, 1.0 / 30.0);
        gyro_only.update(ax, ay, az, 0.0, 0.0, 3.0, 1.0 / 30.0);
    }

    const auto f = fused.estimate();
    CHECK(f.valid);
    CHECK(f.yaw_is_magnetic);
    // Held to the magnetic reference despite 20 s of biased gyro.
    CHECK(approx(flight::angle_difference_deg(f.yaw_deg, truth), 0.0, 3.0));
    // The bias integrator found the bias that the accelerometer and magnetometer both
    // disagreed with, and reports it with the sign a reader expects.
    CHECK(approx(f.gyro_bias_dps[2], 3.0, 1.0));
    // Heading is the same angle as a compass bearing: clockwise from north.
    CHECK(approx(f.heading_deg, flight::wrap_degrees_360(-f.yaw_deg), 1e-9));

    // The same 20 s without a magnetometer drifts by roughly bias x time away from the
    // arbitrary zero it seeded at, which is the whole reason yaw needed a second sensor.
    const double drifted = gyro_only.estimate().yaw_deg;
    CHECK(std::fabs(flight::angle_difference_deg(drifted, 0.0)) > 30.0);
    CHECK(!gyro_only.estimate().yaw_is_magnetic);
}

// An uncalibrated magnetometer still stops yaw drifting, but the vehicle must not claim
// the result is an absolute magnetic heading.
void test_a_field_with_no_heading_in_it_is_not_seeded_as_one() {
    // The magnitude gate admits any field between kEarthFieldMinUt and kEarthFieldMaxUt.
    // A field of plausible strength pointing straight down the vehicle's up axis -- a
    // magnetic pole, or a local vertical disturbance on the pad -- carries no horizontal
    // component, so no heading can be recovered from it. The estimator used to seed yaw at
    // zero in that case and then set magnetometer confidence to the threshold anyway,
    // reporting an absolute magnetic heading of 0 deg that nothing had measured.
    flight::OrientationEstimator estimator;

    // Level, and a 50 uT field with nothing but a vertical component.
    const double vertical_only[3] = {0.0, 0.0, 50.0};
    estimator.update(0.0, 0.0, 9.81, 0.0, 0.0, 0.0,
                     vertical_only[0], vertical_only[1], vertical_only[2], true, 0.033);
    auto e = estimator.estimate();
    CHECK(e.valid);
    CHECK(!e.yaw_is_magnetic);   // the claim this test exists to prevent

    // The same estimator, given a field that does carry a heading, earns the claim -- it
    // takes kMagConfidenceRequired corrections, so the first sample alone is not enough
    // once the seed no longer grants them.
    flight::OrientationEstimator usable;
    for (int i = 0; i < 10; ++i) {
        usable.update(0.0, 0.0, 9.81, 0.0, 0.0, 0.0, 0.0, 30.0, 40.0, true, 0.033);
    }
    const auto good = usable.estimate();
    CHECK(good.valid);
    CHECK(good.yaw_is_magnetic);

    // And an uncalibrated magnetometer still never claims one, however good the field.
    flight::OrientationEstimator uncalibrated;
    for (int i = 0; i < 10; ++i) {
        uncalibrated.update(0.0, 0.0, 9.81, 0.0, 0.0, 0.0, 0.0, 30.0, 40.0, false, 0.033);
    }
    CHECK(!uncalibrated.estimate().yaw_is_magnetic);
}

void test_uncalibrated_magnetometer_does_not_claim_absolute_heading() {
    double ax, ay, az, mx, my, mz;
    body_gravity(0.0, 0.0, ax, ay, az);
    body_field(0.0, 0.0, 25.0, mx, my, mz);

    flight::OrientationEstimator est;
    for (int i = 0; i < 200; ++i) {
        est.update(ax, ay, az, 0.0, 0.0, 0.0, mx, my, mz, /*mag_calibrated=*/false,
                   1.0 / 30.0);
    }
    const auto e = est.estimate();
    CHECK(e.valid);
    CHECK(!e.yaw_is_magnetic);
    // ...and the yaw it is holding is still the right one; only the claim is withheld.
    CHECK(approx(flight::angle_difference_deg(e.yaw_deg, 25.0), 0.0, 3.0));
}

// A field that is not the earth's must not steer the vehicle: too weak, too strong, or
// simply absent are all reasons to fall back to the gyro rather than to fuse rubbish.
void test_orientation_rejects_an_implausible_field() {
    double ax, ay, az;
    body_gravity(0.0, 0.0, ax, ay, az);

    for (const double scale : {0.1, 10.0}) {  // ~4 uT and ~435 uT
        double mx, my, mz;
        body_field(0.0, 0.0, 90.0, mx, my, mz);
        flight::OrientationEstimator est;
        for (int i = 0; i < 200; ++i) {
            est.update(ax, ay, az, 0.0, 0.0, 0.0, mx * scale, my * scale, mz * scale, true,
                       1.0 / 30.0);
        }
        const auto e = est.estimate();
        CHECK(!e.yaw_is_magnetic);
        // Seeded at zero yaw and never corrected, because the field was never believed.
        CHECK(approx(e.yaw_deg, 0.0, 1.0));
    }
}

// Under boost the accelerometer measures thrust, not gravity. Correcting attitude towards
// it would tip the solution towards the thrust axis for the whole powered phase.
void test_orientation_ignores_the_accelerometer_under_high_g() {
    flight::OrientationEstimator est;
    est.update(0.0, 0.0, 9.80665, 0.0, 0.0, 0.0, 1.0 / 30.0);  // seed level
    for (int i = 0; i < 100; ++i) {
        // 6 g straight along +X: a gravity-following filter would roll onto its side.
        est.update(6.0 * 9.80665, 0.0, 0.0, 0.0, 0.0, 0.0, 1.0 / 30.0);
    }
    const auto e = est.estimate();
    CHECK(approx(e.roll_deg, 0.0, 1.0));
    CHECK(approx(e.pitch_deg, 0.0, 1.0));
}

// ----------------------------------------------------------------------------
void test_gps_parser() {
    flight::NmeaParser p;
    const std::string gga =
        "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    for (char c : gga) p.consume(c);
    CHECK(p.has_fix());
    CHECK(approx(p.latest().latitude, 48.1173, 0.01));
    CHECK(approx(p.latest().longitude, 11.5167, 0.01));
    CHECK(approx(p.latest().altitude, 545.4, 0.1));
    CHECK(p.latest().satellites == 8);
    CHECK(p.checksum_errors() == 0);

    // Corrupted checksum: counted, fix unchanged.
    flight::NmeaParser bad;
    const std::string bad_gga =
        "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*00\r\n";
    for (char c : bad_gga) bad.consume(c);
    CHECK(!bad.has_fix());
    CHECK(bad.checksum_errors() == 1);

    // No-fix GGA (quality 0) clears the fix without error.
    flight::NmeaParser nofix;
    const std::string zero =
        "$GPGGA,123519,,,,,0,00,,,M,,M,,*48\r\n";
    for (char c : zero) nofix.consume(c);
    CHECK(!nofix.has_fix());

    // RMC provides UTC time.
    flight::NmeaParser rmc;
    const std::string rmc_line =
        "$GPRMC,081836,A,3751.65,S,14507.36,E,000.0,360.0,130998,011.3,E*62\r\n";
    for (char c : rmc_line) rmc.consume(c);
    CHECK(rmc.latest().time_valid);
    CHECK(approx(rmc.latest().time_of_day_s, 8 * 3600 + 18 * 60 + 36, 0.001));

    // Overlong garbage line must not crash or set a fix.
    flight::NmeaParser flood;
    for (int i = 0; i < 5000; ++i) flood.consume('X');
    flood.consume('\n');
    CHECK(!flood.has_fix());
}

// ----------------------------------------------------------------------------
void test_scheduler() {
    flight::PeriodicTask t(100, 0);
    CHECK(t.due(0));
    CHECK(!t.due(0));
    CHECK(!t.due(50));
    CHECK(t.due(100));
    CHECK(t.due(250));
    CHECK(!t.due(260));
    CHECK(t.due(300));

    // Catch-up: a long stall fires once, then re-anchors.
    flight::PeriodicTask s(100, 0);
    CHECK(s.due(0));
    CHECK(s.due(10000));
    CHECK(!s.due(10050));
    CHECK(s.due(10100));

    flight::PeriodicTask disabled(0, 0);
    CHECK(!disabled.due(1000000));
}

// ----------------------------------------------------------------------------
void test_fault_manager() {
    flight::FaultManager fm;
    CHECK(!fm.has_critical());
    fm.report(flight::FaultCode::imu_stale, flight::FaultSeverity::error, 10);
    fm.report(flight::FaultCode::imu_stale, flight::FaultSeverity::error, 20);
    CHECK(fm.active(flight::FaultCode::imu_stale));
    CHECK(fm.record(flight::FaultCode::imu_stale).occurrences == 2);
    CHECK(fm.record(flight::FaultCode::imu_stale).first_ms == 10);
    CHECK(fm.record(flight::FaultCode::imu_stale).last_ms == 20);

    fm.clear(flight::FaultCode::imu_stale);
    CHECK(!fm.active(flight::FaultCode::imu_stale));

    fm.report(flight::FaultCode::config_invalid, flight::FaultSeverity::critical, 30);
    CHECK(fm.has_critical());
    CHECK(fm.ever_critical());
    fm.clear(flight::FaultCode::config_invalid);
    CHECK(!fm.has_critical());
    CHECK(fm.ever_critical());
    CHECK(fm.total_occurrences() == 3);
}

// ----------------------------------------------------------------------------
flight::Configuration fast_state_config() {
    flight::Configuration c;
    c.team_id = "CAN-Team-07";
    c.launch_accel_mps2 = 30.0;
    c.launch_altitude_gain_m = 15.0;
    c.launch_confirm_ms = 100;
    c.min_flight_ms = 200;
    c.landing_accel_epsilon_mps2 = 2.5;
    c.landing_altitude_rate_max_mps = 1.0;
    c.landing_confirm_ms = 200;
    // Scaled with the rest. The gate itself is exercised at its real value by
    // test_a_hovering_drone_is_not_a_landing().
    c.landing_descent_confirm_ms = 50;
    c.post_impact_transmission_ms = 5000;
    return c;
}

void test_state_machine_full_mission() {
    const flight::Configuration c = fast_state_config();
    flight::StateMachine sm(c);
    CHECK(sm.state() == flight::MissionState::init);

    sm.begin_self_test(0);
    CHECK(sm.state() == flight::MissionState::self_test);

    flight::DetectionInputs di;
    di.self_test_ok = true;
    sm.update(0, di);
    CHECK(sm.state() == flight::MissionState::ready);

    // Not armed yet: boost is ignored by the launch lockout.
    di.accel_magnitude_mps2 = 60.0;
    sm.update(5, di);
    CHECK(sm.state() == flight::MissionState::ready);

    // Armed: sustained boost -> FLIGHT after launch_confirm_ms.
    di.armed = true;
    sm.update(10, di);
    CHECK(sm.state() == flight::MissionState::ready);
    sm.update(150, di);
    CHECK(sm.state() == flight::MissionState::flight);

    // Coast: not at rest yet, and descending hard enough to open the descent gate.
    di.accel_magnitude_mps2 = 5.0;
    di.altitude_rate_mps = -20.0;
    sm.update(300, di);
    CHECK(sm.state() == flight::MissionState::flight);
    CHECK(!sm.descent_observed());          // one sample is not a descent
    sm.update(360, di);                     // held past landing_descent_confirm_ms
    CHECK(sm.descent_observed());

    // At rest past min_flight_ms, held for landing_confirm_ms -> LANDED.
    di.accel_magnitude_mps2 = flight::sensors::kStandardGravity;
    di.altitude_rate_mps = 0.1;
    sm.update(400, di);
    CHECK(sm.state() == flight::MissionState::flight);
    sm.update(650, di);
    CHECK(sm.state() == flight::MissionState::landed);
    CHECK(sm.post_impact_window_active(1000));

    // After the post-impact window -> RECOVERY.
    sm.update(650 + 5000, di);
    CHECK(sm.state() == flight::MissionState::recovery);
    CHECK(!sm.post_impact_window_active(650 + 5000));
}

// ----------------------------------------------------------------------------
// F-20. "At rest" -- about 1 g, and no vertical motion -- is also a perfect description of
// a vehicle hanging under a hovering drone. Before the descent gate this declared a
// landing at 18.0 s of a lift with a 20 s hover: twelve seconds before release, after
// which the whole real descent happened in RECOVERY and the mandatory post-impact window
// had already been spent in the air.
//
// The profile below is the real one, at the real thresholds -- not a scaled-down test
// config -- because the numbers that made this fire are the flight numbers.
void test_a_hovering_drone_is_not_a_landing() {
    flight::Configuration c;
    c.team_id = "CAN-Team-07";
    flight::StateMachine sm(c);

    flight::DetectionInputs di;
    di.self_test_ok = true;
    di.sensors_ok = true;
    sm.begin_self_test(0);
    sm.update(0, di);
    CHECK(sm.state() == flight::MissionState::ready);

    // A drone lift: three seconds on the pad, a 3 m/s climb to 30 m, then a 20 s hover
    // before the release. The vehicle is armed once the arming delay has elapsed.
    auto lift_sample = [&](std::uint64_t ms) {
        const double t = ms / 1000.0;
        di.armed = ms >= c.arming_delay_ms;
        di.accel_magnitude_mps2 = flight::sensors::kStandardGravity;
        if (t < 5.0) {                       // on the ground
            di.altitude_agl_m = 0.0;
            di.altitude_rate_mps = 0.0;
        } else if (t < 15.0) {               // climbing at 3 m/s
            di.altitude_agl_m = 3.0 * (t - 5.0);
            di.altitude_rate_mps = 3.0;
        } else {                             // hovering at 30 m
            di.altitude_agl_m = 30.0;
            di.altitude_rate_mps = 0.0;
        }
    };

    bool entered_flight = false;
    std::uint64_t left_flight_at = 0;
    flight::MissionState left_for = flight::MissionState::init;
    for (std::uint64_t ms = 0; ms <= 35000; ms += 33) {
        lift_sample(ms);
        sm.update(ms, di);
        if (sm.state() == flight::MissionState::flight) entered_flight = true;
        const bool ok = sm.state() == flight::MissionState::ready ||
                        sm.state() == flight::MissionState::flight;
        if (!ok && left_flight_at == 0) {
            left_flight_at = ms;
            left_for = sm.state();
        }
    }
    // The whole assertion, made once: through the climb and a twenty-second hover the
    // vehicle never leaves FLIGHT. It used to reach LANDED at 18.0 s and RECOVERY at
    // 23.0 s. Recording *when* it went wrong is the useful half of the failure.
    CHECK(left_flight_at == 0);
    if (left_flight_at != 0) {
        std::cerr << "  left FLIGHT at " << left_flight_at << " ms for "
                  << flight::to_string(left_for) << "\n";
    }
    // ...and it did reach FLIGHT, or the test above proves nothing. The altitude
    // condition fires during the ascent, which is correct: MIS-004 wants telemetry to
    // reflect the climb.
    CHECK(entered_flight);
    CHECK(sm.state() == flight::MissionState::flight);
    CHECK(!sm.descent_observed());

    // Release. The canopy takes load and the vehicle descends at the rulebook cap.
    bool landed_mid_descent = false;
    for (std::uint64_t ms = 35000; ms <= 41500; ms += 33) {
        di.altitude_rate_mps = -5.0;
        di.altitude_agl_m = 30.0 - 5.0 * ((ms - 35000) / 1000.0);
        if (di.altitude_agl_m < 0.0) di.altitude_agl_m = 0.0;
        sm.update(ms, di);
        if (sm.state() != flight::MissionState::flight) landed_mid_descent = true;
    }
    CHECK(!landed_mid_descent);
    // A second into that descent the gate is open, with five seconds of the 6.45 s
    // descent still to run.
    CHECK(sm.descent_observed());

    // Touchdown, and now the same at-rest condition that the hover produced does mean a
    // landing.
    for (std::uint64_t ms = 41500; ms <= 45000; ms += 33) {
        di.altitude_rate_mps = 0.0;
        di.altitude_agl_m = 0.0;
        sm.update(ms, di);
    }
    CHECK(sm.state() == flight::MissionState::landed);
    // And the post-impact window is spent on the ground, where the rulebook wants it.
    CHECK(sm.post_impact_window_active(45100));
}

// The exposure is wider than hovering. Any run of samples with a vertical rate under
// landing_altitude_rate_max_mps satisfies the at-rest test, which includes a lift gentle
// enough to stay below it -- so a slow ascent must not read as a landing either.
void test_a_lift_slower_than_the_rest_threshold_is_not_a_landing() {
    flight::Configuration c;
    c.team_id = "CAN-Team-07";
    flight::StateMachine sm(c);

    flight::DetectionInputs di;
    di.self_test_ok = true;
    di.sensors_ok = true;
    sm.begin_self_test(0);
    sm.update(0, di);

    // Climb at 0.8 m/s: below landing_altitude_rate_max_mps (1.0), so every sample of it
    // reads as "not descending" to the at-rest test. Forty seconds of it.
    std::uint64_t declared_at = 0;
    for (std::uint64_t ms = 0; ms <= 45000; ms += 33) {
        di.armed = ms >= c.arming_delay_ms;
        di.accel_magnitude_mps2 = flight::sensors::kStandardGravity;
        di.altitude_rate_mps = 0.8;
        di.altitude_agl_m = 0.8 * (ms / 1000.0);
        sm.update(ms, di);
        if (declared_at == 0 && (sm.state() == flight::MissionState::landed ||
                                 sm.state() == flight::MissionState::recovery)) {
            declared_at = ms;
        }
    }
    CHECK(declared_at == 0);
    if (declared_at != 0) {
        std::cerr << "  declared a landing during a 0.8 m/s climb at " << declared_at
                  << " ms\n";
    }
    CHECK(sm.state() == flight::MissionState::flight);
    CHECK(!sm.descent_observed());
}

// The gate belongs to one FLIGHT. A descent observed before this state began -- the state
// machine having been driven through an earlier profile -- must not authorise a landing
// inside it.
void test_the_descent_gate_does_not_survive_a_state_change() {
    const flight::Configuration c = fast_state_config();
    flight::StateMachine sm(c);
    flight::DetectionInputs di;
    di.self_test_ok = true;
    sm.begin_self_test(0);
    sm.update(0, di);
    di.armed = true;

    // Launch, descend enough to open the gate, then land.
    di.accel_magnitude_mps2 = 60.0;
    sm.update(10, di);
    sm.update(150, di);
    CHECK(sm.state() == flight::MissionState::flight);
    di.accel_magnitude_mps2 = 5.0;
    di.altitude_rate_mps = -20.0;
    sm.update(300, di);
    sm.update(400, di);
    CHECK(sm.descent_observed());

    // A critical fault forces FAULT from FLIGHT, which is a state change.
    di.critical_fault = true;
    sm.update(500, di);
    CHECK(sm.state() == flight::MissionState::fault);
    CHECK(!sm.descent_observed());
}

// A single noisy barometer sample must not open the gate. The confirm window is what
// separates a descent from a spike, and validate_config() refuses to run without one.
void test_one_descending_sample_does_not_open_the_descent_gate() {
    flight::Configuration c = fast_state_config();
    c.landing_descent_confirm_ms = 500;
    flight::StateMachine sm(c);
    flight::DetectionInputs di;
    di.self_test_ok = true;
    sm.begin_self_test(0);
    sm.update(0, di);
    di.armed = true;
    di.accel_magnitude_mps2 = 60.0;
    sm.update(10, di);
    sm.update(150, di);
    CHECK(sm.state() == flight::MissionState::flight);

    di.accel_magnitude_mps2 = flight::sensors::kStandardGravity;
    // A spike every second, never held. The run restarts each time.
    bool gate_opened = false;
    for (std::uint64_t ms = 200; ms <= 6000; ms += 100) {
        di.altitude_rate_mps = (ms % 1000 == 0) ? -9.0 : 0.0;
        sm.update(ms, di);
        if (sm.descent_observed()) gate_opened = true;
    }
    CHECK(!gate_opened);
    // ...and with nothing to authorise it, no landing is declared however long the vehicle
    // sits at rest.
    CHECK(sm.state() == flight::MissionState::flight);
}

// The two thresholds read the same quantity from opposite ends. If the rate that counts as
// descending were at or below the rate that counts as stopped, one sample could satisfy
// both -- exactly the confusion the gate exists to remove.
void test_the_descent_and_rest_thresholds_may_not_overlap() {
    std::string why;
    flight::Configuration c;
    c.team_id = "CAN-Team-07";
    CHECK(flight::validate_config(c, why));

    c.landing_descent_rate_mps = c.landing_altitude_rate_max_mps;
    CHECK(!flight::validate_config(c, why));
    CHECK(why.find("landing_descent_rate_mps") != std::string::npos);

    c.landing_descent_rate_mps = c.landing_altitude_rate_max_mps - 0.1;
    CHECK(!flight::validate_config(c, why));

    c.landing_descent_rate_mps = 2.0;
    CHECK(flight::validate_config(c, why));

    c.landing_descent_confirm_ms = 0;
    CHECK(!flight::validate_config(c, why));
    CHECK(why.find("landing_descent_confirm_ms") != std::string::npos);
}

// The rulebook's 1 Hz is a floor, and this vehicle is built to be above it rather than on
// it. Three things enforce that and they must agree with each other: the profile's own
// constant, the ceiling, and what validate_config() will accept.
void test_the_link_profile_is_compulsorily_faster_than_1_hz() {
    // The ceiling is below the rulebook period, not equal to it.
    CHECK(cansat::link::kMaxTelemetryPeriodMs < cansat::link::kRulebookMinRatePeriodMs);
    CHECK(cansat::link::kRulebookMinRatePeriodMs - cansat::link::kMaxTelemetryPeriodMs ==
          cansat::link::kTelemetryJitterMarginMs);
    // The shipped profile is inside it, and comfortably: this is the value a flight build
    // actually carries.
    CHECK(cansat::link::kTelemetryPeriodMs <= cansat::link::kMaxTelemetryPeriodMs);
    CHECK(cansat::link::kTelemetryPeriodMs == 700);

    // The default Configuration takes it from the profile rather than repeating it, so a
    // vehicle built with no explicit period is already above 1 Hz.
    flight::Configuration c;
    c.team_id = "CAN-Team-07";
    CHECK(c.telemetry_period_ms == cansat::link::kTelemetryPeriodMs);
    const double rate_hz = 1000.0 / static_cast<double>(c.telemetry_period_ms);
    CHECK(rate_hz > 1.0);
    CHECK(rate_hz > 1.4);

    std::string why;
    CHECK(flight::validate_config(c, why));

    // Every period at or above the rulebook figure is refused, including the figure
    // itself. A vehicle cannot be configured onto the line.
    for (std::uint32_t period : {1000u, 1001u, 1500u, 2000u, 60000u}) {
        c.telemetry_period_ms = period;
        CHECK(!flight::validate_config(c, why));
    }
    // Nor can it be configured just under the ceiling and just over it.
    c.telemetry_period_ms = cansat::link::kMaxTelemetryPeriodMs;
    CHECK(flight::validate_config(c, why));
    c.telemetry_period_ms = cansat::link::kMaxTelemetryPeriodMs + 1;
    CHECK(!flight::validate_config(c, why));
}

// The controller schedules from the configured period, so the guard above is only worth
// having if the packets actually come out at that spacing. Fly a mission and measure the
// gaps between transmitted packets.
void test_a_default_vehicle_transmits_faster_than_1_hz() {
    flight::Configuration c;
    c.team_id = "CAN-Team-07";
    c.require_calibration_to_arm = false;
    c.calib_samples = 4;
    flight::test::MockImu imu;
    flight::test::MockBarometer baro;
    flight::test::MockGps gps;
    flight::test::MockRadio radio;
    flight::test::MockLogger logger;
    flight::test::MockBoard board;
    flight::Controller ctrl(c, imu, baro, gps, radio, logger, board);
    CHECK(ctrl.initialize());

    std::vector<std::uint64_t> sent_at;
    std::size_t last = 0;
    for (std::uint64_t ms = 0; ms <= 30000; ms += 2) {
        ctrl.poll(ms);
        if (radio.packets.size() != last) {
            last = radio.packets.size();
            sent_at.push_back(ms);
        }
    }
    CHECK(sent_at.size() > 20);

    // No interval may reach a second, and the average rate must clear 1 Hz with the
    // margin the profile promises.
    std::uint64_t worst_gap = 0;
    for (std::size_t i = 1; i < sent_at.size(); ++i) {
        const std::uint64_t gap = sent_at[i] - sent_at[i - 1];
        CHECK(gap == c.telemetry_period_ms);
        if (gap > worst_gap) worst_gap = gap;
    }
    CHECK(worst_gap < cansat::link::kRulebookMinRatePeriodMs);
    const double span_s = (sent_at.back() - sent_at.front()) / 1000.0;
    const double measured_hz = (sent_at.size() - 1) / span_s;
    CHECK(measured_hz > 1.0);
    if (!(measured_hz > 1.0)) {
        std::cerr << "  measured " << measured_hz << " Hz over " << span_s << " s\n";
    }
}

void test_state_machine_fault_paths() {
    const flight::Configuration c = fast_state_config();

    flight::StateMachine sm(c);
    sm.begin_self_test(0);
    flight::DetectionInputs di;
    di.self_test_ok = false;
    sm.update(0, di);
    CHECK(sm.state() == flight::MissionState::fault);  // failed self-test

    flight::StateMachine sm2(c);
    sm2.begin_self_test(0);
    di.self_test_ok = true;
    sm2.update(0, di);
    CHECK(sm2.state() == flight::MissionState::ready);
    di.critical_fault = true;
    sm2.update(10, di);
    CHECK(sm2.state() == flight::MissionState::fault);  // critical fault from any state
}

// ----------------------------------------------------------------------------
void test_a_refused_configuration_says_which_setting_was_wrong() {
    // validate_config() has thirty ways to refuse and names the one that applied. The
    // controller used to replace that with "configuration invalid", leaving an operator
    // with a vehicle that will not fly and thirty candidates for why.
    flight::test::MockImu imu;
    flight::test::MockBarometer baro;
    flight::test::MockGps gps;
    flight::test::MockRadio radio;
    flight::test::MockLogger logger;
    flight::test::MockBoard board;

    flight::Configuration bad;
    bad.team_id = "CAN-Team-07";
    bad.post_impact_transmission_ms = 1000;  // rulebook minimum is 5000
    flight::Controller refused(bad, imu, baro, gps, radio, logger, board);
    CHECK(!refused.initialize());
    const std::string reason = refused.config_error();
    CHECK(!reason.empty());
    CHECK(reason.find("post_impact_transmission_ms") != std::string::npos);

    // A different rule gives a different reason, not the same generic string.
    flight::Configuration other;
    other.team_id = "CAN-Team-07";
    other.radio.spreading_factor = 3;  // outside 6..12
    flight::Controller refused_two(other, imu, baro, gps, radio, logger, board);
    CHECK(!refused_two.initialize());
    const std::string second = refused_two.config_error();
    CHECK(second.find("spreading_factor") != std::string::npos);
    CHECK(second != reason);

    // A configuration that is accepted leaves it empty rather than stale.
    flight::Configuration good;
    good.team_id = "CAN-Team-07";
    flight::Controller accepted(good, imu, baro, gps, radio, logger, board);
    CHECK(accepted.initialize());
    CHECK(std::string(accepted.config_error()).empty());
}

void test_config_validation() {
    std::string why;
    flight::Configuration c;
    CHECK(!flight::validate_config(c, why));  // placeholder team id

    c.team_id = "CAN-Team-07";
    CHECK(flight::validate_config(c, why));
    CHECK(why.empty());

    c.telemetry_period_ms = 2000;  // slower than the 1 Hz minimum
    CHECK(!flight::validate_config(c, why));
    // Exactly 1 Hz is refused too, and that is the point of the guard rather than an
    // off-by-one: the rulebook figure is a floor, and a vehicle sitting on it goes below
    // it on the first millisecond of jitter.
    c.telemetry_period_ms = cansat::link::kRulebookMinRatePeriodMs;
    CHECK(!flight::validate_config(c, why));
    CHECK(why.find("strictly faster") != std::string::npos);
    // One millisecond over the ceiling is refused; the ceiling itself is accepted.
    c.telemetry_period_ms = cansat::link::kMaxTelemetryPeriodMs + 1;
    CHECK(!flight::validate_config(c, why));
    c.telemetry_period_ms = cansat::link::kMaxTelemetryPeriodMs;
    CHECK(flight::validate_config(c, why));
    // And zero, which is neither fast nor slow but a stopped scheduler.
    c.telemetry_period_ms = 0;
    CHECK(!flight::validate_config(c, why));
    c.telemetry_period_ms = cansat::link::kTelemetryPeriodMs;

    c.post_impact_transmission_ms = 3000;  // below rulebook 5 s
    CHECK(!flight::validate_config(c, why));
    c.post_impact_transmission_ms = 5000;
    CHECK(flight::validate_config(c, why));

    c.orientation_kp_accel = -1.0;
    CHECK(!flight::validate_config(c, why));
    c.orientation_kp_accel = 2.0;

    // Both proportional gains zero would leave attitude a free-running integration with
    // no reference at all, which is not a configuration anyone means to fly.
    c.orientation_kp_accel = 0.0;
    c.orientation_kp_mag = 0.0;
    CHECK(!flight::validate_config(c, why));
    c.orientation_kp_accel = 2.0;
    c.orientation_kp_mag = 0.6;

    c.orientation_bias_limit_dps = 0.0;
    CHECK(!flight::validate_config(c, why));
    c.orientation_bias_limit_dps = 10.0;

    // A magnetometer mode slower than the acquisition rate cannot feed every update.
    c.mag_mode = flight::sensors::MagMode::continuous_8hz;
    CHECK(!flight::validate_config(c, why));
    CHECK(why.find("magnetometer") != std::string::npos);
    c.mag_mode = flight::sensors::MagMode::continuous_100hz;
    CHECK(flight::validate_config(c, why));

    c.yaw_cog_tolerance_deg = 0.0;
    CHECK(!flight::validate_config(c, why));
    c.yaw_cog_tolerance_deg = 60.0;
    CHECK(flight::validate_config(c, why));

    c.reference_pressure_pa = 0.0;
    CHECK(!flight::validate_config(c, why));
    c.reference_pressure_pa = 101325.0;
    CHECK(flight::validate_config(c, why));
}

// The airtime guard. A telemetry period the radio cannot physically sustain must be
// rejected on the pad rather than silently under-running in flight.
void test_config_radio_airtime_guard() {
    std::string why;
    flight::Configuration c;
    c.team_id = "CAN-Team-07";

    // Default: SF7/125 kHz, GPS logged rather than transmitted, so a 199-byte worst case
    // at a 700 ms period -> ~318 ms airtime, ~45 % duty. The period is set from the
    // *measured* airtime rather than this model, so the check that matters is below.
    CHECK(flight::validate_config(c, why));
    CHECK(!c.transmit_gps);
    CHECK(c.worst_case_packet_bytes == 199);
    CHECK(c.worst_case_packet_bytes < cansat::kMaxLoraPayloadBytes);
    CHECK(approx(flight::worst_case_airtime_ms(c), 317.7, 0.5));
    CHECK(flight::channel_duty(c) <= c.max_channel_duty);
    CHECK(approx(flight::channel_duty(c), 0.454, 0.005));

    // The real one. Bring-up rows 5.2 and 5.3 measured 406.9 ms for a full-length packet,
    // 1.8 % above the model, twice, on two different boards. A period sized from the model
    // alone would put the true duty over the policy while every test still passed.
    {
        // 406.9 ms was measured for a 255-byte packet, 1.8 % above that packet's model
        // figure. The same 1.8 % applied to the 199-byte model figure is the honest
        // estimate for the packet this configuration actually sends.
        const double measured_airtime_ms = flight::worst_case_airtime_ms(c) * 1.018;
        const double true_duty = measured_airtime_ms / c.telemetry_period_ms;
        CHECK(true_duty <= c.max_channel_duty);
        if (true_duty > c.max_channel_duty) {
            std::cerr << "  measured duty " << true_duty << " exceeds the "
                      << c.max_channel_duty << " policy at a "
                      << c.telemetry_period_ms << " ms period\n";
        }
    }

    // And it still clears the rulebook minimum with margin, rather than sitting on it.
    CHECK(c.telemetry_period_ms < 1000);

    // The former default of SF9 puts one packet at ~1 s of airtime: not sustainable.
    c.radio.spreading_factor = 9;
    CHECK(!flight::validate_config(c, why));
    CHECK(why.find("airtime") != std::string::npos);
    CHECK(flight::worst_case_airtime_ms(c) > 900.0);

    // SF7 at 2 Hz over 125 kHz is also refused: 400 ms in a 500 ms slot is 80 % duty.
    c.radio.spreading_factor = 7;
    c.telemetry_period_ms = 500;
    CHECK(!flight::validate_config(c, why));

    // Doubling the bandwidth halves the airtime and makes 2 Hz legitimate.
    c.radio.bandwidth_hz = 250000;
    CHECK(flight::validate_config(c, why));
    CHECK(flight::channel_duty(c) < 0.45);

    // Parameter-range rejections.
    flight::Configuration bad;
    bad.team_id = "CAN-Team-07";
    bad.radio.spreading_factor = 13;
    CHECK(!flight::validate_config(bad, why));
    bad.radio.spreading_factor = 7;
    bad.radio.coding_rate = 9;
    CHECK(!flight::validate_config(bad, why));
    bad.radio.coding_rate = 5;
    bad.radio.preamble_length = 4;
    CHECK(!flight::validate_config(bad, why));
    bad.radio.preamble_length = 8;
    bad.worst_case_packet_bytes = 256;  // beyond the LoRa FIFO
    CHECK(!flight::validate_config(bad, why));
    bad.worst_case_packet_bytes = 255;
    bad.max_channel_duty = 0.0;
    CHECK(!flight::validate_config(bad, why));
}

// The formatter must never emit a packet this library's own parser rejects.
void test_a_value_too_wide_to_format_invalidates_the_packet() {
    // Every mandatory field is finite-checked before formatting, but finite is not the
    // same as representable: "%.2f" of 1e300 is over three hundred characters. The
    // formatter used to return the first 63 of them -- a long digit string with no
    // decimal point, which is a corrupted reading wearing the shape of a reading. Both
    // behaviours end in a rejected packet, so the difference only shows in what the
    // packet carries: a field that says nothing, rather than one that says something
    // false.
    cansat::TelemetryRecord r = make_valid_record();
    r.pressure_pa = 1e300;
    const auto packet = cansat::format_packet(r);
    CHECK(packet.has_value());
    if (packet) {
        CHECK(packet->find("; Pr-;") != std::string::npos);
        CHECK(packet->find("Pr-1000000") == std::string::npos);
        CHECK(!cansat::parse_packet(*packet));
    }

    r.pressure_pa = -1e300;
    const auto negative = cansat::format_packet(r);
    CHECK(negative.has_value());
    if (negative) {
        CHECK(negative->find("; Pr-;") != std::string::npos);
        CHECK(!cansat::parse_packet(*negative));
    }

    // A pressure a barometer could actually report still formats and still round-trips.
    r.pressure_pa = 101325.25;
    const auto ordinary = cansat::format_packet(r);
    CHECK(ordinary.has_value());
    CHECK(ordinary->find("Pr-101325.25;") != std::string::npos);
    CHECK(cansat::parse_packet(*ordinary));
}

void test_formatter_and_parser_agree_at_the_edges() {
    // Timestamp: the hour field is two digits, so it wraps at 100 hours rather than
    // widening. Anything else produces a packet every ground station rejects.
    CHECK(cansat::format_timestamp(0) == "00:00:00:000");
    CHECK(cansat::format_timestamp(1) == "00:00:00:001");
    CHECK(cansat::format_timestamp(999) == "00:00:00:999");
    CHECK(cansat::format_timestamp(1000) == "00:00:01:000");
    CHECK(cansat::format_timestamp(59999) == "00:00:59:999");
    CHECK(cansat::format_timestamp(60000) == "00:01:00:000");
    CHECK(cansat::format_timestamp(3600000) == "01:00:00:000");
    // 99:59:59:999 is the last representable instant.
    const std::uint64_t last = (99ULL * 3600 + 59 * 60 + 59) * 1000 + 999;
    CHECK(cansat::format_timestamp(last) == "99:59:59:999");
    CHECK(cansat::format_timestamp(last + 1) == "00:00:00:000");  // wraps, never 100:...

    // Every timestamp the formatter can produce must survive its own parser.
    auto record = make_valid_record();
    // The list is typed rather than deduced. std::uint64_t is unsigned long long on
    // Windows but unsigned long on 64-bit Linux, so a braced list mixing ULL literals
    // with std::uint64_t values deduces two different types for the same element type
    // and fails to compile there -- while looking perfectly correct here.
    for (const std::uint64_t t : std::initializer_list<std::uint64_t>{
             0, 1, 999, 3600000, last, last + 1, 1234567890}) {
        record.timestamp_ms = t;
        const auto packet = cansat::format_packet(record);
        CHECK(packet.has_value());
        CHECK(static_cast<bool>(cansat::parse_packet(*packet)));
    }

    // Packet numbers: the rulebook pads to three digits, and wider numbers stay valid.
    for (const std::uint32_t n : {1u, 9u, 99u, 100u, 999u, 1000u, 65535u, 4294967295u}) {
        record.packet_number = n;
        const auto packet = cansat::format_packet(record);
        CHECK(packet.has_value());
        const auto parsed = cansat::parse_packet(*packet);
        CHECK(static_cast<bool>(parsed));
        CHECK(parsed.record->packet_number == n);
    }

    // Values that round to a signed zero must still satisfy the precision rule.
    record.packet_number = 1;
    record.roll_deg = -0.001;
    const auto negative_zero = cansat::format_packet(record);
    CHECK(negative_zero.has_value());
    CHECK(static_cast<bool>(cansat::parse_packet(*negative_zero)));
}

// A packet longer than the airtime budget must lose its optional fields, not be truncated
// by the radio into something the ground station can only read as corruption.
void test_controller_drops_optional_fields_before_overrunning_the_budget() {
    flight::Configuration c;
    c.team_id = "CAN-Team-07";
    c.health_period_ms = 50;
    // A budget the 118-byte mandatory block fits inside, but GPS and diagnostics do not.
    c.worst_case_packet_bytes = 140;
    flight::test::MockImu imu;
    flight::test::MockBarometer baro;
    flight::test::MockGps gps;
    flight::test::MockRadio radio;
    flight::test::MockLogger logger;
    flight::test::MockBoard board;
    flight::Controller ctrl(c, imu, baro, gps, radio, logger, board);
    CHECK(ctrl.initialize());
    ctrl.poll(0);

    CHECK(radio.packets.size() == 1);
    const std::string& sent = radio.packets.front();
    CHECK(sent.size() <= 140);
    CHECK(sent.find("MODE-") == std::string::npos);    // diagnostics dropped first
    CHECK(sent.find("FAULTS-") == std::string::npos);
    CHECK(sent.find("GP-Lat-") == std::string::npos);  // then GPS, also optional
    CHECK(ctrl.faults().active(flight::FaultCode::packet_oversize));

    // The packet is still a valid, compliant, parseable telemetry point.
    const auto parsed = cansat::parse_packet(sent);
    CHECK(static_cast<bool>(parsed));
    CHECK(parsed.record->packet_number == 1);

    // With the default budget — the full FIFO — the diagnostics survive.
    flight::Configuration d;
    d.team_id = "CAN-Team-07";
    flight::test::MockRadio radio2;
    flight::Controller ctrl2(d, imu, baro, gps, radio2, logger, board);
    CHECK(ctrl2.initialize());
    ctrl2.poll(0);
    CHECK(radio2.packets.size() == 1);
    CHECK(radio2.packets.front().find("MODE-") != std::string::npos);
    CHECK(radio2.packets.front().size() <= d.worst_case_packet_bytes);
    CHECK(!ctrl2.faults().active(flight::FaultCode::packet_oversize));
}

// Feeds a whole sentence to a parser and reports whether it produced a usable fix.
bool feed_nmea(flight::NmeaParser& parser, const std::string& sentence) {
    for (const char c : sentence) parser.consume(c);
    return parser.has_fix();
}

// Wraps an NMEA body in its "$", checksum and terminator, so a test states the sentence it
// means rather than a checksum somebody has to recompute by hand when a field changes.
std::string nmea(const std::string& body) {
    std::uint8_t checksum = 0;
    for (const char c : body) checksum ^= static_cast<std::uint8_t>(c);
    char tail[8];
    std::snprintf(tail, sizeof(tail), "*%02X", static_cast<unsigned>(checksum));
    return "$" + body + tail + "\r\n";
}

// [F-18] A receiver that never moved produced fixes 55.6 m apart in one second, and an
// altitude spanning 49.8 m on a bench. The parser accepted every one of them: it rejected
// only quality <= 0, then stored the satellite count without ever testing it. Fix *quality*
// was parsed and discarded, so a 4-satellite fix with poor geometry was carried into
// telemetry exactly like a 12-satellite one.
void test_a_fix_with_too_few_satellites_is_refused() {
    flight::NmeaParser parser;
    // Three satellites cannot produce a 3D fix, and the receiver still reports quality 1.
    CHECK(!feed_nmea(parser, nmea("GPGGA,120000.00,2110.0000,N,07246.9980,E,1,03,1.0,15.0,M,0.0,M,,")));
    CHECK(parser.fixes_rejected() == 1);
    CHECK(!parser.latest().valid);   // and it must not invent a position either
}

void test_a_fix_with_poor_geometry_is_refused() {
    flight::NmeaParser parser;
    // Eight satellites, but an HDOP of 20 -- all bunched in one part of the sky. This is
    // the case that produces a large, confident, wrong position.
    CHECK(!feed_nmea(parser, nmea("GPGGA,120000.00,2110.0000,N,07246.9980,E,1,08,20.0,15.0,M,0.0,M,,")));
    CHECK(parser.fixes_rejected() == 1);
}

void test_a_good_fix_still_passes_and_carries_its_quality() {
    flight::NmeaParser parser;
    // The sentence the existing tests use. The gate must not cost a fix that was always
    // fine -- and the numbers it judged on must be readable afterwards, because a gate
    // whose inputs are not recorded cannot be tuned in the field.
    CHECK(feed_nmea(parser, nmea("GPGGA,120000.00,2110.0000,N,07246.9980,E,1,08,0.9,15.0,M,0.0,M,,")));
    CHECK(parser.latest().satellites == 8);
    CHECK(parser.latest().hdop > 0.89 && parser.latest().hdop < 0.91);
    CHECK(parser.fixes_rejected() == 0);
}

void test_a_refused_fix_does_not_disturb_the_last_good_one() {
    // Degrade rather than stop: a bad sentence must not erase a position the vehicle
    // already had. Losing the fix outright is what the no-fix path is for.
    flight::NmeaParser parser;
    CHECK(feed_nmea(parser, nmea("GPGGA,120000.00,2110.0000,N,07246.9980,E,1,08,0.9,15.0,M,0.0,M,,")));
    const double kept_lat = parser.latest().latitude;
    // Still a fix afterwards -- the vehicle has not lost its position, it has declined an
    // update. That is exactly the distinction the quality <= 0 path does *not* make, and
    // asserting !has_fix() here would be asserting the wrong behaviour.
    CHECK(feed_nmea(parser, nmea("GPGGA,120001.00,2111.0000,N,07247.9980,E,1,03,1.0,15.0,M,0.0,M,,")));
    CHECK(parser.latest().latitude == kept_lat);
    CHECK(parser.fixes_rejected() == 1);
    // And a genuine loss of fix still clears it.
    CHECK(!feed_nmea(parser, nmea("GPGGA,120002.00,,,,,0,00,,,M,,M,,")));
}

// Ground-to-vehicle maintenance commands. The vehicle flies with no uplink -- the config
// flag defaults to false -- so everything here is about the bench, and about making sure a
// command can never be produced by accident from traffic that is not one.
void test_a_command_round_trips_for_its_own_team() {
    std::uint32_t pn = 0;
    const std::string line =
        cansat::format_command("CAN-Team-25", cansat::CommandKind::erase_log, "hunter2", 1);
    CHECK(cansat::parse_command(line, "CAN-Team-25", "hunter2", pn) ==
          cansat::CommandKind::erase_log);
    CHECK(pn == 1);
}

void test_the_password_never_appears_on_the_wire() {
    // The whole point of hashing it. A link anyone can listen to must not carry the secret.
    const std::string line = cansat::format_command(
        "CAN-Team-25", cansat::CommandKind::erase_log, "hunter2", 1);
    CHECK(line.find("hunter2") == std::string::npos);
}

void test_a_command_for_another_team_is_ignored() {
    std::uint32_t pn = 0;
    const std::string line =
        cansat::format_command("CAN-Team-07", cansat::CommandKind::erase_log, "hunter2", 1);
    CHECK(cansat::parse_command(line, "CAN-Team-25", "hunter2", pn) == cansat::CommandKind::none);
}

void test_a_command_with_the_wrong_password_is_ignored() {
    std::uint32_t pn = 0;
    const std::string line =
        cansat::format_command("CAN-Team-25", cansat::CommandKind::erase_log, "hunter2", 1);
    CHECK(cansat::parse_command(line, "CAN-Team-25", "hunter3", pn) == cansat::CommandKind::none);
    CHECK(cansat::parse_command(line, "CAN-Team-25", "", pn) == cansat::CommandKind::none);
}

void test_a_token_is_valid_for_exactly_one_packet_number() {
    // A captured frame is only ever worth the number it was made for. Everything else about
    // replay protection is the controller's job; this is what makes it possible.
    std::uint32_t pn = 0;
    const std::string line =
        cansat::format_command("CAN-Team-25", cansat::CommandKind::erase_log, "hunter2", 10);
    CHECK(cansat::parse_command(line, "CAN-Team-25", "hunter2", pn) ==
          cansat::CommandKind::erase_log);

    // Same token, a different declared number: the digest no longer matches.
    std::string moved = line;
    const std::size_t at = moved.find("PN-10;");
    CHECK(at != std::string::npos);
    moved.replace(at, 6, "PN-11;");
    CHECK(cansat::parse_command(moved, "CAN-Team-25", "hunter2", pn) == cansat::CommandKind::none);
}

void test_a_telemetry_packet_is_never_a_command() {
    std::uint32_t pn = 0;
    const std::string packet =
        "CAN-Team-25; P-001; Ti-00:00:00:000; A-23.0; Pr-101048.55; T-31.4; Ro--1.5; Pi-5.0; "
        "Ya-0.0; AX--0.86; AY--0.25; AZ-9.81; MODE-READY; FAULTS-2; CAL-0; ARM-0; YR-G;";
    CHECK(cansat::parse_command(packet, "CAN-Team-25", "hunter2", pn) == cansat::CommandKind::none);
}

void test_an_unconfigured_vehicle_matches_nothing() {
    std::uint32_t pn = 0;
    const std::string line =
        cansat::format_command("CAN-Team-25", cansat::CommandKind::erase_log, "hunter2", 1);
    CHECK(cansat::parse_command(line, "", "hunter2", pn) == cansat::CommandKind::none);
    // And a vehicle with no password set must not be commandable at all.
    CHECK(cansat::format_command("CAN-Team-25", cansat::CommandKind::erase_log, "", 1).empty());
}

void test_a_truncated_command_is_ignored() {
    std::uint32_t pn = 0;
    const std::string line =
        cansat::format_command("CAN-Team-25", cansat::CommandKind::erase_log, "hunter2", 1);
    for (std::size_t cut = 1; cut < line.size(); ++cut) {
        CHECK(cansat::parse_command(line.substr(0, cut), "CAN-Team-25", "hunter2", pn) ==
              cansat::CommandKind::none);
    }
}

// The digest both ends compute. A divergence here is a console that cannot command the
// vehicle it was built for, and it would show up on the bench as "the button does nothing".
void test_the_commanded_rate_periods_clear_their_own_airtime() {
    // The static_asserts in link_profile.hpp already refuse a build where a period is
    // inside its own airtime plus the guard -- they refused 363 and 280 when this was
    // written, which is how the periods came to be 364 and 281. This states the figures the
    // design document and the runbook quote, so a change that stays legal while moving the
    // published rates fails here rather than on a launch day.
    const double gps = cansat::lora_time_on_air_ms(cansat::link::kMaxRatePacketBytesGps,
                                                   cansat::link::kModem);
    const double lean = cansat::lora_time_on_air_ms(cansat::link::kMaxRatePacketBytesLean,
                                                    cansat::link::kModem);
    const auto near = [](double a, double b, double tol) { return a > b - tol && a < b + tol; };

    // 201 bytes costs exactly what 199 does: both quantise to 298 symbols at SF7/125 kHz.
    // This is the whole reason position can go on the air for free.
    CHECK(near(gps, cansat::link::kWorstCaseAirtimeMs, 0.001));
    CHECK(near(gps, 317.70, 0.05));
    CHECK(near(lean, 235.78, 0.05));

    CHECK(cansat::link::kMaxRatePeriodGpsMs == 364);
    CHECK(cansat::link::kMaxRatePeriodLeanMs == 281);

    // The guard is what binds, not the duty policy: at these periods the duty is ~89 % and
    // ~85 %, and neither period would be legal if the SD write had nowhere to go.
    CHECK(gps * 1.018 + cansat::link::kMaxRateGuardMs <=
          static_cast<double>(cansat::link::kMaxRatePeriodGpsMs));
    CHECK(lean * 1.018 + cansat::link::kMaxRateGuardMs <=
          static_cast<double>(cansat::link::kMaxRatePeriodLeanMs));

    // And both are genuinely faster than normal flight, in the right order.
    CHECK(cansat::link::kMaxRatePeriodLeanMs < cansat::link::kMaxRatePeriodGpsMs);
    CHECK(cansat::link::kMaxRatePeriodGpsMs < cansat::link::kTelemetryPeriodMs);
}

void test_a_token_authorises_one_command_and_not_another() {
    // The reason the digest covers the command. Without it, the operator who was allowed to
    // erase the log was also allowed -- by anyone holding that one frame -- to put the
    // vehicle into a mode it cannot be talked out of. Both substitutions are checked,
    // because either direction is a way to make the vehicle do something nobody pressed a
    // button for.
    const std::string password = "bench-password";
    const std::string team = "CAN-Team-25";
    std::uint32_t pn = 0;

    const std::string erase =
        cansat::format_command(team, cansat::CommandKind::erase_log, password, 40);
    CHECK(cansat::parse_command(erase, team, password, pn) == cansat::CommandKind::erase_log);

    std::string swapped = erase;
    const std::size_t at = swapped.find("ERASE_LOG");
    CHECK(at != std::string::npos);
    swapped.replace(at, std::string("ERASE_LOG").size(), "MAX_RATE");
    CHECK(cansat::parse_command(swapped, team, password, pn) == cansat::CommandKind::none);

    const std::string rate =
        cansat::format_command(team, cansat::CommandKind::max_rate, password, 40);
    CHECK(cansat::parse_command(rate, team, password, pn) == cansat::CommandKind::max_rate);
    std::string back = rate;
    const std::size_t at2 = back.find("MAX_RATE");
    CHECK(at2 != std::string::npos);
    back.replace(at2, std::string("MAX_RATE").size(), "ERASE_LOG");
    CHECK(cansat::parse_command(back, team, password, pn) == cansat::CommandKind::none);

    // And a name this build does not know is none, rather than the nearest thing it knows.
    // MAX_RATE_LEAN is the case that matters: an earlier build understood it.
    std::string unknown = rate;
    unknown.replace(at2, std::string("MAX_RATE").size(), "MAX_RATE_LEAN");
    CHECK(cansat::parse_command(unknown, team, password, pn) == cansat::CommandKind::none);
}

void test_command_tokens_match_the_shared_fixture(const std::string& repo_root) {
    const std::string path = repo_root + "/test-data/command-tokens.tsv";
    std::ifstream file(path);
    CHECK(file.is_open());
    std::string line;
    int rows = 0;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        const std::size_t a = line.find('\t');
        const std::size_t b = line.find('\t', a + 1);
        const std::size_t c = line.find('\t', b + 1);
        CHECK(a != std::string::npos && b != std::string::npos && c != std::string::npos);
        const std::string password = line.substr(0, a);
        const std::string command = line.substr(a + 1, b - a - 1);
        const std::uint32_t pn =
            static_cast<std::uint32_t>(std::stoul(line.substr(b + 1, c - b - 1)));
        std::string expected = line.substr(c + 1);
        while (!expected.empty() &&
               (expected.back() == '\r' || expected.back() == '\n')) {
            expected.pop_back();
        }
        // The fixture names the command in text; the digest takes the enum. A row naming a
        // command this build does not know is a fixture ahead of the firmware, and saying so
        // beats quietly computing a token for CommandKind::none and reporting a mismatch.
        cansat::CommandKind kind = cansat::CommandKind::none;
        if (command == "ERASE_LOG") kind = cansat::CommandKind::erase_log;
        else if (command == "MAX_RATE") kind = cansat::CommandKind::max_rate;
        CHECK(kind != cansat::CommandKind::none);
        CHECK(cansat::command_token(password, kind, pn) == expected);
        ++rows;
    }
    CHECK(rows >= 8);
}

void test_a_hemisphere_from_the_wrong_axis_is_rejected() {
    // A sentence can pass its checksum and still carry a hemisphere character that does
    // not belong to the field it is in. Accepting all four on both axes read a latitude
    // marked 'E' as northern, and one marked 'W' as southern -- a position on the wrong
    // side of the equator, produced from a sentence that was already saying something had
    // gone wrong.
    flight::NmeaParser parser;
    const auto feed = [&parser](const char* body) {
        std::uint8_t checksum = 0;
        for (const char* p = body + 1; *p != '\0'; ++p) checksum ^= static_cast<std::uint8_t>(*p);
        char line[128];
        std::snprintf(line, sizeof(line), "%s*%02X\r\n", body, static_cast<unsigned>(checksum));
        bool applied = false;
        for (const char* p = line; *p != '\0'; ++p) applied = parser.consume(*p) || applied;
        return applied;
    };

    // A good fix first, so a rejection afterwards is visibly a rejection and not an
    // absence of data.
    CHECK(feed("$GPGGA,120000.00,2110.0000,N,07246.9980,E,1,08,1.0,15.0,M,0.0,M,,"));
    CHECK(parser.latest().valid);
    const double good_lat = parser.latest().latitude;
    CHECK(good_lat > 21.0 && good_lat < 21.2);

    // Latitude carrying a longitude hemisphere: rejected, and the fix is dropped rather
    // than left standing at its previous value.
    CHECK(!feed("$GPGGA,120001.00,2110.0000,E,07246.9980,E,1,08,1.0,15.0,M,0.0,M,,"));
    CHECK(!parser.latest().valid);

    // The same for a longitude carrying a latitude hemisphere.
    CHECK(!feed("$GPGGA,120002.00,2110.0000,N,07246.9980,N,1,08,1.0,15.0,M,0.0,M,,"));
    CHECK(!parser.latest().valid);

    // And in RMC, which carries the ground track the yaw cross-check uses.
    CHECK(!feed("$GPRMC,120003.00,A,2110.0000,W,07246.9980,E,0.5,90.0,050926,,,A"));
    CHECK(!parser.latest().valid);

    // A correct sentence still parses after all of that.
    CHECK(feed("$GPGGA,120004.00,2110.0000,S,07246.9980,W,1,08,1.0,15.0,M,0.0,M,,"));
    CHECK(parser.latest().valid);
    CHECK(parser.latest().latitude < 0.0);   // S
    CHECK(parser.latest().longitude < 0.0);  // W
}

void test_gps_coordinate_validation() {
    // Southern and western hemispheres must come back negative.
    flight::NmeaParser sw;
    CHECK(feed_nmea(sw,
        "$GPGGA,123519,4807.038,S,01131.000,W,1,08,0.9,545.4,M,46.9,M,,*48\r\n"));
    CHECK(approx(sw.latest().latitude, -48.1173, 0.001));
    CHECK(approx(sw.latest().longitude, -11.5167, 0.001));

    // A three-digit longitude in the eastern hemisphere.
    flight::NmeaParser east;
    CHECK(feed_nmea(east,
        "$GPGGA,123519,3345.000,S,15112.000,E,1,09,0.8,12.0,M,20.0,M,,*65\r\n"));
    CHECK(approx(east.latest().latitude, -33.75, 0.001));
    CHECK(approx(east.latest().longitude, 151.2, 0.001));

    // Latitude beyond 90 degrees is impossible, not merely unlikely.
    flight::NmeaParser bad_lat;
    CHECK(!feed_nmea(bad_lat,
        "$GPGGA,123519,9907.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*4B\r\n"));

    // Longitude beyond 180 degrees, likewise.
    flight::NmeaParser bad_lon;
    CHECK(!feed_nmea(bad_lon,
        "$GPGGA,123519,4807.038,N,18131.000,E,1,08,0.9,545.4,M,46.9,M,,*4F\r\n"));

    // A minutes field of 77 cannot occur: minutes run 0..59.
    flight::NmeaParser bad_min;
    CHECK(!feed_nmea(bad_min,
        "$GPGGA,123519,4877.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*40\r\n"));

    // No hemisphere character means the sign is unknown; it must not be assumed north.
    flight::NmeaParser no_hemi;
    CHECK(!feed_nmea(no_hemi,
        "$GPGGA,123519,4807.038,,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*09\r\n"));

    // RMC carries a fix too, and its void form must clear the fix without inventing one.
    flight::NmeaParser rmc;
    CHECK(feed_nmea(rmc,
        "$GPRMC,123519,A,4807.038,N,01131.000,E,022.4,084.4,230394,003.1,W*6A\r\n"));
    CHECK(approx(rmc.latest().latitude, 48.1173, 0.001));
    for (const char c : std::string("$GPRMC,123519,V,,,,,,,230394,,*33\r\n")) rmc.consume(c);
    CHECK(!rmc.has_fix());

    // Multi-constellation receivers use GN/GL/GA talker ids, not only GP.
    flight::NmeaParser gnss;
    CHECK(feed_nmea(gnss,
        "$GNGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*59\r\n"));
    CHECK(approx(gnss.latest().latitude, 48.1173, 0.001));

    // Rejected sentences are not checksum failures, and must not be counted as such.
    CHECK(bad_lat.checksum_errors() == 0);
    CHECK(no_hemi.checksum_errors() == 0);
}

// Angles wrap; quaternions do not. The estimator carries its state as a quaternion for
// exactly this reason, and the Euler angles it reports must stay well formed as the
// vehicle tumbles through every seam those angles have.
void test_orientation_survives_the_wrap_and_the_poles() {
    // Inverted: gravity on -Z, so roll is at the +-180 degree seam.
    flight::OrientationEstimator est;
    est.update(0.0, 0.2, -9.8, 0.0, 0.0, 0.0, 0.05);
    CHECK(std::fabs(est.estimate().roll_deg) > 170.0);

    // Nudge the accelerometer across the seam. A filter that averaged the raw angles
    // would swing most of the way around the circle; this one must stay put.
    for (int i = 0; i < 40; ++i) {
        est.update(0.0, -0.2, -9.8, 0.0, 0.0, 0.0, 0.05);
        CHECK(std::fabs(est.estimate().roll_deg) > 170.0);
    }
    const double settled = est.estimate().roll_deg;
    CHECK(settled > -180.0 && settled <= 180.0);

    // Level and still: converges to level rather than drifting.
    flight::OrientationEstimator level;
    for (int i = 0; i < 100; ++i) level.update(0.0, 0.0, 9.80665, 0.0, 0.0, 0.0, 0.033);
    CHECK(approx(level.estimate().roll_deg, 0.0, 0.5));
    CHECK(approx(level.estimate().pitch_deg, 0.0, 0.5));

    // A continuous tumble at 100 deg/s about every axis at once, for 20 seconds. The old
    // Euler integration lost its solution as pitch passed 90 degrees; this must stay a
    // valid attitude throughout, with no accelerometer help at all (free fall).
    flight::OrientationEstimator tumble;
    tumble.update(0.0, 0.0, 9.80665, 0.0, 0.0, 0.0, 0.033);  // seed level
    for (int i = 0; i < 600; ++i) {
        tumble.update(0.0, 0.0, 0.0, 100.0, 100.0, 100.0, 0.033);
        const auto e = tumble.estimate();
        CHECK(std::isfinite(e.roll_deg) && std::isfinite(e.pitch_deg) &&
              std::isfinite(e.yaw_deg));
        CHECK(e.roll_deg > -180.0 && e.roll_deg <= 180.0);
        CHECK(e.pitch_deg >= -90.0 && e.pitch_deg <= 90.0);
        CHECK(e.yaw_deg > -180.0 && e.yaw_deg <= 180.0);
    }

    // A pure pitch-up through the +90 degree singularity: the reported pitch must reach
    // the pole and come back, never produce a NaN on the way.
    flight::OrientationEstimator over_the_top;
    over_the_top.update(0.0, 0.0, 9.80665, 0.0, 0.0, 0.0, 0.033);
    double max_pitch = 0.0;
    for (int i = 0; i < 200; ++i) {
        over_the_top.update(0.0, 0.0, 0.0, 0.0, 90.0, 0.0, 0.033);
        const auto e = over_the_top.estimate();
        CHECK(std::isfinite(e.pitch_deg));
        if (e.pitch_deg > max_pitch) max_pitch = e.pitch_deg;
    }
    CHECK(max_pitch > 88.0);
}

// The estimator must not invent an attitude from nothing, and must not be knocked over by
// inputs a broken sensor can produce.
void test_orientation_rejects_unusable_input() {
    flight::OrientationEstimator est;
    // All-zero accelerometer: nothing defines which way is up, so nothing is claimed.
    est.update(0.0, 0.0, 0.0, 1.0, 2.0, 3.0, 0.033);
    CHECK(!est.estimate().valid);

    // Non-finite input is discarded rather than propagated into the quaternion.
    est.update(0.0, 0.0, 9.80665, 0.0, 0.0, 0.0, 0.033);
    CHECK(est.estimate().valid);
    const double before = est.estimate().roll_deg;
    est.update(std::nan(""), 0.0, 9.80665, 0.0, 0.0, 0.0, 0.033);
    CHECK(approx(est.estimate().roll_deg, before, 1e-12));

    // A stalled loop cannot be allowed to integrate an unbounded step.
    est.update(0.0, 0.0, 9.80665, 0.0, 0.0, 0.0, 1e9);
    CHECK(std::isfinite(est.estimate().roll_deg));

    // reset() drops the solution; the next accelerometer sample re-seeds it.
    est.reset();
    CHECK(!est.estimate().valid);
}

// A vehicle turning at a constant rate on the pad is perfectly steady by variance, and a
// std-dev gate alone would accept that rotation as gyro bias — cancelling a real body rate
// for the whole flight, silently.
void test_calibration_rejects_a_steady_rotation_as_bias() {
    flight::Configuration config;
    config.team_id = "CAN-Team-07";
    config.calib_samples = 8;
    config.calib_timeout_ms = 100000;

    // A plausible zero-rate offset is accepted.
    flight::StartupCalibrator small(config);
    flight::ImuSample sample{};
    sample.ax_mps2 = 0.0;
    sample.ay_mps2 = 0.0;
    sample.az_mps2 = 9.80665;
    sample.gz_dps = 3.0;  // within the datasheet's +-20 deg/s zero-rate range
    sample.valid = true;
    small.update(0);  // starts the calibration window
    for (int i = 0; i < 20; ++i) small.add_imu(sample);
    small.update(1000);
    CHECK(small.complete());
    CHECK(small.result().gyro_bias_valid);
    CHECK(approx(small.result().gyro_bias_dps[2], 3.0, 0.01));

    // A steady 40 deg/s rotation has zero variance but cannot be bias.
    flight::StartupCalibrator spinning(config);
    sample.gz_dps = 40.0;
    spinning.update(0);
    for (int i = 0; i < 20; ++i) spinning.add_imu(sample);
    spinning.update(1000);
    CHECK(!spinning.complete());  // refused, and it keeps trying until the timeout

    // Once the rotation stops, calibration settles normally.
    sample.gz_dps = 1.0;
    for (int i = 0; i < 20; ++i) spinning.add_imu(sample);
    spinning.update(2000);
    CHECK(spinning.complete());
    CHECK(approx(spinning.result().gyro_bias_dps[2], 1.0, 0.01));
}

// A fault that was critical must stay critical until it is cleared. The controller
// escalates some faults, and a later routine report at a lower severity must not quietly
// downgrade one that still applies.
void test_fault_severity_never_falls_while_active() {
    flight::FaultManager faults;

    faults.report(flight::FaultCode::imu_init, flight::FaultSeverity::error, 100);
    CHECK(faults.active(flight::FaultCode::imu_init));
    CHECK(!faults.has_critical());

    // Escalation is honoured.
    faults.report(flight::FaultCode::imu_init, flight::FaultSeverity::critical, 200);
    CHECK(faults.has_critical());

    // A later, routine report at a lower severity must not undo it.
    faults.report(flight::FaultCode::imu_init, flight::FaultSeverity::warning, 300);
    CHECK(faults.has_critical());

    // Clearing genuinely resolves it, and the next occurrence starts from its own severity.
    faults.clear(flight::FaultCode::imu_init);
    CHECK(!faults.has_critical());
    CHECK(!faults.active(flight::FaultCode::imu_init));
    faults.report(flight::FaultCode::imu_init, flight::FaultSeverity::warning, 400);
    CHECK(faults.active(flight::FaultCode::imu_init));
    CHECK(!faults.has_critical());

    // ever_critical() latches for the whole power session: a critical fault that has been
    // cleared still happened, and the ground station should be able to see that it did.
    CHECK(faults.ever_critical());

    // Occurrence counting and timestamps.
    flight::FaultManager counted;
    counted.report(flight::FaultCode::sd_write, flight::FaultSeverity::warning, 1000);
    counted.report(flight::FaultCode::sd_write, flight::FaultSeverity::warning, 5000);
    counted.report(flight::FaultCode::radio_tx, flight::FaultSeverity::error, 7000);
    CHECK(counted.total_occurrences() == 3);
    CHECK(counted.active_count() == 2);
    counted.clear(flight::FaultCode::sd_write);
    CHECK(counted.active_count() == 1);
    CHECK(counted.total_occurrences() == 3);  // history is not erased by clearing

    // Every code has a name, and an out-of-range code is refused rather than writing past
    // the fixed array.
    for (std::size_t i = 0; i < static_cast<std::size_t>(flight::FaultCode::count); ++i) {
        const char* name = flight::fault_name(static_cast<flight::FaultCode>(i));
        CHECK(name != nullptr && name[0] != '\0');
    }
    flight::FaultManager guarded;
    guarded.report(flight::FaultCode::count, flight::FaultSeverity::critical, 0);
    CHECK(guarded.total_occurrences() == 0);
    CHECK(!guarded.has_critical());
}

// A steady parachute descent reads as 1 g on the accelerometer, indistinguishable from
// sitting on the ground. Only the vertical rate separates them, so prove it does.
void test_landing_is_not_declared_during_a_steady_descent() {
    flight::Configuration c;
    c.team_id = "CAN-Team-07";
    flight::StateMachine sm(c);

    flight::DetectionInputs di;
    di.self_test_ok = true;
    di.sensors_ok = true;
    di.armed = true;
    sm.begin_self_test(0);
    sm.update(0, di);                     // SELF_TEST -> READY
    CHECK(sm.state() == flight::MissionState::ready);

    // Launch.
    di.accel_magnitude_mps2 = 40.0;
    sm.update(1000, di);
    sm.update(1400, di);
    CHECK(sm.state() == flight::MissionState::flight);

    // Steady descent under a parachute: 1 g, but moving down at 6 m/s. Sustained well past
    // both the minimum flight time and the landing confirmation window.
    di.accel_magnitude_mps2 = 9.80665;
    di.altitude_rate_mps = -6.0;
    di.altitude_agl_m = 200.0;
    for (std::uint64_t t = 2000; t <= 30000; t += 500) {
        sm.update(t, di);
        CHECK(sm.state() == flight::MissionState::flight);
    }

    // Touchdown: same acceleration, but the vertical rate collapses.
    di.altitude_rate_mps = 0.1;
    di.altitude_agl_m = 0.5;
    sm.update(30500, di);
    CHECK(sm.state() == flight::MissionState::flight);   // still inside the 3 s window
    sm.update(33600, di);
    CHECK(sm.state() == flight::MissionState::landed);

    // A momentary blip during descent must not start the landing timer over a real one.
    flight::StateMachine sm2(c);
    flight::DetectionInputs d2 = di;
    d2.armed = true;
    d2.self_test_ok = true;
    d2.sensors_ok = true;
    sm2.begin_self_test(0);
    sm2.update(0, d2);
    d2.accel_magnitude_mps2 = 40.0;
    sm2.update(1000, d2);
    sm2.update(1400, d2);
    CHECK(sm2.state() == flight::MissionState::flight);
    d2.accel_magnitude_mps2 = 9.80665;
    for (std::uint64_t t = 5000; t <= 20000; t += 500) {
        // Rate flickers below the threshold for a single sample every couple of seconds.
        d2.altitude_rate_mps = (t % 2000 == 0) ? -0.2 : -6.0;
        sm2.update(t, d2);
    }
    CHECK(sm2.state() == flight::MissionState::flight);
}

// Battery reporting has to be honest about which voltage it is showing. With no divider
// measured, the number is the ADC pin voltage, and an operator reading 1.6 V off a 3.7 V
// cell needs to know that before reacting to it.
void test_battery_voltage_reports_whether_it_is_scaled() {
    flight::Configuration c;
    c.team_id = "CAN-Team-07";
    c.battery_period_ms = 10;
    c.health_period_ms = 10;
    flight::test::MockImu imu;
    flight::test::MockBarometer baro;
    flight::test::MockGps gps;
    flight::test::MockRadio radio;
    flight::test::MockLogger logger;
    flight::test::MockBoard board;
    board.pin_voltage = 1.65f;  // half of a 3.3 V reference: a plausible divider output

    flight::Controller unscaled(c, imu, baro, gps, radio, logger, board);
    CHECK(unscaled.initialize());
    unscaled.poll(0);
    unscaled.poll(20);
    // Divider unknown: the raw pin voltage is reported, and flagged as unscaled.
    CHECK(approx(unscaled.health().battery_voltage, 1.65, 0.001));
    CHECK(!unscaled.health().battery_voltage_is_scaled);
    // A low-battery fault cannot be judged from a pin reading, so it stays clear.
    CHECK(!unscaled.faults().active(flight::FaultCode::battery_low));

    // With a measured divider, the same pin voltage becomes a cell voltage.
    flight::Configuration scaled_config = c;
    scaled_config.battery_divider_ratio = 2.0f;   // (R1 + R2) / R2
    scaled_config.battery_low_voltage = 3.4f;
    flight::Controller scaled(scaled_config, imu, baro, gps, radio, logger, board);
    CHECK(scaled.initialize());
    scaled.poll(0);
    scaled.poll(20);
    CHECK(approx(scaled.health().battery_voltage, 3.30, 0.001));
    CHECK(scaled.health().battery_voltage_is_scaled);
    CHECK(scaled.faults().active(flight::FaultCode::battery_low));  // 3.30 < 3.40

    // A healthy cell clears the fault again.
    board.pin_voltage = 2.0f;  // 4.0 V after scaling
    scaled.poll(40);
    scaled.poll(60);
    CHECK(approx(scaled.health().battery_voltage, 4.0, 0.001));
    CHECK(!scaled.faults().active(flight::FaultCode::battery_low));
}

// The loop tick is bounded from above by hardware nobody thinks about: the GPS UART's
// FIFO keeps filling whether or not the loop reads it.
void test_loop_tick_is_bounded_by_the_gps_uart_fifo() {
    std::string why;
    flight::Configuration c;
    c.team_id = "CAN-Team-07";

    // 32 bytes at 9600 baud, 8N1, is 33.3 ms of data.
    CHECK(approx(flight::uart_fifo_fill_ms(9600, 32), 33.333, 0.01));
    CHECK(approx(flight::uart_fifo_fill_ms(38400, 32), 8.333, 0.01));
    CHECK(flight::uart_fifo_fill_ms(0, 32) == 0.0);  // no baud, no constraint

    // The default 2 ms tick has 16x margin.
    CHECK(c.loop_tick_ms == 2);
    CHECK(flight::validate_config(c, why));

    // A tick beyond half the fill time is refused: NMEA bytes would be lost before
    // anything read them, showing up as checksum errors rather than an obvious fault.
    c.loop_tick_ms = 20;
    CHECK(!flight::validate_config(c, why));
    CHECK(why.find("GPS UART") != std::string::npos);

    // A faster GPS link tightens the limit, and the same tick is then refused sooner.
    flight::Configuration fast;
    fast.team_id = "CAN-Team-07";
    fast.gps_baud = 115200;   // 2.8 ms to fill
    fast.loop_tick_ms = 5;
    CHECK(!flight::validate_config(fast, why));
    fast.loop_tick_ms = 1;
    CHECK(flight::validate_config(fast, why));

    // Zero is meaningless, and a tick slower than the sensor period cannot schedule it.
    flight::Configuration bad;
    bad.team_id = "CAN-Team-07";
    bad.loop_tick_ms = 0;
    CHECK(!flight::validate_config(bad, why));
    bad.loop_tick_ms = 10;
    bad.sensor_period_ms = 5;
    CHECK(!flight::validate_config(bad, why));
}

// The sensor timing model, pinned to the BMP280 datasheet's own published presets.
void test_sensor_timing_model() {
    using flight::sensors::BaroFilter;
    using flight::sensors::Oversampling;

    // Datasheet table 14, "handheld device, dynamic": osrs_t x1, osrs_p x4, filter x16,
    // published output data rate 83 Hz.
    CHECK(approx(flight::sensors::baro_measure_ms_typ(Oversampling::x1, Oversampling::x4),
                 11.5, 1e-9));
    CHECK(approx(flight::sensors::baro_output_rate_hz(Oversampling::x1, Oversampling::x4),
                 83.33, 0.1));

    // Datasheet section 3.8.1 worst case for osrs_t x2 / osrs_p x16: 43.2 ms.
    CHECK(approx(flight::sensors::baro_measure_ms_max(Oversampling::x2, Oversampling::x16),
                 43.2, 0.05));
    // ...whose typical output rate is the datasheet's 26.3 Hz "indoor navigation" preset.
    CHECK(approx(flight::sensors::baro_output_rate_hz(Oversampling::x2, Oversampling::x16),
                 26.3, 0.1));

    // Skipping a channel removes its term entirely.
    CHECK(flight::sensors::baro_measure_ms_typ(Oversampling::skipped, Oversampling::skipped) ==
          1.0);
    CHECK(flight::sensors::baro_measure_ms_typ(Oversampling::skipped, Oversampling::x1) <
          flight::sensors::baro_measure_ms_typ(Oversampling::x1, Oversampling::x1));

    // More oversampling always costs time, never saves it.
    double previous = 0.0;
    for (const Oversampling p : {Oversampling::x1, Oversampling::x2, Oversampling::x4,
                                 Oversampling::x8, Oversampling::x16}) {
        const double t = flight::sensors::baro_measure_ms_typ(Oversampling::x1, p);
        CHECK(t > previous);
        previous = t;
    }
    CHECK(flight::sensors::baro_measure_ms_max(Oversampling::x1, Oversampling::x4) >
          flight::sensors::baro_measure_ms_typ(Oversampling::x1, Oversampling::x4));

    // Register encodings, datasheet section 4.3.
    // osrs_t x1 (001), osrs_p x4 (011), normal mode (11) -> 0b001_011_11 = 0x2F.
    CHECK(flight::sensors::baro_ctrl_meas(Oversampling::x1, Oversampling::x4) == 0x2F);
    // The previous hard-coded setting: osrs_t x2 (010), osrs_p x16 (101) -> 0x57.
    CHECK(flight::sensors::baro_ctrl_meas(Oversampling::x2, Oversampling::x16) == 0x57);
    // t_sb 0.5 ms (000), filter x16 (100) -> 0b000_100_00 = 0x10.
    CHECK(flight::sensors::baro_config(BaroFilter::x16) == 0x10);
    CHECK(flight::sensors::baro_config(BaroFilter::off) == 0x00);

    // MPU-9250 register map: DLPF 1..6 runs the gyro at 1 kHz, divided by (1 + div).
    CHECK(approx(flight::sensors::imu_sample_rate_hz(4, 4), 200.0, 1e-9));
    CHECK(approx(flight::sensors::imu_sample_rate_hz(0, 4), 1600.0, 1e-9));
    // The accelerometer's bandwidth comes from its own register on this part and is NOT
    // the MPU-6050's figure for the same setting.
    CHECK(approx(flight::sensors::imu_accel_bandwidth_hz(4), 21.2, 1e-9));
    CHECK(approx(flight::sensors::imu_gyro_bandwidth_hz(4), 20.0, 1e-9));
    CHECK(approx(flight::sensors::imu_gyro_bandwidth_hz(0), 250.0, 1e-9));
    CHECK(approx(flight::sensors::imu_accel_bandwidth_hz(7), 420.0, 1e-9));
    CHECK(flight::sensors::imu_accel_bandwidth_hz(3) > flight::sensors::imu_accel_bandwidth_hz(4));

    // The AK8963 free-runs; only its 100 Hz mode can feed a 30 Hz attitude update.
    CHECK(approx(flight::sensors::mag_output_rate_hz(
                     flight::sensors::MagMode::continuous_100hz), 100.0, 1e-9));
    CHECK(approx(flight::sensors::mag_output_rate_hz(
                     flight::sensors::MagMode::continuous_8hz), 8.0, 1e-9));
    CHECK(approx(flight::sensors::mag_output_rate_hz(
                     flight::sensors::MagMode::power_down), 0.0, 1e-9));
}

// The acquisition rate must be one the sensors can actually feed.
void test_config_sensor_rate_guard() {
    using flight::sensors::Oversampling;
    std::string why;
    flight::Configuration c;
    c.team_id = "CAN-Team-07";

    // Default: 33 ms sampling against a barometer whose worst case is ~14 ms.
    CHECK(flight::validate_config(c, why));
    CHECK(c.sensor_period_ms == 33);
    const double baro_min = flight::sensors::baro_min_sample_period_ms(
        c.baro_osrs_t, c.baro_osrs_p, c.baro_standby_ms);
    CHECK(baro_min < static_cast<double>(c.sensor_period_ms));
    CHECK(flight::sensors::baro_output_rate_hz(c.baro_osrs_t, c.baro_osrs_p) > 30.0);

    // The old x2/x16 barometer setting cannot feed a 30 Hz loop: ~44 ms per conversion.
    c.baro_osrs_t = Oversampling::x2;
    c.baro_osrs_p = Oversampling::x16;
    CHECK(!flight::validate_config(c, why));
    CHECK(why.find("barometer") != std::string::npos);

    // It is fine at the slower rate it can actually sustain.
    c.sensor_period_ms = 50;
    CHECK(flight::validate_config(c, why));

    // The IMU's two anti-alias filters sit close to the acquisition Nyquist limit, which
    // is the documented trade: a narrower filter would blur the launch transient. What
    // must never happen is a bandwidth above the acquisition rate itself.
    flight::Configuration d;
    const double acquisition = 1000.0 / static_cast<double>(d.sensor_period_ms);
    const double nyquist = acquisition / 2.0;
    CHECK(flight::sensors::imu_accel_bandwidth_hz(d.imu_accel_dlpf_cfg) <= nyquist * 1.5);
    CHECK(flight::sensors::imu_gyro_bandwidth_hz(d.imu_gyro_dlpf_cfg) <= nyquist * 1.5);
    CHECK(flight::sensors::imu_sample_rate_hz(d.imu_gyro_dlpf_cfg, d.imu_sample_rate_div) >
          acquisition);
    // And the magnetometer must be able to supply one fresh sample per acquisition.
    CHECK(flight::sensors::mag_output_rate_hz(d.mag_mode) >= acquisition);

    // An IMU whose internal rate is below the acquisition rate IS rejected: reading it
    // faster than it converts returns the previous sample, and a repeated gyro reading
    // integrates as motion that never happened.
    flight::Configuration slow;
    slow.team_id = "CAN-Team-07";
    slow.imu_sample_rate_div = 200;  // 1 kHz / 201 = ~5 Hz against a 30 Hz acquisition
    CHECK(!flight::validate_config(slow, why));
    CHECK(why.find("internal sample rate") != std::string::npos);
}

// A barometer that returns the same conversion twice must not read as zero climb rate.
void test_controller_ignores_repeated_barometer_samples() {
    flight::Configuration c;
    c.team_id = "CAN-Team-07";
    c.sensor_period_ms = 50;
    c.health_period_ms = 50;  // keep health() current for the assertions below
    c.altitude_relative_to_baseline = false;
    flight::test::MockImu imu;
    flight::test::MockBarometer baro;
    flight::test::MockGps gps;
    flight::test::MockRadio radio;
    flight::test::MockLogger logger;
    flight::test::MockBoard board;
    flight::Controller ctrl(c, imu, baro, gps, radio, logger, board);
    CHECK(ctrl.initialize());

    // Climb steadily: a real driver reports falling pressure and rising altitude
    // together, so the estimated rate must go positive.
    double pressure = 101325.0;
    double altitude = 0.0;
    std::uint64_t t = 0;
    for (int i = 0; i < 40; ++i) {
        pressure -= 12.0;   // ~1 m per step
        altitude += 1.0;    // 1 m per 50 ms -> 20 m/s climb
        baro.sample.pressure_pa = pressure;
        baro.sample.altitude_m = altitude;
        t += 50;
        ctrl.poll(t);
    }
    const double climbing = ctrl.health().altitude_rate_mps;
    CHECK(climbing > 1.0);

    // The loop briefly outruns the sensor: a couple of repeated conversions must not be
    // treated as real samples, or the rate is dragged toward zero by fake zero-length steps.
    t += 50;
    ctrl.poll(t);
    t += 50;
    ctrl.poll(t);
    CHECK(approx(ctrl.health().altitude_rate_mps, climbing, 1e-9));

    // But an unchanged pressure for longer than the hold window means the vehicle has
    // genuinely stopped moving. Holding the descent rate for ever would stop the landing
    // detector from firing and leave the mission stuck in FLIGHT after it had landed.
    for (int i = 0; i < 40; ++i) {
        t += 50;
        ctrl.poll(t);
    }
    CHECK(std::fabs(ctrl.health().altitude_rate_mps) < 1.0);
    CHECK(std::fabs(ctrl.health().altitude_rate_mps) < std::fabs(climbing));
}

// The vehicle and the bridge must configure the same modem. They agree only because both
// default from cansat/link_profile.hpp; this test is what keeps that true.
void test_link_profile_is_shared_by_both_ends() {
    flight::Configuration flight_config;          // vehicle side
    cansat::Sx1278Settings bridge_settings;       // ground-station bridge side (defaults)

    CHECK(flight_config.radio.frequency_hz == bridge_settings.frequency_hz);
    CHECK(flight_config.radio.spreading_factor == bridge_settings.spreading_factor);
    CHECK(flight_config.radio.bandwidth_hz == bridge_settings.bandwidth_hz);
    CHECK(flight_config.radio.coding_rate == bridge_settings.coding_rate);
    CHECK(flight_config.radio.preamble_length == bridge_settings.preamble_length);
    CHECK(flight_config.radio.enable_crc == bridge_settings.enable_crc);
    CHECK(flight_config.radio.test_sync_word == bridge_settings.sync_word);

    // The rulebook fixes only these two identities.
    CHECK(flight_config.radio.test_sync_word == 0xF3);
    CHECK(flight_config.radio.official_sync_word == 0xA5);
    CHECK(flight::sync_word(flight_config) == 0xF3);
    flight_config.radio_mode = flight::RadioMode::official;
    CHECK(flight::sync_word(flight_config) == 0xA5);

    // The profile's own arithmetic, checked at runtime as well as by its static_asserts.
    CHECK(cansat::link::kChannelDuty <= cansat::link::kMaxChannelDuty);
    CHECK(cansat::link::kTelemetryPeriodMs <= 1000);
    CHECK(approx(cansat::link::kWorstCaseAirtimeMs,
                 cansat::lora_time_on_air_ms(cansat::link::kWorstCasePacketBytes,
                                             cansat::link::kModem),
                 1e-9));
}

// Pins the C++ airtime model to the same published SX127x reference vectors as
// tools/link_budget.py. If these drift apart, every rate decision built on them is wrong.
void test_lora_airtime_reference_vectors() {
    cansat::LoraModemParams p;
    p.spreading_factor = 7;
    p.bandwidth_hz = 125000;
    p.coding_rate = 5;
    p.preamble_symbols = 8;
    CHECK(approx(cansat::lora_time_on_air_ms(13, p), 46.336, 1e-3));

    p.spreading_factor = 12;
    CHECK(approx(cansat::lora_time_on_air_ms(13, p), 1155.072, 1e-3));
    CHECK(cansat::lora_uses_low_data_rate_optimize(p));  // 32.768 ms symbols

    p.spreading_factor = 10;
    CHECK(!cansat::lora_uses_low_data_rate_optimize(p));  // 8.192 ms symbols
    CHECK(cansat::lora_payload_symbols(1, p) == 13);

    // A short payload at a high spreading factor drives the formula negative; the model
    // must clamp to the eight header symbols rather than wrapping.
    p.spreading_factor = 12;
    p.crc_enabled = false;
    CHECK(cansat::lora_payload_symbols(0, p) >= 8);

    // Airtime must be monotonic in payload length and fall as SF falls.
    cansat::LoraModemParams q;
    q.spreading_factor = 9;
    double previous = 0.0;
    for (std::size_t n = 0; n <= 200; n += 20) {
        const double toa = cansat::lora_time_on_air_ms(n, q);
        CHECK(toa >= previous);
        previous = toa;
    }
    cansat::LoraModemParams slow = q;
    slow.spreading_factor = 12;
    CHECK(cansat::lora_time_on_air_ms(188, slow) > cansat::lora_time_on_air_ms(188, q));

    // 30 Hz of full telemetry is not reachable on any standard setting — the project
    // runs sensors fast and the radio at the rate the channel allows.
    for (std::uint8_t sf = 7; sf <= 12; ++sf) {
        for (std::uint32_t bw : {125000u, 250000u, 500000u}) {
            cansat::LoraModemParams r;
            r.spreading_factor = sf;
            r.bandwidth_hz = bw;
            CHECK(cansat::lora_max_rate_hz(188, r, 1.0) < 30.0);
        }
    }
}

// ----------------------------------------------------------------------------
// The SD log is read by spreadsheets and scripts that index columns by position, so every
// row must have exactly as many columns as the header — with or without a GPS fix, whose
// three fields are the easy ones to miscount.
void test_sd_log_row_matches_its_header() {
    const auto columns = [](const std::string& row) {
        std::size_t count = 1;
        for (const char c : row) {
            if (c == ',') ++count;
        }
        return count;
    };

    flight::Configuration config;
    config.team_id = "CAN-Team-07";
    flight::TelemetryBuilder builder(config);

    flight::SensorSnapshot snapshot;
    snapshot.imu_valid = snapshot.baro_valid = snapshot.orientation_valid = true;
    snapshot.altitude_m = 12.3;
    snapshot.pressure_pa = 99000.12;
    snapshot.temperature_c = 21.5;
    snapshot.az_mps2 = 9.81;

    const std::size_t header_columns = columns(flight::TelemetryBuilder::sd_header());

    // Without a fix: the three GPS columns must still be present, just empty.
    const auto without_gps = builder.build(1, 1000, snapshot);
    CHECK(without_gps.has_value());
    const std::string row_without =
        builder.sd_line(*without_gps, flight::MissionState::ready, 0);
    CHECK(columns(row_without) == header_columns);
    CHECK(row_without.find(",0,,,,") != std::string::npos);  // gps_valid 0, three blanks

    // With a fix: the same column count, now populated.
    snapshot.gps.valid = true;
    snapshot.gps.latitude = 21.1667;
    snapshot.gps.longitude = 72.7833;
    snapshot.gps.altitude = 15.0;
    const auto with_gps = builder.build(2, 2000, snapshot);
    CHECK(with_gps.has_value());
    const std::string row_with =
        builder.sd_line(*with_gps, flight::MissionState::flight, 3);
    CHECK(columns(row_with) == header_columns);
    CHECK(row_with.find(",1,21.166700,") != std::string::npos);

    // The packet itself is the last column, so it can be recovered from the SD log alone.
    CHECK(row_with.size() > with_gps->packet.size());
    CHECK(row_with.compare(row_with.size() - with_gps->packet.size(),
                           with_gps->packet.size(), with_gps->packet) == 0);
    CHECK(row_with.find("FLIGHT") != std::string::npos);
}

void test_telemetry_builder() {
    flight::Configuration c;
    c.team_id = "CAN-Team-07";
    // This suite checks the packet's field order, GPS included, so it puts GPS on the air.
    // The default logs it instead -- see Configuration::transmit_gps.
    c.transmit_gps = true;
    flight::TelemetryBuilder b(c);

    flight::SensorSnapshot s;
    s.imu_valid = s.orientation_valid = s.baro_valid = true;
    s.altitude_m = 12.3; s.pressure_pa = 98765.4; s.temperature_c = 21.7;
    s.roll_deg = -4.2; s.pitch_deg = 5.5; s.yaw_deg = 61.0;
    s.ax_mps2 = 0.02; s.ay_mps2 = -0.1; s.az_mps2 = 9.79;

    auto built = b.build(1, 1500, s);
    CHECK(built.has_value());
    CHECK(built->packet.rfind("CAN-Team-07; P-001; Ti-00:00:01:500; A-12.3; ", 0) == 0);
    CHECK(built->packet.find("GP-") == std::string::npos);  // no fix -> no GPS fields

    // Add a GPS fix: optional fields appended AFTER the mandatory block.
    s.gps.valid = true; s.gps.latitude = 18.5; s.gps.longitude = 73.8; s.gps.altitude = 15.0;
    built = b.build(2, 1500, s);
    CHECK(built.has_value());
    const auto az = built->packet.find("AZ-");
    const auto gp = built->packet.find("GP-Lat-");
    CHECK(az != std::string::npos && gp != std::string::npos && gp > az);
    CHECK(static_cast<bool>(cansat::parse_packet(built->packet)));

    // Invalid mandatory data -> no telemetry point.
    s.baro_valid = false;
    CHECK(!b.build(3, 1500, s).has_value());
}

// ----------------------------------------------------------------------------
struct MemBlocks {
    std::vector<std::array<std::uint8_t, 512>> blocks;
    explicit MemBlocks(std::size_t n) : blocks(n) {}
    static bool rd(void* ctx, std::uint32_t lba, std::uint8_t* out) {
        auto* m = static_cast<MemBlocks*>(ctx);
        if (lba >= m->blocks.size()) return false;
        std::memcpy(out, m->blocks[lba].data(), 512);
        return true;
    }
    static bool wr(void* ctx, std::uint32_t lba, const std::uint8_t* in) {
        auto* m = static_cast<MemBlocks*>(ctx);
        if (lba >= m->blocks.size()) return false;
        std::memcpy(m->blocks[lba].data(), in, 512);
        return true;
    }
};

void test_the_widest_sd_row_still_fits_one_block() {
    // The onboard log writes one CSV row per 512-byte block, and RawBlockLog cuts anything
    // longer. A cut row is a corrupted record in the file that exists precisely so the
    // flight survives losing the radio, so the row must be bounded by construction rather
    // than by the values a mission happens to produce.
    //
    // The row is metadata plus the whole packet, and the packet is already capped at
    // cansat::link::kWorstCasePacketBytes. What this pins is the metadata: whatever the
    // rest of the row costs, it plus that cap has to fit in one block. Raising the packet
    // budget for airtime reasons then fails here instead of silently shortening the log.
    flight::Configuration config;
    config.team_id = "CAN-Team-07";
    flight::TelemetryBuilder builder(config);

    flight::SensorSnapshot snapshot;
    snapshot.imu_valid = snapshot.baro_valid = snapshot.orientation_valid = true;
    // Values at the wide end of what the plausibility gates admit, so every numeric column
    // is as long as it can legitimately be, and negative so each carries a sign.
    snapshot.altitude_m = -1234.5;
    snapshot.pressure_pa = -123456.78;
    snapshot.temperature_c = -123.4;
    snapshot.roll_deg = -179.9;
    snapshot.pitch_deg = -179.9;
    snapshot.yaw_deg = -359.9;
    snapshot.ax_mps2 = -299.99;
    snapshot.ay_mps2 = -299.99;
    snapshot.az_mps2 = -299.99;
    snapshot.gps.valid = true;
    snapshot.gps.latitude = -179.999999;
    snapshot.gps.longitude = -179.999999;
    snapshot.gps.altitude = -9999.9;

    // The longest state name, the largest counters, and a mission clock at the widest the
    // field can hold.
    const auto built = builder.build(4294967295u, 359999999u, snapshot);
    CHECK(built.has_value());
    if (!built) return;

    const std::string row =
        builder.sd_line(*built, flight::MissionState::self_test, 4294967295u);

    // The metadata around the packet, which is what this test actually bounds.
    CHECK(row.size() >= built->packet.size());
    const std::size_t overhead = row.size() - built->packet.size();
    const std::size_t widest = overhead + cansat::link::kWorstCasePacketBytes;

    CHECK(widest <= flight::RawBlockLog::kMaxRecordBytes);
    if (widest > flight::RawBlockLog::kMaxRecordBytes) {
        std::cerr << "  widest possible SD row is " << widest << " bytes, and one block "
                  << "holds " << flight::RawBlockLog::kMaxRecordBytes << "\n";
    }
}

// reset() + scrub_step() together are meant to leave the region the way
// tools/prepare_sd_card.py leaves a freshly created file: empty log, fresh header, and the
// old bytes overwritten with spaces rather than merely stepped over.
void test_a_reset_log_reads_empty_and_the_old_bytes_are_scrubbed_away() {
    MemBlocks mem(16);
    flight::RawBlockLog::Io io;
    io.ctx = &mem;
    io.read_block = &MemBlocks::rd;
    io.write_block = &MemBlocks::wr;

    flight::RawBlockLog log;
    CHECK(log.begin(io, 0, 16));
    for (int i = 0; i < 8; ++i) CHECK(log.append_line("secret-row", 10));
    CHECK(log.record_count() == 8);

    CHECK(log.reset());
    CHECK(log.record_count() == 0);        // empty immediately, before a single scrub write
    CHECK(log.scrubbing());                // and the scrub is armed
    const std::uint32_t boot_before = log.boot_count();

    // Drive it to completion the way the controller does.
    int guard = 0;
    while (log.scrub_step(2) && ++guard < 100) {}
    CHECK(!log.scrubbing());
    CHECK(log.scrub_blocks_remaining() == 0);

    // Every data block now reads as spaces: the old rows are gone from the media, not just
    // from the header's reach.
    std::uint8_t block[512];
    for (std::uint32_t lba = flight::RawBlockLog::kHeaderBlocks; lba < 16; ++lba) {
        CHECK(MemBlocks::rd(&mem, lba, block));
        CHECK(std::memcmp(block, "secret-row", 10) != 0);
        CHECK(block[0] == ' ');
    }

    // The boot count is the vehicle's life story, not the file's. An erased card must not
    // become indistinguishable from one that has never flown.
    CHECK(log.boot_count() == boot_before);

    // And the log is still usable: the scrub stopped at the write pointer, so appending
    // after it works and lands in the first data block.
    CHECK(log.append_line("after-erase", 11));
    CHECK(log.record_count() == 1);
    CHECK(MemBlocks::rd(&mem, flight::RawBlockLog::kHeaderBlocks, block));
    CHECK(std::memcmp(block, "after-erase", 11) == 0);
}

// The scrub walks down from the end while records append upward. They must never meet on
// the same block, because that is what lets the log stay open during a scrub that takes
// minutes on real hardware.
void test_a_scrub_never_overwrites_a_record_written_during_it() {
    MemBlocks mem(16);
    flight::RawBlockLog::Io io;
    io.ctx = &mem;
    io.read_block = &MemBlocks::rd;
    io.write_block = &MemBlocks::wr;

    flight::RawBlockLog log;
    CHECK(log.begin(io, 0, 16));
    for (int i = 0; i < 10; ++i) CHECK(log.append_line("old", 3));
    CHECK(log.reset());

    // Interleave: one scrub slice, one append, until the scrub gives up.
    int guard = 0;
    while (log.scrubbing() && ++guard < 100) {
        log.scrub_step(1);
        log.append_line("new", 3);
    }
    CHECK(!log.scrubbing());

    // Every record written during the scrub survived it.
    std::uint8_t block[512];
    for (std::uint32_t i = 0; i < log.record_count(); ++i) {
        CHECK(MemBlocks::rd(&mem, flight::RawBlockLog::kHeaderBlocks + i, block));
        CHECK(std::memcmp(block, "new", 3) == 0);
    }
}

void test_raw_block_log() {
    MemBlocks mem(8);
    flight::RawBlockLog::Io io;
    io.ctx = &mem;
    io.read_block = &MemBlocks::rd;
    io.write_block = &MemBlocks::wr;

    flight::RawBlockLog log;
    CHECK(log.begin(io, 0, 8));
    CHECK(log.boot_count() == 1);
    CHECK(log.append_line("row-1", 5));
    CHECK(log.append_line("row-2", 5));
    CHECK(log.append_line("row-3", 5));
    CHECK(log.record_count() == 3);

    // Simulate a reset: fresh object, same media -> resumes, boot count increments.
    flight::RawBlockLog log2;
    CHECK(log2.begin(io, 0, 8));
    CHECK(log2.boot_count() == 2);
    CHECK(log2.record_count() == 3);
    CHECK(log2.append_line("row-4", 5));
    CHECK(log2.append_line("row-5", 5));
    CHECK(log2.append_line("row-6", 5));
    CHECK(log2.record_count() == 6);
    // Two header blocks plus six records fills an eight-block region.
    CHECK(!log2.append_line("row-7", 5));
    CHECK(log2.full());

    // Stored record content is recoverable, starting after the two header blocks.
    std::uint8_t block[512];
    CHECK(MemBlocks::rd(&mem, flight::RawBlockLog::kHeaderBlocks, block));
    CHECK(std::memcmp(block, "row-1", 5) == 0);
    CHECK(block[5] == '\n');

    // A record longer than a block is cut, and the cut is counted rather than hidden.
    MemBlocks big(16);
    flight::RawBlockLog::Io big_io;
    big_io.ctx = &big;
    big_io.read_block = &MemBlocks::rd;
    big_io.write_block = &MemBlocks::wr;
    flight::RawBlockLog long_log;
    CHECK(long_log.begin(big_io, 0, 16));
    const std::string oversized(1000, 'x');
    CHECK(long_log.append_line(oversized.data(), oversized.size()));
    CHECK(long_log.truncated_records() == 1);
    CHECK(long_log.append_line("short", 5));
    CHECK(long_log.truncated_records() == 1);
}

// The header is rewritten after every record, so power can fail during that write. With a
// single header that would leave no valid resume point, and the next boot would restart at
// the first record block — overwriting the entire flight it had just recorded. Two
// alternating copies mean only one can ever be damaged.
void test_raw_block_log_survives_a_torn_header_write() {
    MemBlocks mem(16);
    flight::RawBlockLog::Io io;
    io.ctx = &mem;
    io.read_block = &MemBlocks::rd;
    io.write_block = &MemBlocks::wr;

    flight::RawBlockLog log;
    CHECK(log.begin(io, 0, 16));
    for (int i = 0; i < 5; ++i) CHECK(log.append_line("record", 6));
    CHECK(log.record_count() == 5);

    // Corrupt each header copy in turn, as an interrupted write would.
    for (std::uint32_t header = 0; header < flight::RawBlockLog::kHeaderBlocks; ++header) {
        MemBlocks damaged = mem;
        std::memset(damaged.blocks[header].data(), 0xFF, 512);
        flight::RawBlockLog::Io damaged_io;
        damaged_io.ctx = &damaged;
        damaged_io.read_block = &MemBlocks::rd;
        damaged_io.write_block = &MemBlocks::wr;

        flight::RawBlockLog resumed;
        CHECK(resumed.begin(damaged_io, 0, 16));
        // The surviving copy carries the resume point: no flight data is overwritten.
        CHECK(resumed.record_count() >= 4);
        CHECK(resumed.boot_count() >= 2);

        std::uint8_t block[512];
        CHECK(MemBlocks::rd(&damaged, flight::RawBlockLog::kHeaderBlocks, block));
        CHECK(std::memcmp(block, "record", 6) == 0);
    }

    // Both copies destroyed is unrecoverable, and must start cleanly rather than resume at
    // a block number read out of corrupted bytes.
    MemBlocks wiped = mem;
    for (std::uint32_t header = 0; header < flight::RawBlockLog::kHeaderBlocks; ++header) {
        std::memset(wiped.blocks[header].data(), 0x00, 512);
    }
    flight::RawBlockLog::Io wiped_io;
    wiped_io.ctx = &wiped;
    wiped_io.read_block = &MemBlocks::rd;
    wiped_io.write_block = &MemBlocks::wr;
    flight::RawBlockLog fresh;
    CHECK(fresh.begin(wiped_io, 0, 16));
    CHECK(fresh.record_count() == 0);
    CHECK(fresh.boot_count() == 1);

    // A region too small for the headers plus one record is refused outright.
    MemBlocks tiny(2);
    flight::RawBlockLog::Io tiny_io;
    tiny_io.ctx = &tiny;
    tiny_io.read_block = &MemBlocks::rd;
    tiny_io.write_block = &MemBlocks::wr;
    flight::RawBlockLog small;
    CHECK(!small.begin(tiny_io, 0, 2));
}

// ----------------------------------------------------------------------------
void test_controller_sequence_and_degradation() {
    flight::Configuration c;
    c.team_id = "CAN-Team-07";
    c.telemetry_period_ms = 500;
    c.radio.bandwidth_hz = 250000;  // 2 Hz needs the wider modem (link-budget.md)
    flight::test::MockImu imu;
    flight::test::MockBarometer baro;
    flight::test::MockGps gps;
    flight::test::MockRadio radio;
    flight::test::MockLogger logger;
    flight::test::MockBoard board;
    flight::Controller ctrl(c, imu, baro, gps, radio, logger, board);

    CHECK(ctrl.initialize());
    CHECK(board.led_on);
    CHECK(radio.sync_word_ == 0xF3);
    CHECK(ctrl.state() == flight::MissionState::ready);

    for (std::uint64_t t = 0; t <= 2000; t += 500) ctrl.poll(t);
    CHECK(radio.packets.size() == 5);
    CHECK(ctrl.packet_count() == 5);
    for (std::size_t i = 0; i < radio.packets.size(); ++i) {
        const auto parsed = cansat::parse_packet(radio.packets[i]);
        CHECK(static_cast<bool>(parsed));
        CHECK(parsed.record->packet_number == i + 1);
    }

    // Radio goes dead: numbering keeps advancing, nothing is delivered, no crash.
    radio.fail_tx = true;
    const std::uint32_t before = ctrl.packet_count();
    const std::size_t delivered_before = radio.packets.size();
    for (std::uint64_t t = 2500; t <= 6000; t += 500) ctrl.poll(t);
    CHECK(ctrl.packet_count() > before);
    CHECK(radio.packets.size() == delivered_before);  // nothing got through
    CHECK(ctrl.health().packets_tx_failed > 0);

    // Radio recovers: delivery resumes with strictly increasing, non-duplicated numbers.
    radio.fail_tx = false;
    for (std::uint64_t t = 6500; t <= 9000; t += 500) ctrl.poll(t);
    CHECK(radio.packets.size() > delivered_before);
    std::uint32_t last = 0;
    for (const auto& pkt : radio.packets) {
        const auto parsed = cansat::parse_packet(pkt);
        CHECK(static_cast<bool>(parsed));
        CHECK(parsed.record->packet_number > last);
        last = parsed.record->packet_number;
    }
}

void test_controller_sensor_failure_suppresses_but_continues() {
    flight::Configuration c;
    c.team_id = "CAN-Team-07";
    c.telemetry_period_ms = 500;
    c.radio.bandwidth_hz = 250000;  // 2 Hz needs the wider modem (link-budget.md)
    c.sensor_stale_after_ms = 400;
    flight::test::MockImu imu;
    flight::test::MockBarometer baro;
    flight::test::MockGps gps;
    flight::test::MockRadio radio;
    flight::test::MockLogger logger;
    flight::test::MockBoard board;
    flight::Controller ctrl(c, imu, baro, gps, radio, logger, board);
    CHECK(ctrl.initialize());
    ctrl.poll(0);
    CHECK(radio.packets.size() == 1);

    // Barometer dies -> mandatory data incomplete -> packets suppressed, loop alive.
    baro.fail_read = true;
    for (std::uint64_t t = 500; t <= 3000; t += 500) ctrl.poll(t);
    const std::size_t after_fail = radio.packets.size();
    CHECK(ctrl.health().packets_suppressed > 0);

    // Barometer recovers -> telemetry resumes.
    baro.fail_read = false;
    for (std::uint64_t t = 3500; t <= 5000; t += 500) ctrl.poll(t);
    CHECK(radio.packets.size() > after_fail);
    CHECK(ctrl.state() != flight::MissionState::fault);  // single-sensor loss is not critical
}

static flight::Configuration fast_arm_config() {
    flight::Configuration c;
    c.team_id = "CAN-Team-07";
    c.telemetry_period_ms = 500;
    c.radio.bandwidth_hz = 250000;  // 2 Hz needs the wider modem (link-budget.md)
    c.sensor_period_ms = 100;
    c.launch_accel_mps2 = 30.0;
    c.launch_confirm_ms = 100;
    c.calib_samples = 6;
    c.calib_timeout_ms = 100000;
    c.arming_delay_ms = 400;
    c.health_period_ms = 100;  // keep health() current for assertions
    return c;
}

void test_controller_launch_detection() {
    const flight::Configuration c = fast_arm_config();
    flight::test::MockImu imu;
    flight::test::MockBarometer baro;
    flight::test::MockGps gps;
    flight::test::MockRadio radio;
    flight::test::MockLogger logger;
    flight::test::MockBoard board;
    flight::Controller ctrl(c, imu, baro, gps, radio, logger, board);
    CHECK(ctrl.initialize());
    ctrl.poll(0);
    CHECK(ctrl.state() == flight::MissionState::ready);

    // Hold still through calibration + the arming delay.
    for (std::uint64_t t = 100; t <= 800; t += 100) ctrl.poll(t);
    CHECK(ctrl.health().calibrated);
    CHECK(ctrl.health().armed);
    CHECK(ctrl.state() == flight::MissionState::ready);

    // Now a sustained boost -> FLIGHT.
    imu.sample.az_mps2 = 55.0;
    for (std::uint64_t t = 900; t <= 1600; t += 100) ctrl.poll(t);
    CHECK(ctrl.state() == flight::MissionState::flight);
}

void test_controller_arming_lockout_blocks_early_boost() {
    const flight::Configuration c = fast_arm_config();
    flight::test::MockImu imu;
    flight::test::MockBarometer baro;
    flight::test::MockGps gps;
    flight::test::MockRadio radio;
    flight::test::MockLogger logger;
    flight::test::MockBoard board;
    flight::Controller ctrl(c, imu, baro, gps, radio, logger, board);
    CHECK(ctrl.initialize());
    ctrl.poll(0);

    // Boost before the arming delay elapses: the lockout keeps the vehicle in READY,
    // so a startup sensor glitch cannot trigger a false launch.
    imu.sample.az_mps2 = 55.0;
    for (std::uint64_t t = 50; t <= 350; t += 50) ctrl.poll(t);
    CHECK(!ctrl.health().armed);
    CHECK(ctrl.state() == flight::MissionState::ready);
}

void test_startup_calibrator_stationary_and_moving() {
    flight::Configuration c;
    c.team_id = "CAN-Team-07";
    c.calib_samples = 10;
    c.calib_timeout_ms = 100000;

    // --- stationary: gyro bias + baro reference are captured ---
    flight::StartupCalibrator cal(c);
    flight::ImuSample s{};
    s.valid = true;
    s.ax_mps2 = 0.0; s.ay_mps2 = 0.0; s.az_mps2 = flight::sensors::kStandardGravity;
    s.gx_dps = 0.4; s.gy_dps = -0.7; s.gz_dps = 2.1;  // constant offset == bias
    flight::BaroSample b{};
    b.valid = true; b.pressure_pa = 100000.0; b.temperature_c = 20.0;
    b.altitude_m = flight::sensors::pressure_altitude_m(100000.0, 101325.0);
    for (int i = 0; i < 14; ++i) { cal.add_imu(s); cal.add_baro(b); cal.update(i * 100); }
    CHECK(cal.complete());
    CHECK(!cal.failed());
    CHECK(cal.result().gyro_bias_valid);
    CHECK(approx(cal.result().gyro_bias_dps[0], 0.4, 1e-6));
    CHECK(approx(cal.result().gyro_bias_dps[2], 2.1, 1e-6));
    CHECK(cal.result().baro_reference_valid);
    CHECK(approx(cal.result().ground_pressure_pa, 100000.0, 1e-6));
    // A perfect 1 g at rest needs no scale correction at all.
    CHECK(cal.result().accel_reference_valid);
    CHECK(approx(cal.result().accel_scale, 1.0, 1e-9));
    CHECK(approx(cal.result().accel_magnitude_ref, flight::sensors::kStandardGravity, 1e-9));

    // --- moving: never accepts, resolves best-effort at the timeout, bias NOT trusted ---
    flight::Configuration c2 = c;
    c2.calib_timeout_ms = 1500;
    flight::StartupCalibrator cal2(c2);
    for (int i = 0; i < 60; ++i) {
        flight::ImuSample m{};
        m.valid = true;
        m.ax_mps2 = 0.0; m.ay_mps2 = 0.0; m.az_mps2 = flight::sensors::kStandardGravity;
        m.gz_dps = (i % 2) ? 60.0 : -60.0;  // large per-axis variance
        cal2.add_imu(m);
        cal2.update(static_cast<std::uint64_t>(i) * 50);
    }
    CHECK(cal2.failed());
    CHECK(cal2.result().motion_detected);
    CHECK(!cal2.result().gyro_bias_valid);
}

void test_controller_sensor_plausibility() {
    flight::Configuration c;
    c.team_id = "CAN-Team-07";
    c.telemetry_period_ms = 500;
    c.radio.bandwidth_hz = 250000;  // 2 Hz needs the wider modem (link-budget.md)
    c.sensor_stale_after_ms = 100000;  // isolate the plausibility path from staleness
    flight::test::MockImu imu;
    flight::test::MockBarometer baro;
    flight::test::MockGps gps;
    flight::test::MockRadio radio;
    flight::test::MockLogger logger;
    flight::test::MockBoard board;
    flight::Controller ctrl(c, imu, baro, gps, radio, logger, board);
    CHECK(ctrl.initialize());
    ctrl.poll(0);
    CHECK(radio.packets.size() == 1);

    // Barometer starts returning an impossible pressure (below the BMP280 spec floor).
    baro.sample.pressure_pa = 5000.0;
    for (std::uint64_t t = 500; t <= 3000; t += 500) ctrl.poll(t);
    CHECK(ctrl.faults().active(flight::FaultCode::sensor_implausible));
    CHECK(ctrl.health().packets_suppressed > 0);          // mandatory data incomplete
    CHECK(ctrl.state() != flight::MissionState::fault);    // one bad sensor is not critical

    // Recovers: fault clears and telemetry resumes.
    baro.sample.pressure_pa = 101325.0;
    const std::size_t before = radio.packets.size();
    for (std::uint64_t t = 3500; t <= 5000; t += 500) ctrl.poll(t);
    CHECK(!ctrl.faults().active(flight::FaultCode::sensor_implausible));
    CHECK(radio.packets.size() > before);
}

// The shared protocol fixtures. The same file drives the Python ground station and the
// JavaScript web console, so all three parsers are held to one definition of a valid
// packet. A ground station that disagrees with the transmitter miscounts packet loss.
void test_shared_protocol_fixtures(const std::string& repo_root) {
    const std::string path = repo_root + "/test-data/protocol-fixtures.tsv";
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
        std::cerr << "  could not open " << path
                  << " (pass the repository root as argv[1])\n";
        return;
    }

    int cases = 0;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;

        const std::size_t first = line.find('\t');
        const std::size_t second = line.find('\t', first + 1);
        CHECK(first != std::string::npos && second != std::string::npos);
        if (first == std::string::npos || second == std::string::npos) continue;

        const std::string id = line.substr(0, first);
        const std::string expect = line.substr(first + 1, second - first - 1);
        const std::string packet = line.substr(second + 1);

        const bool parsed = static_cast<bool>(cansat::parse_packet(packet));
        const bool want = expect == "ok";
        ++g_checks;
        if (parsed != want) {
            std::cerr << "FAIL fixture " << id << ": expected " << expect << ", got "
                      << (parsed ? "ok" : "err") << "\n";
            ++g_failures;
        }
        ++cases;
    }
    CHECK(cases >= 30);  // the whole fixture set, not a truncated read
}

// A GPS whose lead is pulled off -- at parachute deployment, or by the impact -- leaves
// its last good fix sitting in the NMEA parser. The parser has no clock and cannot know
// the module stopped talking, so without an age check the vehicle would keep reporting
// that position in every remaining packet and the recovery team would be sent to where
// the payload was several minutes earlier.
void test_a_frozen_gps_fix_is_not_reported_as_a_live_position() {
    flight::Configuration c;
    c.team_id = "CAN-Team-07";
    c.telemetry_period_ms = 500;
    c.radio.bandwidth_hz = 250000;  // 2 Hz needs the wider modem (link-budget.md)
    c.gps_fix_timeout_ms = 3000;
    // This suite is about a fix ageing out of *telemetry*, so it needs the fix on the air.
    // The default leaves GPS in the log only -- see Configuration::transmit_gps.
    c.transmit_gps = true;
    c.worst_case_packet_bytes = cansat::link::kWorstCasePacketBytesWithGps;
    flight::test::MockImu imu;
    flight::test::MockBarometer baro;
    flight::test::MockGps gps;
    flight::test::MockRadio radio;
    flight::test::MockLogger logger;
    flight::test::MockBoard board;
    flight::Controller ctrl(c, imu, baro, gps, radio, logger, board);
    CHECK(ctrl.initialize());

    // A talking receiver: the fix reaches telemetry.
    for (std::uint64_t t = 0; t <= 1000; t += 500) ctrl.poll(t);
    CHECK(radio.packets.back().find("GP-Lat-") != std::string::npos);

    // The lead comes off. latest() still hands out the same fix -- the parser kept it --
    // but nothing renews it, so it must age out rather than be transmitted forever.
    gps.silent = true;
    for (std::uint64_t t = 1500; t <= 3500; t += 500) ctrl.poll(t);
    // 2.5 s old: inside the timeout, still trusted.
    CHECK(radio.packets.back().find("GP-Lat-") != std::string::npos);

    for (std::uint64_t t = 4000; t <= 6000; t += 500) ctrl.poll(t);
    // Past the timeout: the position is withdrawn from the packet, not frozen into it.
    CHECK(radio.packets.back().find("GP-Lat-") == std::string::npos);

    // The loss is announced rather than passed over in silence.
    CHECK(ctrl.faults().active(flight::FaultCode::gps_unavailable));

    // The receiver comes back: the fix is trusted again, and the fault clears.
    gps.silent = false;
    for (std::uint64_t t = 6500; t <= 7500; t += 500) ctrl.poll(t);
    CHECK(radio.packets.back().find("GP-Lat-") != std::string::npos);
    CHECK(!ctrl.faults().active(flight::FaultCode::gps_unavailable));
}

// The fix timeout is bounded below by the receiver's own navigation rate: a timeout
// shorter than one update period would expire a live fix between updates.
void test_config_rejects_a_gps_timeout_faster_than_the_receiver() {
    flight::Configuration c;
    c.team_id = "CAN-Team-07";
    std::string why;
    CHECK(flight::validate_config(c, why));

    c.gps_fix_timeout_ms = 500;
    CHECK(!flight::validate_config(c, why));
    CHECK(why.find("gps_fix_timeout_ms") != std::string::npos);

    c.gps_fix_timeout_ms = 3000;
    c.gps_silence_after_ms = 250;
    CHECK(!flight::validate_config(c, why));
    CHECK(why.find("gps_silence_after_ms") != std::string::npos);
}


// ---------------------------------------------------------------------------
// Magnetometer integration at the controller level.

// Every packet must say which kind of yaw it carries, because the two are not
// interchangeable and the ground station cannot tell them apart from the number alone.
void test_telemetry_declares_the_yaw_reference() {
    flight::Configuration c = fast_arm_config();
    // Ship a calibration, as a bench-calibrated vehicle would.
    c.mag_calibration.valid = true;
    flight::test::MockImu imu;
    flight::test::MockBarometer baro;
    flight::test::MockGps gps;
    flight::test::MockRadio radio;
    flight::test::MockLogger logger;
    flight::test::MockBoard board;
    flight::Controller ctrl(c, imu, baro, gps, radio, logger, board);
    CHECK(ctrl.initialize());
    for (std::uint64_t t = 0; t <= 2000; t += 100) ctrl.poll(t);

    CHECK(!radio.packets.empty());
    // The magnetometer has been feeding the estimator throughout, so by now yaw is
    // magnetic and the tag says so.
    CHECK(radio.packets.back().find("YR-M") != std::string::npos);
    CHECK(ctrl.health().mag_present);
    CHECK(ctrl.health().mag_ok);
    CHECK(ctrl.health().mag_calibrated);
    CHECK(ctrl.health().yaw_is_magnetic);
    CHECK(ctrl.health().mag_field_ut > 20.0 && ctrl.health().mag_field_ut < 70.0);
    // Heading is a bearing: inside [0, 360).
    CHECK(ctrl.health().heading_deg >= 0.0 && ctrl.health().heading_deg < 360.0);
    // Every packet the vehicle produced is still a legal one.
    for (const std::string& packet : radio.packets) {
        CHECK(static_cast<bool>(cansat::parse_packet(packet)));
    }
}

// A module sold as an MPU-9250 that turns out to be an MPU-6500 has no magnetometer. The
// vehicle must fly on six axes and say so, not refuse to start and not pretend.
void test_a_missing_magnetometer_degrades_rather_than_stops() {
    flight::Configuration c = fast_arm_config();
    c.mag_calibration.valid = true;
    flight::test::MockImu imu;
    imu.magnetometer = false;
    flight::test::MockBarometer baro;
    flight::test::MockGps gps;
    flight::test::MockRadio radio;
    flight::test::MockLogger logger;
    flight::test::MockBoard board;
    flight::Controller ctrl(c, imu, baro, gps, radio, logger, board);

    CHECK(ctrl.initialize());  // still operational
    CHECK(ctrl.faults().active(flight::FaultCode::mag_unavailable));
    for (std::uint64_t t = 0; t <= 2000; t += 100) ctrl.poll(t);

    CHECK(!ctrl.health().mag_present);
    CHECK(!ctrl.health().mag_ok);
    CHECK(!ctrl.health().yaw_is_magnetic);
    CHECK(!radio.packets.empty());
    CHECK(radio.packets.back().find("YR-G") != std::string::npos);
    // Roll, pitch and acceleration are unaffected: only yaw was ever magnetic.
    CHECK(ctrl.health().orientation_ok);
    CHECK(ctrl.health().imu_ok);
    // And it still reaches READY, so the mission is not blocked by a substituted part.
    CHECK(ctrl.state() == flight::MissionState::ready);
}

// A magnetometer that stops answering mid-flight must cost yaw, not the vehicle.
void test_a_magnetometer_that_stops_is_reported_and_survived() {
    flight::Configuration c = fast_arm_config();
    c.mag_calibration.valid = true;
    c.sensor_stale_after_ms = 500;
    flight::test::MockImu imu;
    flight::test::MockBarometer baro;
    flight::test::MockGps gps;
    flight::test::MockRadio radio;
    flight::test::MockLogger logger;
    flight::test::MockBoard board;
    flight::Controller ctrl(c, imu, baro, gps, radio, logger, board);
    CHECK(ctrl.initialize());
    for (std::uint64_t t = 0; t <= 1000; t += 100) ctrl.poll(t);
    CHECK(ctrl.health().yaw_is_magnetic);

    imu.sample.mag_valid = false;  // the AK8963 stops raising data-ready
    for (std::uint64_t t = 1100; t <= 3000; t += 100) ctrl.poll(t);

    CHECK(ctrl.faults().active(flight::FaultCode::mag_unavailable));
    CHECK(!ctrl.health().mag_ok);
    CHECK(!ctrl.health().yaw_is_magnetic);
    // The accelerometer and gyroscope are untouched, so attitude and telemetry continue.
    CHECK(ctrl.health().imu_ok);
    CHECK(ctrl.health().orientation_ok);
    CHECK(radio.packets.back().find("YR-G") != std::string::npos);
    CHECK(!ctrl.faults().active(flight::FaultCode::imu_stale));
}

// Course over ground is a cross-check and nothing more. It must raise a warning when it
// disagrees grossly, and it must never move the attitude solution.
void test_gps_course_is_a_cross_check_not_a_yaw_source() {
    flight::Configuration c = fast_arm_config();
    c.mag_calibration.valid = true;
    c.yaw_cog_min_speed_mps = 5.0;
    c.yaw_cog_tolerance_deg = 60.0;
    c.yaw_cog_confirm_samples = 3;

    flight::test::MockImu imu;
    flight::test::MockBarometer baro;
    flight::test::MockGps gps;
    flight::test::MockRadio radio;
    flight::test::MockLogger logger;
    flight::test::MockBoard board;
    flight::Controller ctrl(c, imu, baro, gps, radio, logger, board);
    CHECK(ctrl.initialize());

    // Agreeing, and moving: no warning. The mock IMU faces magnetic north, so the
    // vehicle's heading is ~0 and a course of 10 degrees is well inside tolerance.
    gps.fix.course_valid = true;
    gps.fix.course_deg = 10.0;
    gps.fix.speed_mps = 12.0;
    for (std::uint64_t t = 0; t <= 2000; t += 100) ctrl.poll(t);
    const double settled_yaw = ctrl.health().heading_deg;
    CHECK(!ctrl.faults().active(flight::FaultCode::yaw_reference_disagreement));

    // Now the course swings to the opposite side while the vehicle keeps pointing north:
    // a warning after the confirmation count, and the heading does NOT follow it.
    gps.fix.course_deg = 200.0;
    for (std::uint64_t t = 2100; t <= 4000; t += 100) ctrl.poll(t);
    CHECK(ctrl.faults().active(flight::FaultCode::yaw_reference_disagreement));
    CHECK(approx(flight::angle_difference_deg(ctrl.health().heading_deg, settled_yaw), 0.0,
                 2.0));
    // A warning only: the mission keeps running and telemetry keeps flowing.
    CHECK(ctrl.state() != flight::MissionState::fault);

    // Slowing below the speed gate withdraws the comparison rather than the heading.
    gps.fix.speed_mps = 0.5;
    for (std::uint64_t t = 4100; t <= 5000; t += 100) ctrl.poll(t);
    CHECK(ctrl.health().yaw_is_magnetic);
}

// The runtime figure-of-eight calibration must refuse to certify itself until every axis
// has actually been swept -- that refusal is what stops a bogus absolute heading.
void test_mag_calibration_requires_real_coverage() {
    flight::Configuration c;
    c.mag_cal_min_samples = 10;
    c.mag_cal_min_span_ut = 30.0;
    flight::MagCalibrator cal(c);

    // Rotating about one axis only: two axes sweep, the third never does.
    for (int i = 0; i < 360; i += 5) {
        const double a = i * 3.14159265358979323846 / 180.0;
        cal.add(40.0 * std::cos(a), 40.0 * std::sin(a), -17.0);
    }
    CHECK(cal.samples() > 10);
    CHECK(!cal.coverage_met());
    CHECK(!cal.result().valid);

    // Add the missing sweep, with a deliberate hard-iron offset and a squashed X axis.
    flight::MagCalibrator full(c);
    const double offset[3] = {12.0, -8.0, 5.0};
    for (int i = 0; i < 360; i += 5) {
        const double a = i * 3.14159265358979323846 / 180.0;
        // Two great circles cover all three axes; X is scaled to 0.5 to give the soft
        // iron term something to find.
        full.add(offset[0] + 0.5 * 40.0 * std::cos(a), offset[1] + 40.0 * std::sin(a),
                 offset[2]);
        full.add(offset[0] + 0.5 * 40.0 * std::cos(a), offset[1],
                 offset[2] + 40.0 * std::sin(a));
    }
    CHECK(full.coverage_met());
    const auto result = full.result();
    CHECK(result.valid);
    for (int i = 0; i < 3; ++i) {
        CHECK(approx(result.offset_ut[i], offset[i], 1.0));
    }
    // Half-widths of 20, 40 and 40 uT give a mean of 33.3, so X is scaled up by 5/3 and
    // Y and Z down by 5/6. What matters is the ratio between them: X must end up twice
    // the scale of the other two, which is exactly the squash that was applied.
    CHECK(approx(result.scale[0], (100.0 / 3.0) / 20.0, 0.05));
    CHECK(approx(result.scale[1], (100.0 / 3.0) / 40.0, 0.05));
    CHECK(approx(result.scale[2], (100.0 / 3.0) / 40.0, 0.05));
    CHECK(approx(result.scale[0] / result.scale[1], 2.0, 0.05));

    // A correction that reports a valid calibration must actually round the field out:
    // corrected samples all land on one sphere, whatever direction they came from.
    double radius_min = 1e9, radius_max = 0.0;
    for (int i = 0; i < 360; i += 15) {
        const double a = i * 3.14159265358979323846 / 180.0;
        double x = offset[0] + 0.5 * 40.0 * std::cos(a);
        double y = offset[1] + 40.0 * std::sin(a);
        double z = offset[2];
        flight::sensors::apply_mag_calibration(result, x, y, z);
        const double r = flight::sensors::vector_magnitude(x, y, z);
        if (r < radius_min) radius_min = r;
        if (r > radius_max) radius_max = r;
    }
    CHECK((radius_max - radius_min) < 0.05 * radius_max);

    // Saturated or absent readings must never stretch the bounding box.
    flight::MagCalibrator guarded(c);
    guarded.add(4000.0, 4000.0, 4000.0);
    guarded.add(0.0, 0.0, 0.0);
    CHECK(guarded.samples() == 0);
}

// The accelerometer correction is a scale, not an offset, so it has to survive rotation.
void test_accel_calibration_is_rotation_invariant() {
    flight::Configuration c;
    c.team_id = "CAN-Team-07";
    c.calib_samples = 8;
    c.calib_timeout_ms = 100000;

    // A part reading 2 % high, resting upright.
    const double high = 1.02;
    flight::StartupCalibrator cal(c);
    flight::ImuSample s{};
    s.valid = true;
    s.az_mps2 = flight::sensors::kStandardGravity * high;
    for (int i = 0; i < 12; ++i) {
        cal.add_imu(s);
        cal.update(static_cast<std::uint64_t>(i) * 100);
    }
    CHECK(cal.complete());
    CHECK(cal.result().accel_reference_valid);
    CHECK(approx(cal.result().accel_scale, 1.0 / high, 1e-9));

    // Applying that scale in ANY attitude returns one standard gravity. An offset vector
    // fitted upright would only have been right while the vehicle stayed upright.
    const double scale = cal.result().accel_scale;
    for (const double roll : {0.0, 37.0, 90.0, 143.0, -85.0}) {
        double ax, ay, az;
        body_gravity(roll, 0.0, ax, ay, az);
        const double magnitude = flight::sensors::vector_magnitude(
            ax * high * scale, ay * high * scale, az * high * scale);
        CHECK(approx(magnitude, flight::sensors::kStandardGravity, 1e-9));
    }
}

// The airtime budget rests on measured packet sizes, and adding a field to every packet
// is exactly the change that quietly invalidates it. These are the three figures
// documentation/design/link-budget.md quotes; they are asserted here so a change to the
// format cannot make that document wrong without failing a test.
void test_measured_packet_sizes_match_the_link_budget() {
    flight::Configuration c;
    c.team_id = "CAN-Team-07";
    // The sizes this pins are the GPS-inclusive ones the 255-byte budget is built from.
    c.transmit_gps = true;
    c.worst_case_packet_bytes = cansat::link::kWorstCasePacketBytesWithGps;
    flight::TelemetryBuilder builder(c);

    flight::SensorSnapshot s;
    s.imu_valid = s.orientation_valid = s.baro_valid = true;
    s.altitude_m = 123.4;
    s.pressure_pa = 101325.00;
    s.temperature_c = 25.0;
    s.roll_deg = 1.0;
    s.pitch_deg = 2.0;
    s.yaw_deg = 3.0;
    s.ax_mps2 = 0.1;
    s.ay_mps2 = 0.2;
    s.az_mps2 = 9.8;

    const auto mandatory = builder.build(1, 1000, s, {});
    CHECK(mandatory.has_value());
    CHECK(mandatory->packet.size() == 118);

    s.gps.valid = true;
    s.gps.latitude = 18.5;
    s.gps.longitude = 73.8;
    s.gps.altitude = 15.0;
    const auto with_gps = builder.build(1, 1000, s, {});
    CHECK(with_gps.has_value());
    CHECK(with_gps->packet.size() == 167);

    // The in-flight packet: GPS plus every diagnostic tag, including the yaw-reference
    // tag the nine-axis upgrade added. Six bytes more than before it existed.
    const std::vector<std::string> tags = {"MODE-RECOVERY", "FAULTS-3", "CAL-1", "ARM-1",
                                           "YR-M"};
    const auto full = builder.build(1, 1000, s, tags);
    CHECK(full.has_value());
    CHECK(full->packet.size() == 212);
    CHECK(full->packet.size() <= c.worst_case_packet_bytes);
    CHECK(static_cast<bool>(cansat::parse_packet(full->packet)));

    // The absolute worst case is not bounded by the format -- the team identifier has no
    // length limit, and extreme values widen every field -- so it is bounded by the
    // controller shedding optional content instead. Demonstrate that the builder really
    // can exceed the FIFO, which is what makes that shedding necessary rather than
    // theoretical.
    flight::SensorSnapshot extreme = s;
    extreme.altitude_m = -9999.9;
    extreme.pressure_pa = -110000.55;
    extreme.temperature_c = -55.5;
    extreme.roll_deg = extreme.pitch_deg = extreme.yaw_deg = -179.9;
    extreme.ax_mps2 = extreme.ay_mps2 = extreme.az_mps2 = -157.99;
    extreme.gps.latitude = -12.345678;
    extreme.gps.longitude = -123.456789;
    extreme.gps.altitude = -1234.5;
    const auto worst = builder.build(
        4294967295u, 359999999u, extreme,
        {"MODE-RECOVERY", "FAULTS-4294967295", "CAL-0", "ARM-0", "YR-M"});
    CHECK(worst.has_value());
    CHECK(worst->packet.size() > cansat::kMaxLoraPayloadBytes);
}

}  // namespace

// ---- Analogue microphone (additional sensor) -------------------------------------
//
// The rule this whole feature is built around: an additional sensor may add a column to the
// log and may never touch mandatory telemetry. Every test below is a way of failing that.

void test_sound_level_reduces_a_window_to_its_envelope() {
    // 3300 mV over 4095 codes is 0.80586 mV per code, so a 1000-code span is 805.9 mV.
    flight::SoundWindow w;
    w.min_counts = 1500;
    w.max_counts = 2500;
    w.sample_count = 256;
    const double mv = flight::sound_peak_to_peak_mv(w, 3300.0, 4095);
    CHECK(mv > 805.0 && mv < 806.9);

    // The level is the SPAN, not the position. A window sitting at a different bias with
    // the same span is the same loudness, and a reading that moved with the bias would be
    // tracking the module's trimpot rather than the sound.
    flight::SoundWindow shifted = w;
    shifted.min_counts = 500;
    shifted.max_counts = 1500;
    CHECK(flight::sound_peak_to_peak_mv(shifted, 3300.0, 4095) == mv);

    // Silence is zero, and zero is a real reading rather than an absent one.
    flight::SoundWindow quiet;
    quiet.min_counts = 2048;
    quiet.max_counts = 2048;
    quiet.sample_count = 256;
    CHECK(flight::sound_peak_to_peak_mv(quiet, 3300.0, 4095) == 0.0);
}

void test_sound_level_refuses_a_window_it_cannot_scale() {
    flight::SoundWindow empty;
    empty.min_counts = 0;
    empty.max_counts = 4095;
    empty.sample_count = 0;          // nothing was sampled
    CHECK(flight::sound_peak_to_peak_mv(empty, 3300.0, 4095) == 0.0);

    flight::SoundWindow w;
    w.min_counts = 100;
    w.max_counts = 900;
    w.sample_count = 64;
    CHECK(flight::sound_peak_to_peak_mv(w, 3300.0, 0) == 0.0);     // no full scale
    CHECK(flight::sound_peak_to_peak_mv(w, 0.0, 4095) == 0.0);     // no reference
    CHECK(flight::sound_peak_to_peak_mv(w, -3300.0, 4095) == 0.0);

    // A window that was never filled leaves max below min. Report nothing rather than a
    // negative loudness.
    flight::SoundWindow unfilled;
    unfilled.min_counts = 4095;
    unfilled.max_counts = 0;
    unfilled.sample_count = 32;
    CHECK(flight::sound_peak_to_peak_mv(unfilled, 3300.0, 4095) == 0.0);
}

// Canopy inflation and touchdown are the loudest things in the flight and the two most
// likely to saturate a gain set for ambient noise. A clipped window is a LOWER BOUND, and
// folding that into the number silently would misreport exactly the two events the sensor
// is carried for.
void test_a_clipped_window_is_reported_as_clipped() {
    flight::SoundWindow at_top;
    at_top.min_counts = 1000;
    at_top.max_counts = 4095;
    at_top.sample_count = 256;
    CHECK(flight::sound_window_clipped(at_top, 4095));

    flight::SoundWindow at_bottom;
    at_bottom.min_counts = 0;
    at_bottom.max_counts = 3000;
    at_bottom.sample_count = 256;
    CHECK(flight::sound_window_clipped(at_bottom, 4095));

    flight::SoundWindow inside;
    inside.min_counts = 1000;
    inside.max_counts = 3000;
    inside.sample_count = 256;
    CHECK(!flight::sound_window_clipped(inside, 4095));

    flight::SoundWindow nothing;
    nothing.sample_count = 0;
    CHECK(!flight::sound_window_clipped(nothing, 4095));
}

// The point of the whole design: the level reaches the SD log and never reaches the packet.
// The rulebook makes optional sensor data optional, and every byte of the packet is airtime
// the mandatory fields need more.
void test_the_sound_level_is_logged_and_never_transmitted() {
    flight::Configuration c;
    c.team_id = "CAN-Team-25";
    flight::TelemetryBuilder builder(c);

    flight::SensorSnapshot s;
    s.imu_valid = true;
    s.baro_valid = true;
    s.orientation_valid = true;
    s.sound_mv_pp = 412.5;
    s.sound_valid = true;
    s.sound_clipped = true;

    const auto built = builder.build(1, 1000, s);
    CHECK(built.has_value());

    // Not in the packet, under any spelling.
    CHECK(built->packet.find("412.5") == std::string::npos);
    CHECK(built->packet.find("SND") == std::string::npos);
    CHECK(built->packet.find("SOUND") == std::string::npos);

    // In the log, in the columns the header names.
    const std::string header = flight::TelemetryBuilder::sd_header();
    CHECK(header.find("sound_mv_pp,sound_clipped,sound_gate_pct,packet") != std::string::npos);
    const std::string line = builder.sd_line(*built, flight::MissionState::flight, 0);
    CHECK(line.find(",412.5,1,") != std::string::npos);

    // The header and the row must carry the same number of columns, or every reader of the
    // CSV is silently misaligned from the moment a column was added.
    std::size_t header_commas = 0;
    for (char ch : header) {
        if (ch == 0x2C) ++header_commas;
    }
    std::size_t line_commas = 0;
    for (char ch : line) {
        if (ch == 0x2C) ++line_commas;
    }
    CHECK(header_commas == line_commas);
}

// [F-18] The gate refuses a fix on satellite count and HDOP, and neither number was written
// anywhere -- not the SD row, not the packet. So a log could show a position wandering 50 m
// and say nothing about why, and the thresholds could not be tuned against real data. The
// quantity a decision turns on has to be recorded next to the decision.
void test_the_log_records_the_numbers_the_gps_gate_judges_on() {
    flight::Configuration c;
    c.team_id = "CAN-Team-25";
    flight::TelemetryBuilder builder(c);

    flight::SensorSnapshot s;
    s.imu_valid = true;
    s.baro_valid = true;
    s.orientation_valid = true;
    cansat::GpsData fix;
    fix.valid = true;
    fix.latitude = 21.220094;
    fix.longitude = 72.884836;
    fix.altitude = 15.0;
    fix.satellites = 9;
    fix.hdop = 1.4;
    s.gps = fix;   // gps.valid is the fix flag

    const std::string header = flight::TelemetryBuilder::sd_header();
    CHECK(header.find("gps_alt,gps_satellites,gps_hdop,sound_mv_pp") != std::string::npos);

    const auto built = builder.build(1, 1000, s);
    CHECK(built.has_value());
    const std::string line = builder.sd_line(*built, flight::MissionState::flight, 0);
    CHECK(line.find(",9,1.4,") != std::string::npos);

    // Still absent from the radio packet: the rulebook fixes that format, and airtime is
    // the constraint the whole telemetry rate was computed from.
    CHECK(built->packet.find("HDOP") == std::string::npos);
    CHECK(built->packet.find("SAT") == std::string::npos);
}

// No fix means no quality either. A zero satellite count and a zero HDOP are both readings a
// receiver can produce, and HDOP 0 is the best geometry there is -- writing either where
// there was no fix at all invents data that would pass any gate.
void test_no_fix_leaves_the_gps_quality_columns_blank() {
    flight::Configuration c;
    c.team_id = "CAN-Team-25";
    flight::TelemetryBuilder builder(c);

    flight::SensorSnapshot s;
    s.imu_valid = true;
    s.baro_valid = true;
    s.orientation_valid = true;
    // s.gps left default: valid == false, no fix

    const auto built = builder.build(1, 1000, s);
    CHECK(built.has_value());
    const std::string line = builder.sd_line(*built, flight::MissionState::flight, 0);

    // Split rather than search. ",0,0.0," also matches state,fault_total,altitude further
    // up the row, so a substring test here passes or fails for the wrong reason.
    std::vector<std::string> cells;
    std::string cell;
    for (const char ch : line) {
        if (ch == 0x2C) { cells.push_back(cell); cell.clear(); } else { cell += ch; }
    }
    cells.push_back(cell);

    const std::string header = flight::TelemetryBuilder::sd_header();
    std::vector<std::string> names;
    cell.clear();
    for (const char ch : header) {
        if (ch == 0x2C) { names.push_back(cell); cell.clear(); } else { cell += ch; }
    }
    names.push_back(cell);

    CHECK(names.size() == cells.size());
    for (std::size_t i = 0; i < names.size(); ++i) {
        if (names[i] == "gps_satellites" || names[i] == "gps_hdop" ||
            names[i] == "gps_lat" || names[i] == "gps_lon" || names[i] == "gps_alt") {
            CHECK(cells[i].empty());
        }
    }
    CHECK(cells[13] == "0");   // gps_valid, and it is the one that says why the rest are blank
}

// An unfitted microphone must be distinguishable from a silent one. Zero is a level a
// working sensor reports; a blank is the absence of a measurement, and a column that cannot
// tell those apart is worse than no column.
void test_an_absent_microphone_leaves_the_columns_blank_rather_than_zero() {
    flight::Configuration c;
    c.team_id = "CAN-Team-25";
    flight::TelemetryBuilder builder(c);

    flight::SensorSnapshot s;
    s.imu_valid = true;
    s.baro_valid = true;
    s.orientation_valid = true;
    s.sound_valid = false;          // no microphone on this vehicle

    const auto built = builder.build(1, 1000, s);
    CHECK(built.has_value());
    const std::string line = builder.sd_line(*built, flight::MissionState::flight, 0);
    CHECK(line.find(",,,,") != std::string::npos);  // all three sound columns empty
    CHECK(line.find(",0.0,0,") == std::string::npos);
}

// A vehicle built without a microphone is a legitimate build. The controller takes it as a
// pointer for that reason, and a null one must change nothing at all.
void test_a_vehicle_without_a_microphone_behaves_as_before() {
    flight::Configuration c;
    c.team_id = "CAN-Team-25";
    flight::test::MockImu imu;
    flight::test::MockBarometer baro;
    flight::test::MockGps gps;
    flight::test::MockRadio radio;
    flight::test::MockLogger logger;
    flight::test::MockBoard board;

    flight::Controller ctrl(c, imu, baro, gps, radio, logger, board);   // no sound sensor
    CHECK(ctrl.initialize());
    for (std::uint64_t t = 0; t <= 2000; t += 10) {
        ctrl.poll(t);
    }
    CHECK(ctrl.health().packets_sent > 0);
    CHECK(!ctrl.health().sound_ok);
    CHECK(!ctrl.faults().active(flight::FaultCode::sound_unavailable));
}

// GPS is not a mandatory telemetry field. It is SEN-011, an additional sensor scored on
// evidence of data "transmitted **or** logged", and the three GP- fields are 56 of the
// packet's bytes -- the difference between a 199-byte and a 255-byte worst case, and so
// between 1.43 Hz and 1.18 Hz on a line the rulebook scores.
void test_a_fix_reaches_the_log_even_when_it_is_not_transmitted() {
    flight::Configuration c;
    c.team_id = "CAN-Team-25";
    CHECK(!c.transmit_gps);                     // the default
    flight::TelemetryBuilder builder(c);

    flight::SensorSnapshot s;
    s.imu_valid = true;
    s.baro_valid = true;
    s.orientation_valid = true;
    cansat::GpsData fix;
    fix.valid = true;
    fix.latitude = 21.220094;
    fix.longitude = 72.884836;
    fix.altitude = 15.0;
    fix.satellites = 9;
    fix.hdop = 1.4;
    s.gps = fix;

    const auto built = builder.build(1, 1000, s);
    CHECK(built.has_value());

    // Not on the air.
    CHECK(built->packet.find("GP-Lat-") == std::string::npos);
    CHECK(built->packet.find("GP-Lon-") == std::string::npos);
    CHECK(built->packet.find("GP-Alt-") == std::string::npos);

    // But every bit of it is in the row, which is what SEN-011 is scored on.
    const std::string line = builder.sd_line(*built, flight::MissionState::flight, 0);
    CHECK(line.find("21.220094") != std::string::npos);
    CHECK(line.find("72.884836") != std::string::npos);
    CHECK(line.find(",9,1.4,") != std::string::npos);
}

void test_turning_gps_transmission_on_without_the_budget_is_refused() {
    // The two settings must move together. Apart, the vehicle builds packets 56 bytes
    // longer than the airtime budget assumes: the duty check passes on a packet the radio
    // never sends, and the controller silently drops MODE/FAULTS/CAL/ARM to fit a cap set
    // for a configuration this no longer is.
    std::string why;
    flight::Configuration c;
    c.team_id = "CAN-Team-25";
    c.transmit_gps = true;                      // budget left at the 199-byte default
    CHECK(!flight::validate_config(c, why));
    CHECK(why.find("transmit_gps") != std::string::npos);
    CHECK(why.find("worst_case_packet_bytes") != std::string::npos);

    // Raising the budget alone is not enough either: 255 bytes will not fit a 700 ms slot.
    c.worst_case_packet_bytes = cansat::link::kWorstCasePacketBytesWithGps;
    CHECK(!flight::validate_config(c, why));
    CHECK(why.find("airtime") != std::string::npos);

    // Both, and it builds.
    c.telemetry_period_ms = 850;
    CHECK(flight::validate_config(c, why));
}

// The rulebook wants continuous telemetry, and "continuous" has to mean the same cadence
// everywhere. The vehicle detects its state and says so in the MODE tag, changes its LED
// blink, and changes nothing at all about how often it transmits. This is true today only
// because telemetry_task_ is configured once from one period with no state term -- an
// accident away from becoming false, which is what this test is for.
void test_the_packet_cadence_is_the_same_in_every_state() {
    flight::Configuration c;
    c.team_id = "CAN-Team-25";
    c.arming_delay_ms = 0;
    c.require_calibration_to_arm = false;
    flight::test::MockImu imu;
    flight::test::MockBarometer baro;
    flight::test::MockGps gps;
    flight::test::MockRadio radio;
    flight::test::MockLogger logger;
    flight::test::MockBoard board;

    // Real telemetry period, but the state thresholds sped up so a mission fits a test.
    c.launch_confirm_ms = 100;
    c.min_flight_ms = 200;
    c.landing_confirm_ms = 200;

    flight::Controller ctrl(c, imu, baro, gps, radio, logger, board);
    CHECK(ctrl.initialize());

    // Fly a profile. Launch is detected from acceleration, not altitude, so the boost has
    // to be in the IMU: pad, boost, coast down, at rest.
    std::vector<std::size_t> counts;
    std::vector<flight::MissionState> seen;
    std::size_t last_count = 0;
    for (std::uint64_t ms = 0; ms <= 60000; ms += 10) {
        const double t_s = ms / 1000.0;
        double altitude = 0.0;
        double az = flight::sensors::kStandardGravity;
        if (t_s > 10.0 && t_s <= 12.0) {          // boost, above launch_accel_mps2
            az = 60.0;
            altitude = (t_s - 10.0) * 20.0;
        } else if (t_s > 12.0 && t_s <= 32.0) {   // coast and descend
            az = 5.0;
            altitude = 40.0 + (t_s - 12.0) * 13.0;
            if (t_s > 22.0) altitude = 170.0 - (t_s - 22.0) * 17.0;
        }
        baro.sample.altitude_m = altitude;
        // The pressure has to move with the altitude, and this profile used not to move
        // it at all. The controller updates its vertical-rate estimate only when the
        // pressure changes -- a repeated reading means no fresh conversion, not a
        // stationary vehicle -- so a profile that flew 170 m up and back down on a
        // constant pressure produced a vertical rate of exactly zero throughout. The test
        // still reached LANDED, because at-rest was satisfied the moment the acceleration
        // came back to 1 g, so nothing ever noticed. The descent gate noticed.
        baro.sample.pressure_pa = 101325.0 * std::exp(-altitude / 8434.0);
        imu.sample.az_mps2 = az;
        ctrl.poll(ms);
        if (radio.packets.size() != last_count) {
            last_count = radio.packets.size();
            counts.push_back(static_cast<std::size_t>(ms));
            seen.push_back(ctrl.state());
        }
    }

    // The profile has to have actually moved through states, or this proves nothing.
    bool saw_flight = false, saw_landed_or_recovery = false;
    for (const flight::MissionState s : seen) {
        if (s == flight::MissionState::flight) saw_flight = true;
        if (s == flight::MissionState::landed || s == flight::MissionState::recovery) {
            saw_landed_or_recovery = true;
        }
    }
    CHECK(saw_flight);
    CHECK(saw_landed_or_recovery);
    CHECK(counts.size() > 10);

    // Every gap between consecutive packets is the configured period, whatever state the
    // vehicle was in when it sent them.
    for (std::size_t i = 1; i < counts.size(); ++i) {
        const std::size_t gap = counts[i] - counts[i - 1];
        CHECK(gap == c.telemetry_period_ms);
        if (gap != c.telemetry_period_ms) {
            std::cerr << "  packet " << i << " came " << gap
                      << " ms after the last, in state "
                      << flight::to_string(seen[i]) << "\n";
        }
    }
}

// The uplink gate. Five of these six are about the command being refused, because the cost
// of a false accept is a flight log that no longer exists.
namespace {
struct GroundLink {
    flight::Configuration c;
    flight::test::MockImu imu;
    flight::test::MockBarometer baro;
    flight::test::MockGps gps;
    flight::test::MockRadio radio;
    flight::test::MockLogger logger;
    flight::test::MockBoard board;

    // Which command sits on the air. Defaulted so every erase test reads as it did before
    // there was more than one command to send.
    cansat::CommandKind kind = cansat::CommandKind::erase_log;

    explicit GroundLink(bool allow, cansat::CommandKind k = cansat::CommandKind::erase_log) {
        c.team_id = "CAN-Team-25";
        c.allow_ground_commands = allow;
        kind = k;
    }
    // Runs to `until_ms`, with the command waiting on the air the whole time.
    void run(std::uint64_t until_ms) {
        ctrl_.reset(new flight::Controller(c, imu, baro, gps, radio, logger, board));
        CHECK(ctrl_->initialize());
        step(0, until_ms);
    }
    // Carries on with the SAME controller, which is the only way to observe a latch: a
    // fresh controller would come up unlatched and every such test would pass vacuously.
    void run_more(std::uint64_t extra_ms) {
        CHECK(ctrl_ != nullptr);
        step(now_ + 10, now_ + extra_ms);
    }
    void step(std::uint64_t from_ms, std::uint64_t until_ms) {
        for (std::uint64_t t = from_ms; t <= until_ms; t += 10) {
            if (radio.inbox.empty()) {
                // Minted against the number the vehicle has reached, the way the console
                // mints it from the last packet it received. A token made for any other
                // number is refused, so the harness has to play by the same rule.
                radio.inbox.push_back(cansat::format_command(
                    "CAN-Team-25", kind, c.command_password,
                    static_cast<std::uint32_t>(radio.packets.size())));
            }
            ctrl_->poll(t);
            now_ = t;
        }
        accepted = ctrl_->health().ground_commands_accepted;
        state = ctrl_->state();
        period_after = ctrl_->telemetry_period_ms();
        rate_maxed = ctrl_->health().rate_maxed;
    }
    std::uint32_t accepted = 0;
    flight::MissionState state = flight::MissionState::init;
    std::uint32_t period_after = 0;
    bool rate_maxed = false;
    std::uint64_t now_ = 0;
    std::unique_ptr<flight::Controller> ctrl_;
};
}  // namespace

void test_the_builder_can_be_told_to_carry_position_after_construction() {
    // The builder holds a COPY of the configuration, so this setter is the entire mechanism
    // by which a command can put the position on the air. Without it the controller would
    // change its own copy, every test that reads the controller would agree, and the packet
    // on the bench would still have no GP- fields in it.
    flight::Configuration c;
    c.team_id = "CAN-Team-07";
    CHECK(!c.transmit_gps);
    flight::TelemetryBuilder b(c);

    flight::SensorSnapshot s;
    s.imu_valid = s.orientation_valid = s.baro_valid = true;
    s.altitude_m = 12.3; s.pressure_pa = 98765.4; s.temperature_c = 21.7;
    s.roll_deg = -4.2; s.pitch_deg = 5.5; s.yaw_deg = 61.0;
    s.ax_mps2 = 0.02; s.ay_mps2 = -0.1; s.az_mps2 = 9.79;
    s.gps.valid = true;
    s.gps.latitude = 21.1667; s.gps.longitude = 72.7833; s.gps.altitude = 12.0;

    auto before = b.build(1, 1000, s);
    CHECK(before.has_value());
    CHECK(before->packet.find("GP-Lat-") == std::string::npos);
    // The record carries the fix either way -- it is what the SD row renders from.
    CHECK(before->record.gps.has_value());

    b.set_transmit_gps(true);
    auto after = b.build(2, 2000, s);
    CHECK(after.has_value());
    CHECK(after->packet.find("GP-Lat-") != std::string::npos);
    CHECK(after->packet.find("GP-Lon-") != std::string::npos);
    CHECK(after->packet.find("GP-Alt-") != std::string::npos);
}

void test_the_gps_command_speeds_up_and_puts_position_on_the_air() {
    GroundLink link(true, cansat::CommandKind::max_rate);
    link.run(6000);
    CHECK(link.state == flight::MissionState::ready);
    CHECK(link.accepted == 1);
    CHECK(link.rate_maxed);
    CHECK(link.period_after == cansat::link::kMaxRatePeriodGpsMs);
    CHECK(!link.radio.packets.empty());
    const std::string& last = link.radio.packets.back();
    CHECK(last.find("GP-Lat-") != std::string::npos);
    // The five tags are what paid for the position and the rate.
    CHECK(last.find("MODE-") == std::string::npos);
    CHECK(last.find("FAULTS-") == std::string::npos);
    CHECK(last.find("ARM-") == std::string::npos);
    CHECK(last.find("CAL-") == std::string::npos);
    CHECK(last.find("YR-") == std::string::npos);
}

void test_either_command_closes_the_uplink_behind_it() {
    // The harness keeps a command on the air the whole time, so an uplink still listening
    // would accept another the moment the replay window allowed it. Exactly one is ever
    // accepted and the radio is never polled again, which is the promise the button makes
    // when it says the vehicle stops accepting commands.
    GroundLink link(true, cansat::CommandKind::max_rate);
    link.run(8000);
    CHECK(link.accepted == 1);
    const int polls_at_latch = link.radio.receive_polls;
    const std::uint32_t period_at_latch = link.period_after;

    link.run_more(8000);
    CHECK(link.accepted == 1);
    CHECK(link.radio.receive_polls == polls_at_latch);
    CHECK(link.period_after == period_at_latch);
}

void test_the_other_max_rate_command_is_unreachable_after_the_first() {
    // Whichever lands first wins. The second is not refused -- it is not heard, which is a
    // different thing and the one an operator has to understand: there is no switching
    // between the two modes.
    GroundLink link(true, cansat::CommandKind::max_rate);
    link.run(6000);
    CHECK(link.period_after == cansat::link::kMaxRatePeriodGpsMs);

    // The other command there is: an erase. It is not refused, it is not heard.
    link.kind = cansat::CommandKind::erase_log;
    link.radio.inbox.clear();
    link.run_more(8000);
    CHECK(link.period_after == cansat::link::kMaxRatePeriodGpsMs);
    CHECK(link.accepted == 1);
    CHECK(link.logger.erases == 0);
}

void test_an_armed_vehicle_refuses_to_change_rate() {
    // The same window as the erase, for a stronger reason: this one cannot be undone, and a
    // vehicle that is armed is a vehicle nobody is holding.
    GroundLink link(true, cansat::CommandKind::max_rate);
    link.c.arming_delay_ms = 0;
    link.c.require_calibration_to_arm = false;
    link.run(4000);
    CHECK(link.state == flight::MissionState::ready);
    CHECK(link.accepted == 0);
    CHECK(!link.rate_maxed);
    CHECK(link.period_after == link.c.telemetry_period_ms);
}

void test_a_max_rate_command_obeys_the_replay_rules() {
    // Minted for a packet number the vehicle has not reached. The erase has the same guard;
    // this states it for a command whose acceptance cannot be walked back.
    flight::Configuration c;
    c.team_id = "CAN-Team-25";
    c.allow_ground_commands = true;
    c.arming_delay_ms = 600000;
    flight::test::MockImu imu; flight::test::MockBarometer baro; flight::test::MockGps gps;
    flight::test::MockRadio radio; flight::test::MockLogger logger; flight::test::MockBoard board;
    flight::Controller ctrl(c, imu, baro, gps, radio, logger, board);
    CHECK(ctrl.initialize());

    radio.inbox.push_back(cansat::format_command(
        "CAN-Team-25", cansat::CommandKind::max_rate, c.command_password, 100000));
    for (std::uint64_t ms = 0; ms <= 5000; ms += 10) ctrl.poll(ms);
    CHECK(!ctrl.health().rate_maxed);
    CHECK(ctrl.telemetry_period_ms() == c.telemetry_period_ms);
    CHECK(ctrl.health().ground_commands_ignored > 0);
}

void test_a_flight_build_cannot_be_commanded_to_max_rate() {
    // The property the README rests on, restated for the commands that matter most: with
    // allow_ground_commands false the radio is never polled, so neither rate command exists
    // as far as the vehicle is concerned.
    GroundLink link(false, cansat::CommandKind::max_rate);
    link.run(6000);
    CHECK(link.accepted == 0);
    CHECK(!link.rate_maxed);
    CHECK(link.radio.receive_polls == 0);
    CHECK(link.period_after == link.c.telemetry_period_ms);
}

void test_a_flight_build_has_no_uplink_at_all() {
    // allow_ground_commands defaults to false, and this is the property the README's "there
    // is no command uplink" rests on. The radio is never even polled.
    flight::Configuration fresh;
    CHECK(!fresh.allow_ground_commands);

    GroundLink link(false);
    link.run(4000);
    CHECK(link.accepted == 0);
    CHECK(link.logger.erases == 0);
    CHECK(link.radio.receive_polls == 0);
}

void test_the_bench_build_erases_the_log_on_command() {
    GroundLink link(true);
    link.run(4000);
    CHECK(link.state == flight::MissionState::ready);
    CHECK(link.accepted >= 1);
    CHECK(link.logger.erases >= 1);
}

void test_a_replayed_command_erases_nothing() {
    // The threat this exists for: someone records a command that worked and sends it again.
    // The token is only valid for the packet number it was minted against, and the vehicle
    // refuses a number it has already accepted -- so the recording is worth exactly one
    // erase, the one the operator meant.
    flight::Configuration c;
    c.team_id = "CAN-Team-25";
    c.allow_ground_commands = true;
    // The window is READY with ARM-0, and these runs are long enough that the vehicle
    // would otherwise arm partway through and refuse for the right reason at the wrong
    // moment. Held disarmed so each test measures only what it names.
    c.arming_delay_ms = 600000;
    flight::test::MockImu imu; flight::test::MockBarometer baro; flight::test::MockGps gps;
    flight::test::MockRadio radio; flight::test::MockLogger logger; flight::test::MockBoard board;
    flight::Controller ctrl(c, imu, baro, gps, radio, logger, board);
    CHECK(ctrl.initialize());

    // Let a few packets go out, then command against the current number.
    for (std::uint64_t ms = 0; ms <= 3000; ms += 10) ctrl.poll(ms);
    const std::uint32_t pn = static_cast<std::uint32_t>(radio.packets.size());
    const std::string captured =
        cansat::format_command("CAN-Team-25", cansat::CommandKind::erase_log,
                               c.command_password, pn);

    radio.inbox.push_back(captured);
    for (std::uint64_t ms = 3010; ms <= 5000; ms += 10) ctrl.poll(ms);
    CHECK(logger.erases == 1);

    // The identical frame again, later. Same bytes, same token, and nothing happens.
    const int erases_after_first = logger.erases;
    radio.inbox.push_back(captured);
    for (std::uint64_t ms = 5010; ms <= 8000; ms += 10) ctrl.poll(ms);
    CHECK(logger.erases == erases_after_first);
    CHECK(ctrl.health().ground_commands_ignored > 0);
}

void test_a_command_minted_for_a_future_packet_is_refused() {
    // Someone who knows the password could pre-compute tokens. Refusing numbers the vehicle
    // has not reached means a stockpile of them is useless until the moment each is current,
    // and by then the operator is standing there anyway.
    flight::Configuration c;
    c.team_id = "CAN-Team-25";
    c.allow_ground_commands = true;
    // The window is READY with ARM-0, and these runs are long enough that the vehicle
    // would otherwise arm partway through and refuse for the right reason at the wrong
    // moment. Held disarmed so each test measures only what it names.
    c.arming_delay_ms = 600000;
    flight::test::MockImu imu; flight::test::MockBarometer baro; flight::test::MockGps gps;
    flight::test::MockRadio radio; flight::test::MockLogger logger; flight::test::MockBoard board;
    flight::Controller ctrl(c, imu, baro, gps, radio, logger, board);
    CHECK(ctrl.initialize());

    radio.inbox.push_back(cansat::format_command(
        "CAN-Team-25", cansat::CommandKind::erase_log, c.command_password, 100000));
    for (std::uint64_t ms = 0; ms <= 5000; ms += 10) ctrl.poll(ms);
    CHECK(logger.erases == 0);
}

void test_a_stale_command_is_refused() {
    // A frame from an earlier session, replayed after the vehicle has moved on. Valid token,
    // never used before, and still refused because it is older than the window.
    flight::Configuration c;
    c.team_id = "CAN-Team-25";
    c.allow_ground_commands = true;
    // The window is READY with ARM-0, and these runs are long enough that the vehicle
    // would otherwise arm partway through and refuse for the right reason at the wrong
    // moment. Held disarmed so each test measures only what it names.
    c.arming_delay_ms = 600000;
    c.command_replay_window = 2;
    flight::test::MockImu imu; flight::test::MockBarometer baro; flight::test::MockGps gps;
    flight::test::MockRadio radio; flight::test::MockLogger logger; flight::test::MockBoard board;
    flight::Controller ctrl(c, imu, baro, gps, radio, logger, board);
    CHECK(ctrl.initialize());

    for (std::uint64_t ms = 0; ms <= 8000; ms += 10) ctrl.poll(ms);
    CHECK(radio.packets.size() > 4);
    radio.inbox.push_back(cansat::format_command(
        "CAN-Team-25", cansat::CommandKind::erase_log, c.command_password, 1));
    for (std::uint64_t ms = 8010; ms <= 11000; ms += 10) ctrl.poll(ms);
    CHECK(logger.erases == 0);
}

void test_an_armed_vehicle_refuses_to_erase() {
    // ARM-1 means the vehicle is ready to fly. Everything from here to recovery holds a log
    // that cannot be recreated, so the window shuts at arming rather than at launch.
    GroundLink link(true);
    link.c.arming_delay_ms = 0;               // armed from the first poll
    link.c.require_calibration_to_arm = false;
    link.run(4000);
    // It must fail because the vehicle was armed, not because it never reached READY --
    // a gate test that passes for the wrong reason is worse than no gate test.
    CHECK(link.state == flight::MissionState::ready);
    CHECK(link.accepted == 0);
    CHECK(link.logger.erases == 0);
}

void test_another_teams_command_erases_nothing() {
    flight::Configuration c;
    c.team_id = "CAN-Team-25";
    c.allow_ground_commands = true;
    flight::test::MockImu imu; flight::test::MockBarometer baro; flight::test::MockGps gps;
    flight::test::MockRadio radio; flight::test::MockLogger logger; flight::test::MockBoard board;
    flight::Controller ctrl(c, imu, baro, gps, radio, logger, board);
    CHECK(ctrl.initialize());
    for (std::uint64_t t = 0; t <= 4000; t += 10) {
        if (radio.inbox.empty()) {
            radio.inbox.push_back(cansat::format_command(
                "CAN-Team-07", cansat::CommandKind::erase_log, c.command_password,
                static_cast<std::uint32_t>(radio.packets.size())));
        }
        ctrl.poll(t);
    }
    CHECK(logger.erases == 0);
    CHECK(ctrl.health().ground_commands_accepted == 0);
    CHECK(ctrl.health().ground_commands_ignored > 0);   // seen, and refused
}

void test_a_refused_erase_is_reported_rather_than_swallowed() {
    GroundLink link(true);
    link.logger.fail_erase = true;
    link.run(4000);
    CHECK(link.logger.erases == 0);
    CHECK(link.accepted >= 1);   // the command was acted on; the erase itself failed
}

void test_listening_never_costs_a_packet() {
    // The uplink must be invisible to the mandatory 1 Hz downlink. Same run, same window,
    // with and without the feature: the packet count may not move.
    GroundLink off(false);
    off.run(6000);
    GroundLink on(true);
    on.run(6000);
    CHECK(off.radio.packets.size() == on.radio.packets.size());
    CHECK(off.radio.packets.size() > 1);   // and both actually transmitted
}

// The button is meant to leave the card in the state tools/prepare_sd_card.py leaves it:
// empty log, fresh header, and the old bytes actually gone. The first two are instant; the
// third is 64 MB of card writes and cannot be.
void test_the_erase_reads_empty_immediately_and_scrubs_afterwards() {
    GroundLink link(true);
    link.logger.scrub_blocks = 40;         // a card with something on it
    link.run(4000);
    CHECK(link.logger.erases >= 1);        // the log read empty on the first command
    CHECK(link.logger.erase_steps > 0);    // and the scrub is being driven
}

void test_the_scrub_finishes_without_blocking_telemetry() {
    // The whole reason the scrub is incremental. A vehicle scrubbing 64 MB must keep
    // sending its 1 Hz packets throughout, so the packet count may not differ from a run
    // that never scrubbed at all.
    GroundLink quiet(true);
    quiet.run(8000);
    GroundLink scrubbing(true);
    scrubbing.logger.scrub_blocks = 400;
    scrubbing.run(8000);
    CHECK(scrubbing.logger.scrub_remaining == 0);                       // it completed
    CHECK(scrubbing.radio.packets.size() == quiet.radio.packets.size());
    CHECK(scrubbing.radio.packets.size() > 1);
}

void test_a_vehicle_that_was_never_asked_never_scrubs() {
    // erase_step() is called every poll; on a vehicle that has erased nothing it must be
    // free and must not invent work.
    GroundLink link(false);
    link.run(4000);
    CHECK(link.logger.erases == 0);
    CHECK(link.logger.scrub_remaining == 0);
}

// And a microphone that fails must cost a warning and a column, never a packet. This is the
// test that fails if the sensor is ever promoted into the mandatory path.
void test_a_failed_microphone_costs_a_warning_and_nothing_else() {
    flight::Configuration c;
    c.team_id = "CAN-Team-25";
    flight::test::MockImu imu;
    flight::test::MockBarometer baro;
    flight::test::MockGps gps;
    flight::test::MockRadio radio;
    flight::test::MockLogger logger;
    flight::test::MockBoard board;
    flight::test::MockSound sound;
    sound.fail_init = true;
    sound.fail_read = true;

    flight::Controller ctrl(c, imu, baro, gps, radio, logger, board, &sound);
    CHECK(ctrl.initialize());          // a dead additional sensor never fails the self-test
    for (std::uint64_t t = 0; t <= 5000; t += 10) {
        ctrl.poll(t);
    }
    CHECK(ctrl.faults().active(flight::FaultCode::sound_unavailable));
    CHECK(ctrl.state() != flight::MissionState::fault);
    CHECK(ctrl.health().packets_sent > 0);   // telemetry unaffected
    CHECK(!ctrl.health().sound_ok);
}

// A working one reaches the log, and the log alone.
void test_a_working_microphone_reaches_the_log() {
    flight::Configuration c;
    c.team_id = "CAN-Team-25";
    flight::test::MockImu imu;
    flight::test::MockBarometer baro;
    flight::test::MockGps gps;
    flight::test::MockRadio radio;
    flight::test::MockLogger logger;
    flight::test::MockBoard board;
    flight::test::MockSound sound;
    sound.level_mv_pp = 250.0;

    flight::Controller ctrl(c, imu, baro, gps, radio, logger, board, &sound);
    CHECK(ctrl.initialize());
    for (std::uint64_t t = 0; t <= 2000; t += 10) {
        ctrl.poll(t);
    }
    CHECK(sound.reads > 0);
    CHECK(ctrl.health().sound_ok);
    CHECK(!ctrl.faults().active(flight::FaultCode::sound_unavailable));
    // This assertion used to read `logger.packets` and require the level to be ABSENT --
    // the exact opposite of this test's name, and the reason F-15 survived: the guard was
    // pointing the wrong way and passing. The radio must not carry it; the log must.
    for (const std::string& packet : radio.packets) {
        CHECK(packet.find("250.0") == std::string::npos);
    }
    CHECK(!logger.lines.empty());
    bool level_logged = false;
    for (const std::string& line : logger.lines) {
        if (line.find("250.0") != std::string::npos) level_logged = true;
    }
    CHECK(level_logged);

    // And the header the log opens with must describe the rows that follow it.
    CHECK(logger.lines.front().find("team_id") == std::string::npos ||
          logger.lines.front() == flight::TelemetryBuilder::sd_header());
}


// A three-pin LM393 board has no analogue output at all. Refusing its threshold duty because
// the level is missing would throw away the only measurement that board can make -- and the
// two variants are sold under the same name, so this is a real build, not a hypothetical.
void test_a_gate_only_module_is_still_a_working_sensor() {
    flight::Configuration c;
    c.team_id = "CAN-Team-25";
    flight::test::MockImu imu;
    flight::test::MockBarometer baro;
    flight::test::MockGps gps;
    flight::test::MockRadio radio;
    flight::test::MockLogger logger;
    flight::test::MockBoard board;
    flight::test::MockSound sound;
    sound.analog_connected = false;      // no AO pin on this board
    sound.gate_connected = true;
    sound.gate_duty_pct = 42.0;

    flight::Controller ctrl(c, imu, baro, gps, radio, logger, board, &sound);
    CHECK(ctrl.initialize());
    for (std::uint64_t t = 0; t <= 3000; t += 10) {
        ctrl.poll(t);
    }
    CHECK(ctrl.health().sound_ok);
    CHECK(!ctrl.faults().active(flight::FaultCode::sound_unavailable));
}

void test_gate_duty_is_a_percentage_and_stays_one() {
    CHECK(flight::sound_gate_duty_pct(0, 256) == 0.0);
    CHECK(flight::sound_gate_duty_pct(256, 256) == 100.0);
    const double half = flight::sound_gate_duty_pct(128, 256);
    CHECK(half > 49.9 && half < 50.1);

    // An empty window is 0, not a division by zero.
    CHECK(flight::sound_gate_duty_pct(0, 0) == 0.0);
    CHECK(flight::sound_gate_duty_pct(5, 0) == 0.0);
    // A counter that has overrun its window is clamped rather than reported above 100.
    CHECK(flight::sound_gate_duty_pct(500, 256) == 100.0);
}

// The gate is a separate column and a separate validity. A board with only AO wired must
// leave it blank rather than write a zero that reads as "never above threshold".
void test_an_unwired_gate_leaves_its_column_blank() {
    flight::Configuration c;
    c.team_id = "CAN-Team-25";
    flight::TelemetryBuilder builder(c);

    flight::SensorSnapshot s;
    s.imu_valid = true;
    s.baro_valid = true;
    s.orientation_valid = true;
    s.sound_mv_pp = 300.0;
    s.sound_valid = true;
    s.sound_gate_valid = false;          // DO not connected

    const auto built = builder.build(1, 1000, s);
    CHECK(built.has_value());
    const std::string line = builder.sd_line(*built, flight::MissionState::flight, 0);
    CHECK(line.find(",300.0,0,,") != std::string::npos);

    const std::string header = flight::TelemetryBuilder::sd_header();
    CHECK(header.find("sound_mv_pp,sound_clipped,sound_gate_pct,packet") !=
          std::string::npos);
    std::size_t header_commas = 0;
    for (char ch : header) {
        if (ch == 0x2C) ++header_commas;
    }
    std::size_t line_commas = 0;
    for (char ch : line) {
        if (ch == 0x2C) ++line_commas;
    }
    CHECK(header_commas == line_commas);
}

int main(int argc, char** argv) {
    const std::string repo_root = argc > 1 ? argv[1] : ".";
    test_mandatory_validity_covers_every_flag();
    test_telemetry_format_exact();
    test_packet_numbering_and_padding();
    test_parser_rejects_precision_and_order();
    test_shared_protocol_fixtures(repo_root);
    test_imu_scaling();
    test_magnetometer_conversions();
    test_magnetometer_axes_are_rotated_into_the_body_frame();
    test_imu_range_bits_match_their_sensitivities();
    test_bmp280_compensation_datasheet_vector();
    test_pressure_altitude();
    test_orientation_levels_and_yaw();
    test_magnetic_yaw_is_tilt_compensated();
    test_orientation_yaw_is_disciplined_by_the_magnetometer();
    test_uncalibrated_magnetometer_does_not_claim_absolute_heading();
    test_a_field_with_no_heading_in_it_is_not_seeded_as_one();
    test_orientation_rejects_an_implausible_field();
    test_orientation_ignores_the_accelerometer_under_high_g();
    test_orientation_rejects_unusable_input();
    test_gps_parser();
    test_scheduler();
    test_fault_manager();
    test_state_machine_full_mission();
    test_a_hovering_drone_is_not_a_landing();
    test_a_lift_slower_than_the_rest_threshold_is_not_a_landing();
    test_the_descent_gate_does_not_survive_a_state_change();
    test_one_descending_sample_does_not_open_the_descent_gate();
    test_the_descent_and_rest_thresholds_may_not_overlap();
    test_the_link_profile_is_compulsorily_faster_than_1_hz();
    test_a_default_vehicle_transmits_faster_than_1_hz();
    test_state_machine_fault_paths();
    test_config_validation();
    test_sound_level_reduces_a_window_to_its_envelope();
    test_sound_level_refuses_a_window_it_cannot_scale();
    test_a_clipped_window_is_reported_as_clipped();
    test_the_sound_level_is_logged_and_never_transmitted();
    test_the_log_records_the_numbers_the_gps_gate_judges_on();
    test_no_fix_leaves_the_gps_quality_columns_blank();
    test_an_absent_microphone_leaves_the_columns_blank_rather_than_zero();
    test_a_vehicle_without_a_microphone_behaves_as_before();
    test_a_failed_microphone_costs_a_warning_and_nothing_else();
    test_a_working_microphone_reaches_the_log();
    test_a_gate_only_module_is_still_a_working_sensor();
    test_gate_duty_is_a_percentage_and_stays_one();
    test_an_unwired_gate_leaves_its_column_blank();
    test_a_refused_configuration_says_which_setting_was_wrong();
    test_config_radio_airtime_guard();
    test_formatter_and_parser_agree_at_the_edges();
    test_a_value_too_wide_to_format_invalidates_the_packet();
    test_controller_drops_optional_fields_before_overrunning_the_budget();
    test_gps_coordinate_validation();
    test_the_erase_reads_empty_immediately_and_scrubs_afterwards();
    test_the_scrub_finishes_without_blocking_telemetry();
    test_a_vehicle_that_was_never_asked_never_scrubs();
    test_a_fix_reaches_the_log_even_when_it_is_not_transmitted();
    test_turning_gps_transmission_on_without_the_budget_is_refused();
    test_the_packet_cadence_is_the_same_in_every_state();
    test_the_builder_can_be_told_to_carry_position_after_construction();
    test_the_gps_command_speeds_up_and_puts_position_on_the_air();
    test_either_command_closes_the_uplink_behind_it();
    test_the_other_max_rate_command_is_unreachable_after_the_first();
    test_an_armed_vehicle_refuses_to_change_rate();
    test_a_max_rate_command_obeys_the_replay_rules();
    test_a_flight_build_cannot_be_commanded_to_max_rate();
    test_a_flight_build_has_no_uplink_at_all();
    test_the_bench_build_erases_the_log_on_command();
    test_a_replayed_command_erases_nothing();
    test_a_command_minted_for_a_future_packet_is_refused();
    test_a_stale_command_is_refused();
    test_an_armed_vehicle_refuses_to_erase();
    test_another_teams_command_erases_nothing();
    test_a_refused_erase_is_reported_rather_than_swallowed();
    test_listening_never_costs_a_packet();
    test_a_command_round_trips_for_its_own_team();
    test_the_password_never_appears_on_the_wire();
    test_a_command_for_another_team_is_ignored();
    test_a_command_with_the_wrong_password_is_ignored();
    test_a_token_is_valid_for_exactly_one_packet_number();
    test_a_telemetry_packet_is_never_a_command();
    test_an_unconfigured_vehicle_matches_nothing();
    test_a_truncated_command_is_ignored();
    test_the_commanded_rate_periods_clear_their_own_airtime();
    test_a_token_authorises_one_command_and_not_another();
    test_command_tokens_match_the_shared_fixture(repo_root);
    test_a_fix_with_too_few_satellites_is_refused();
    test_a_fix_with_poor_geometry_is_refused();
    test_a_good_fix_still_passes_and_carries_its_quality();
    test_a_refused_fix_does_not_disturb_the_last_good_one();
    test_a_hemisphere_from_the_wrong_axis_is_rejected();
    test_orientation_survives_the_wrap_and_the_poles();
    test_calibration_rejects_a_steady_rotation_as_bias();
    test_fault_severity_never_falls_while_active();
    test_landing_is_not_declared_during_a_steady_descent();
    test_battery_voltage_reports_whether_it_is_scaled();
    test_loop_tick_is_bounded_by_the_gps_uart_fifo();
    test_a_frozen_gps_fix_is_not_reported_as_a_live_position();
    test_config_rejects_a_gps_timeout_faster_than_the_receiver();
    test_sensor_timing_model();
    test_config_sensor_rate_guard();
    test_controller_ignores_repeated_barometer_samples();
    test_link_profile_is_shared_by_both_ends();
    test_lora_airtime_reference_vectors();
    test_telemetry_builder();
    test_sd_log_row_matches_its_header();
    test_the_widest_sd_row_still_fits_one_block();
    test_a_reset_log_reads_empty_and_the_old_bytes_are_scrubbed_away();
    test_a_scrub_never_overwrites_a_record_written_during_it();
    test_raw_block_log();
    test_raw_block_log_survives_a_torn_header_write();
    test_controller_sequence_and_degradation();
    test_controller_sensor_failure_suppresses_but_continues();
    test_startup_calibrator_stationary_and_moving();
    test_controller_launch_detection();
    test_controller_arming_lockout_blocks_early_boost();
    test_controller_sensor_plausibility();
    test_telemetry_declares_the_yaw_reference();
    test_a_missing_magnetometer_degrades_rather_than_stops();
    test_a_magnetometer_that_stops_is_reported_and_survived();
    test_gps_course_is_a_cross_check_not_a_yaw_source();
    test_mag_calibration_requires_real_coverage();
    test_accel_calibration_is_rotation_invariant();
    test_measured_packet_sizes_match_the_link_budget();

    std::cout << (g_checks - g_failures) << "/" << g_checks << " checks passed\n";
    if (g_failures != 0) {
        std::cerr << g_failures << " FAILURE(S)\n";
        return 1;
    }
    std::cout << "flight_tests passed\n";
    return 0;
}
