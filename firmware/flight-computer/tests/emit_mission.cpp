// Runs the real flight controller through a scripted mission and prints the packets it
// transmits, one per line, to stdout.
//
// This is not a test on its own: it is the vehicle half of the end-to-end integration
// test. ground-station/software/tests/test_end_to_end.py runs this binary and pushes its
// output through the actual ground-station pipeline — framing, CRC, parser, validator,
// logger, CSV export — so the two halves of the system are checked against each other
// rather than each against its own idea of the format.
//
// Usage: emit_mission [packet_count]

#include "cansat/link_profile.hpp"
#include "flight/controller.hpp"
#include "mock_hardware.hpp"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <string>

int main(int argc, char** argv) {
    const int wanted = argc > 1 ? std::atoi(argv[1]) : 30;

    flight::Configuration config;
    config.team_id = "CAN-Team-01";
    // GPS on the air, which is the default, and the diagnostic tags on the air as well,
    // which is not. This fixture is what proves the C++, Python and JavaScript parsers agree
    // on the optional fields, and the end-to-end test also checks that MODE crosses the
    // whole pipeline -- so the tags stay in, and the budget and period make room for them.
    // No sound sensor is wired into this run, so no SN- field appears.
    config.append_diagnostic_fields = true;
    // A bench budget: with the tags on, packets pass the organizers' 200-byte ceiling,
    // which validate_config() allows only for a tagged build like this one.
    config.worst_case_packet_bytes = cansat::link::kBenchPacketBytes;
    // The period a 255-byte packet actually fits inside: 700 ms would put it at 57 % duty,
    // which validate_config() correctly refuses.
    config.telemetry_period_ms = 850;
    config.altitude_relative_to_baseline = false;
    config.calib_samples = 4;
    config.arming_delay_ms = 200;
    config.launch_confirm_ms = 100;
    config.min_flight_ms = 1000;

    flight::test::MockImu imu;
    flight::test::MockBarometer baro;
    flight::test::MockGps gps;
    flight::test::MockRadio radio;
    flight::test::MockLogger logger;
    flight::test::MockBoard board;
    flight::Controller controller(config, imu, baro, gps, radio, logger, board);

    if (!controller.initialize()) {
        std::cerr << "controller failed to initialise: " << controller.last_error() << "\n";
        return 1;
    }

    // A simple ascent-then-descent profile so the packets carry changing values rather
    // than a constant, which would hide field-ordering mistakes in the pipeline.
    std::uint64_t now = 0;
    std::size_t emitted = 0;
    for (int step = 0; emitted < static_cast<std::size_t>(wanted) && step < 20000; ++step) {
        const double seconds = now / 1000.0;
        const double altitude = 120.0 * std::sin(seconds * 0.05);  // metres, up then down
        baro.sample.altitude_m = altitude;
        baro.sample.pressure_pa = 101325.0 * std::exp(-altitude / 8434.0);
        baro.sample.temperature_c = 25.0 - altitude * 0.0065;

        imu.sample.ax_mps2 = 0.4 * std::sin(seconds);
        imu.sample.ay_mps2 = 0.4 * std::cos(seconds);
        imu.sample.az_mps2 = 9.80665 + 0.2 * std::sin(seconds * 2.0);
        imu.sample.gz_dps = 12.0;  // steady spin, so yaw advances

        controller.poll(now);
        now += 50;
        emitted = radio.packets.size();
    }

    for (const std::string& packet : radio.packets) {
        std::cout << packet << "\n";
    }
    std::cerr << radio.packets.size() << " packets emitted\n";
    return radio.packets.empty() ? 1 : 0;
}
