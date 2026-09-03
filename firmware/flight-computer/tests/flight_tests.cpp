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
#include "flight/startup_calibration.hpp"
#include "flight/state_machine.hpp"
#include "flight/telemetry_builder.hpp"
#include "mock_hardware.hpp"

#include <array>
#include <cmath>
#include <cstring>
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

    // Default: SF7/125 kHz, 200-byte worst case, 1 Hz -> ~318 ms airtime, ~32 % duty.
    CHECK(flight::validate_config(c, why));
    CHECK(approx(flight::worst_case_airtime_ms(c), 317.7, 0.5));
    CHECK(flight::channel_duty(c) < 0.35);

    // The former default of SF9 puts one packet at ~1 s of airtime: not sustainable.
    c.radio.spreading_factor = 9;
    CHECK(!flight::validate_config(c, why));
    CHECK(why.find("airtime") != std::string::npos);
    CHECK(flight::worst_case_airtime_ms(c) > 900.0);

    // SF7 at 2 Hz over 125 kHz is also refused: 318 ms in a 500 ms slot is 64 % duty.
    c.radio.spreading_factor = 7;
    c.telemetry_period_ms = 500;
    CHECK(!flight::validate_config(c, why));

    // Doubling the bandwidth halves the airtime and makes 2 Hz legitimate.
    c.radio.bandwidth_hz = 250000;
    CHECK(flight::validate_config(c, why));
    CHECK(flight::channel_duty(c) < 0.35);

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
    bad.worst_case_packet_bytes = 200;
    bad.max_channel_duty = 0.0;
    CHECK(!flight::validate_config(bad, why));
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
    CHECK(log2.append_line("row-7", 5));
    CHECK(log2.record_count() == 7);
    CHECK(!log2.append_line("row-8", 5));  // region full
    CHECK(log2.full());

    // Stored record content is recoverable.
    std::uint8_t block[512];
    CHECK(MemBlocks::rd(&mem, 1, block));
    CHECK(std::memcmp(block, "row-1", 5) == 0);
    CHECK(block[5] == '\n');
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

}  // namespace

int main() {
    test_telemetry_format_exact();
    test_packet_numbering_and_padding();
    test_parser_rejects_precision_and_order();
    test_mpu_scaling();
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
    test_link_profile_is_shared_by_both_ends();
    test_lora_airtime_reference_vectors();
    test_telemetry_builder();
    test_raw_block_log();
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
