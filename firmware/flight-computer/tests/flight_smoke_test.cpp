#include "flight/controller.hpp"
#include "flight/gps_parser.hpp"
#include "mock_hardware.hpp"

#include <cassert>
#include <iostream>

int main() {
    flight::Configuration config;
    config.team_id = "CAN-Team-01";
    flight::test::MockImu imu;
    flight::test::MockBarometer barometer;
    flight::test::MockGps gps;
    flight::test::MockRadio radio;
    flight::test::MockLogger logger;
    flight::test::MockBoard board;
    flight::Controller controller(config, imu, barometer, gps, radio, logger, board);

    assert(controller.initialize());
    assert(board.led_on);
    assert(radio.sync_word_ == 0xF3);
    // Default telemetry period is 1000 ms: the radio cannot sustain faster with the
    // default SF7/125 kHz modem (documentation/design/link-budget.md).
    controller.poll(0);
    controller.poll(1000);
    controller.poll(2000);
    assert(radio.packets.size() == 3);
    assert(radio.packets.front().find("CAN-Team-01; P-001; Ti-00:00:00:000;") == 0);
    assert(radio.packets.back().find("P-003") != std::string::npos);

    flight::NmeaParser gps_parser;
    const std::string nmea = "$GPGGA,123519,4807.038,N,01131.000,E,1,08,0.9,545.4,M,46.9,M,,*47\r\n";
    for (const char character : nmea) gps_parser.consume(character);
    assert(gps_parser.has_fix());
    assert(gps_parser.latest().latitude > 48.1 && gps_parser.latest().latitude < 48.2);
    assert(gps_parser.checksum_errors() == 0);

    std::cout << "flight smoke test passed\n";
}
