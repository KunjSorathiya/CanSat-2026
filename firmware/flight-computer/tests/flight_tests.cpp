// Comprehensive host test suite for the CanSat flight core. No hardware required.
#include "cansat/link_profile.hpp"
#include "cansat/lora_airtime.hpp"
#include "cansat/sx1278.hpp"
#include "cansat/telemetry.hpp"
#include "flight/config.hpp"
#include "flight/controller.hpp"
#include "flight/fault_manager.hpp"
#include "flight/gps_parser.hpp"
#include "flight/orientation.hpp"
#include "flight/raw_block_log.hpp"
#include "flight/scheduler.hpp"
#include "flight/sensor_math.hpp"
#include "flight/sensor_timing.hpp"
#include "flight/startup_calibration.hpp"
#include "flight/state_machine.hpp"
#include "flight/telemetry_builder.hpp"
#include "mock_hardware.hpp"

#include <array>
#include <cmath>
#include <cstring>
#include <fstream>
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
void test_mpu_scaling() {
    using namespace flight::sensors;
    const ImuScales s = imu_scales(AccelRange::g16, GyroRange::dps2000);
    CHECK(approx(accel_raw_to_mps2(2048, s), kStandardGravity, 1e-6));
    CHECK(approx(accel_raw_to_mps2(-2048, s), -kStandardGravity, 1e-6));
    CHECK(approx(gyro_raw_to_dps(164, s), 10.0, 0.05));

    const ImuScales s2 = imu_scales(AccelRange::g2, GyroRange::dps250);
    CHECK(approx(accel_raw_to_mps2(16384, s2), kStandardGravity, 1e-6));
    CHECK(approx(gyro_raw_to_dps(131, s2), 1.0, 0.01));
    CHECK(approx(mpu_temperature_c(0), 36.53, 1e-6));
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
void test_orientation_levels_and_yaw() {
    flight::OrientationEstimator est(0.98);
    // First update establishes the accelerometer reference directly.
    est.update(0.0, 0.0, 9.80665, 0.0, 0.0, 0.0, 0.1);
    auto e = est.estimate();
    CHECK(e.valid);
    CHECK(approx(e.roll_deg, 0.0, 0.5));
    CHECK(approx(e.pitch_deg, 0.0, 0.5));

    // Rolled onto its side: gravity now on +Y.
    flight::OrientationEstimator rolled(0.98);
    rolled.update(0.0, 9.80665, 0.0, 0.0, 0.0, 0.0, 0.1);
    CHECK(approx(rolled.estimate().roll_deg, 90.0, 1.0));

    // Pure gyro yaw integration: 10 deg/s for 3 s -> ~30 deg (relative).
    flight::OrientationEstimator yaw(0.98);
    yaw.update(0.0, 0.0, 9.80665, 0.0, 0.0, 10.0, 1.0);
    yaw.update(0.0, 0.0, 9.80665, 0.0, 0.0, 10.0, 1.0);
    yaw.update(0.0, 0.0, 9.80665, 0.0, 0.0, 10.0, 1.0);
    CHECK(approx(yaw.estimate().yaw_deg, 30.0, 0.5));

    CHECK(approx(flight::wrap_degrees(190.0), -170.0, 1e-9));
    CHECK(approx(flight::wrap_degrees(-190.0), 170.0, 1e-9));
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

    // Coast: not at rest yet.
    di.accel_magnitude_mps2 = 5.0;
    di.altitude_rate_mps = -20.0;
    sm.update(300, di);
    CHECK(sm.state() == flight::MissionState::flight);

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
void test_config_validation() {
    std::string why;
    flight::Configuration c;
    CHECK(!flight::validate_config(c, why));  // placeholder team id

    c.team_id = "CAN-Team-07";
    CHECK(flight::validate_config(c, why));
    CHECK(why.empty());

    c.telemetry_period_ms = 2000;  // slower than 1 Hz minimum
    CHECK(!flight::validate_config(c, why));
    c.telemetry_period_ms = 1000;

    c.post_impact_transmission_ms = 3000;  // below rulebook 5 s
    CHECK(!flight::validate_config(c, why));
    c.post_impact_transmission_ms = 5000;
    CHECK(flight::validate_config(c, why));

    c.orientation_alpha = 1.5;
    CHECK(!flight::validate_config(c, why));
    c.orientation_alpha = 0.98;

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

    // Default: SF7/125 kHz, the 255-byte FIFO limit as the budget, 1 Hz -> ~400 ms
    // airtime, ~40 % duty.
    CHECK(flight::validate_config(c, why));
    CHECK(c.worst_case_packet_bytes == cansat::kMaxLoraPayloadBytes);
    CHECK(approx(flight::worst_case_airtime_ms(c), 399.6, 0.5));
    CHECK(flight::channel_duty(c) < 0.45);

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
    for (const std::uint64_t t : {0ULL, 1ULL, 999ULL, 3600000ULL, last, last + 1,
                                  1234567890ULL}) {
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

// A checksum-valid sentence can still carry an impossible position. Those must not reach
// telemetry: the vehicle should transmit no fix rather than a wrong one.
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

// The complementary filter blends angles, which wrap. Blending them naively is wrong at
// the seam, and a tumbling CanSat crosses that seam on every rotation.
void test_orientation_blends_across_the_wrap() {
    flight::OrientationEstimator est(0.98);

    // Establish a reference near +180 deg of roll: gravity on -Y with the vehicle rolled
    // almost all the way over. atan2(ay, az) with ay slightly positive and az negative
    // gives a roll just under +180 deg.
    est.update(0.0, 0.2, -9.8, 0.0, 0.0, 0.0, 0.05);
    const double start = est.estimate().roll_deg;
    CHECK(std::fabs(start) > 170.0);

    // Now roll a little further, so the true attitude crosses the seam and the
    // accelerometer reports the other sign. A naive weighted mean would swing the
    // estimate most of the way around the circle; the wrapped blend must not.
    for (int i = 0; i < 20; ++i) {
        est.update(0.0, -0.2, -9.8, 0.0, 0.0, 0.0, 0.05);
        const double roll = est.estimate().roll_deg;
        CHECK(std::fabs(roll) > 170.0);  // stays near the seam, never swings to ~0
    }

    // The estimate must still be a valid wrapped angle.
    const double settled = est.estimate().roll_deg;
    CHECK(settled > -180.0 && settled <= 180.0);

    // Away from the seam the wrapped blend is the ordinary weighted mean: level vehicle,
    // no rotation, so the estimate must converge to level rather than drift.
    flight::OrientationEstimator level(0.98);
    for (int i = 0; i < 50; ++i) level.update(0.0, 0.0, 9.80665, 0.0, 0.0, 0.0, 0.033);
    CHECK(approx(level.estimate().roll_deg, 0.0, 0.5));
    CHECK(approx(level.estimate().pitch_deg, 0.0, 0.5));

    // A gyro-only spin through the seam wraps rather than accumulating past 180 deg.
    flight::OrientationEstimator spin(1.0);  // ignore the accelerometer entirely
    for (int i = 0; i < 100; ++i) spin.update(0.0, 0.0, 9.80665, 100.0, 0.0, 0.0, 0.05);
    const double spun = spin.estimate().roll_deg;
    CHECK(spun > -180.0 && spun <= 180.0);
    CHECK(std::isfinite(spun));
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
        CHECK(name != nullptr && name[0] != ' ');
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

    // MPU-6050 register map: DLPF 1..6 runs the gyro at 1 kHz, divided by (1 + div).
    CHECK(approx(flight::sensors::imu_sample_rate_hz(4, 4), 200.0, 1e-9));
    CHECK(approx(flight::sensors::imu_sample_rate_hz(0, 4), 1600.0, 1e-9));
    CHECK(approx(flight::sensors::imu_accel_bandwidth_hz(4), 21.0, 1e-9));
    CHECK(approx(flight::sensors::imu_gyro_bandwidth_hz(4), 20.0, 1e-9));
    CHECK(flight::sensors::imu_accel_bandwidth_hz(3) > flight::sensors::imu_accel_bandwidth_hz(4));
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

    // The IMU's anti-alias filter must stay below the acquisition Nyquist limit.
    flight::Configuration d;
    const double nyquist = 1000.0 / (2.0 * static_cast<double>(d.sensor_period_ms));
    CHECK(flight::sensors::imu_accel_bandwidth_hz(d.imu_dlpf_cfg) <= nyquist * 1.5);
    CHECK(flight::sensors::imu_sample_rate_hz(d.imu_dlpf_cfg, d.imu_sample_rate_div) >
          1000.0 / static_cast<double>(d.sensor_period_ms));
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
    CHECK(std::fabs(cal.result().accel_bias_mps2[2]) < 0.01);

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

}  // namespace

int main(int argc, char** argv) {
    const std::string repo_root = argc > 1 ? argv[1] : ".";
    test_telemetry_format_exact();
    test_packet_numbering_and_padding();
    test_parser_rejects_precision_and_order();
    test_shared_protocol_fixtures(repo_root);
    test_mpu_scaling();
    test_imu_range_bits_match_their_sensitivities();
    test_bmp280_compensation_datasheet_vector();
    test_pressure_altitude();
    test_orientation_levels_and_yaw();
    test_gps_parser();
    test_scheduler();
    test_fault_manager();
    test_state_machine_full_mission();
    test_state_machine_fault_paths();
    test_config_validation();
    test_config_radio_airtime_guard();
    test_formatter_and_parser_agree_at_the_edges();
    test_controller_drops_optional_fields_before_overrunning_the_budget();
    test_gps_coordinate_validation();
    test_orientation_blends_across_the_wrap();
    test_calibration_rejects_a_steady_rotation_as_bias();
    test_fault_severity_never_falls_while_active();
    test_landing_is_not_declared_during_a_steady_descent();
    test_battery_voltage_reports_whether_it_is_scaled();
    test_loop_tick_is_bounded_by_the_gps_uart_fifo();
    test_sensor_timing_model();
    test_config_sensor_rate_guard();
    test_controller_ignores_repeated_barometer_samples();
    test_link_profile_is_shared_by_both_ends();
    test_lora_airtime_reference_vectors();
    test_telemetry_builder();
    test_sd_log_row_matches_its_header();
    test_raw_block_log();
    test_raw_block_log_survives_a_torn_header_write();
    test_controller_sequence_and_degradation();
    test_controller_sensor_failure_suppresses_but_continues();
    test_startup_calibrator_stationary_and_moving();
    test_controller_launch_detection();
    test_controller_arming_lockout_blocks_early_boost();
    test_controller_sensor_plausibility();

    std::cout << (g_checks - g_failures) << "/" << g_checks << " checks passed\n";
    if (g_failures != 0) {
        std::cerr << g_failures << " FAILURE(S)\n";
        return 1;
    }
    std::cout << "flight_tests passed\n";
    return 0;
}
