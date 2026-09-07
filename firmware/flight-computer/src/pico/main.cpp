#include "flight/config.hpp"
#include "flight/controller.hpp"
#include "flight/pico/pico_hal.hpp"

#ifdef PICO_BUILD
#include "hardware/watchdog.h"
#include "pico/stdlib.h"
#else
#include <chrono>
#include <thread>
#endif

#include <cstdint>
#include <cstdio>

namespace {

std::uint64_t now_ms() {
#ifdef PICO_BUILD
    return to_ms_since_boot(get_absolute_time());
#else
    const auto t = std::chrono::steady_clock::now().time_since_epoch();
    return static_cast<std::uint64_t>(
        std::chrono::duration_cast<std::chrono::milliseconds>(t).count());
#endif
}

void tick_delay_ms(std::uint32_t ms) {
#ifdef PICO_BUILD
    sleep_ms(ms);
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
#endif
}

flight::Configuration make_config() {
    flight::Configuration config;
    // CONFIRMED registered competition identifier, 2026-09-07. This is not a
    // placeholder and must not be "corrected" to one: the formatter and
    // validate_config() reject only the rulebook's "CAN-Team-XX" example, so a
    // wrong-but-well-formed number here would never be caught by anything.
    config.team_id = "CAN-Team-25";
    // 1 Hz: the fastest the default SF7/125 kHz modem sustains with margin. See
    // documentation/design/link-budget.md before raising this.
    config.telemetry_period_ms = cansat::link::kTelemetryPeriodMs;
    config.radio_mode = flight::RadioMode::test;  // switch to ::official for launch
    return config;
}

#ifdef PICO_BUILD
// ---- startup summary -------------------------------------------------------------
// What is fitted and what answered, printed once the sensors have actually been read.
//
// It runs from inside the flight loop rather than before it, so it delays nothing: the
// first telemetry packet still leaves on schedule. And it repeats while the vehicle is
// unarmed, because the USB port re-enumerates for a second or two after a flash and a
// single print at boot lands before anything is listening -- which is precisely how a
// working vehicle looks dead. Once armed it stops for good and the flight loop is silent
// again, which is the property the launch build is meant to have.
constexpr std::uint64_t kSummaryFirstMs = 1200;   // two health refreshes in
constexpr std::uint64_t kSummaryRepeatMs = 3000;

void print_row(const char* subsystem, const char* part, const char* status,
               const char* note) {
    std::printf("  %-11s %-10s %-9s %s\n", subsystem, part, status, note);
}

void print_startup_summary(const flight::Configuration& config,
                           const flight::Controller& controller) {
    const flight::HealthSnapshot& h = controller.health();
    const flight::FaultManager& faults = controller.faults();
    char note[72];

    std::printf("\n=====================================================\n");
    std::printf(" CanSat 2026 - flight firmware\n");
    std::printf("=====================================================\n");
    std::printf(" team %s | radio %s | telemetry every %lu ms\n", config.team_id.c_str(),
                config.radio_mode == flight::RadioMode::official ? "OFFICIAL 0xA5"
                                                                 : "TEST 0xF3",
                static_cast<unsigned long>(config.telemetry_period_ms));
    std::printf(" boot: %s\n\n", h.watchdog_reboot ? "WATCHDOG RESET" : "power-on");

    print_row("IMU", "MPU-6500", h.imu_ok ? "OK" : "FAILED",
              h.mag_present ? "nine-axis" : "six axes, no magnetometer - yaw is YR-G");
    print_row("Barometer", "BMP280", h.baro_ok ? "OK" : "FAILED", "");

    // No fix is not a failure: indoors it is the expected answer, and the receiver can be
    // perfectly healthy while it waits for sky. Checksum errors are the number that
    // separates a quiet receiver from a mis-wired one.
    std::snprintf(note, sizeof(note), "%lu checksum errors",
                  static_cast<unsigned long>(h.gps_checksum_errors));
    print_row("GPS", "NEO-6M", h.gps_fix ? "FIX" : "NO FIX", note);

    print_row("Radio", "SX1278", h.radio_ok ? "OK" : "FAILED", "");
    print_row("SD card", "-", h.sd_ok ? "OK" : "FAILED",
              h.sd_ok ? "" : "NOTHING IS BEING LOGGED");

    // sound_ok is false both for "not fitted" and "fitted but silent", and those want
    // different actions from an operator. The fault log is what separates them.
    const bool sound_silent = faults.active(flight::FaultCode::sound_unavailable);
    print_row("Sound", "LM393",
              h.sound_ok ? "OK" : (sound_silent ? "SILENT" : "NOT FITTED"),
              h.sound_ok ? "" : (sound_silent ? "wired but no signal" : "optional sensor"));

    std::printf("\n state %s | armed %s | calibrated %s\n", flight::to_string(h.state),
                h.armed ? "yes" : "no", h.calibrated ? "yes" : "no");

    // A count is not actionable. Three active faults on a vehicle where every subsystem
    // reports OK is a puzzle; "mag_unavailable watchdog_reboot calibration" is an answer.
    if (h.fault_active == 0) {
        std::printf(" faults: none\n");
    } else {
        std::printf(" faults active (%lu):", static_cast<unsigned long>(h.fault_active));
        for (std::uint8_t i = 0; i < static_cast<std::uint8_t>(flight::FaultCode::count);
             ++i) {
            const auto code = static_cast<flight::FaultCode>(i);
            if (faults.active(code)) std::printf(" %s", flight::fault_name(code));
        }
        std::printf("\n");
    }
    std::printf(" telemetry is running. This summary repeats until the vehicle arms.\n");
    std::printf("=====================================================\n");
}
#endif  // PICO_BUILD

}  // namespace

int main() {
#ifdef PICO_BUILD
    stdio_init_all();
    // The status LED goes on immediately so the vehicle visibly powers up.
    flight::pico_buses_init();
#endif

    const flight::Configuration config = make_config();

    flight::PicoImu imu(config);
    flight::PicoBarometer barometer(config);
    flight::PicoGps gps(config);
    flight::PicoRadio radio(config);
    flight::PicoSdLogger logger;
    flight::PicoBoardIo board(config);
    // Additional sensor. Passed by address because the controller treats it as optional:
    // remove this line and the vehicle flies exactly as it did before the microphone
    // existed, minus two columns in the log.
    flight::PicoSoundSensor sound(config);

    flight::Controller controller(config, imu, barometer, gps, radio, logger, board, &sound);

#ifdef PICO_BUILD
    // If the previous run hung or browned out, the watchdog reset us. Record it before
    // re-arming the watchdog; telemetry restarts automatically regardless.
    controller.set_boot_cause(watchdog_caused_reboot());
#endif

    board.set_status_led(true);
    if (!controller.initialize()) {
        // Degraded operation continues below either way. But if the configuration itself
        // was refused, the vehicle will never produce a compliant packet, and the reason
        // is the single most useful thing it can say -- so it says it on the serial console
        // an operator already has open at this point in bring-up, rather than keeping it
        // for an accessor nobody calls.
        const char* why = controller.config_error();
        if (why != nullptr && why[0] != '\0') {
            std::printf("CONFIG REFUSED: %s\n", why);
        }
    }

#ifdef PICO_BUILD
    // 2 s hardware watchdog: a hung loop reboots and telemetry restarts automatically.
    watchdog_enable(2000, true);
#endif

#ifdef PICO_BUILD
    std::uint64_t next_summary_ms = kSummaryFirstMs;
#endif

    while (true) {
        controller.poll(now_ms());
#ifdef PICO_BUILD
        // Bench aid only. It stops the moment the vehicle arms, so nothing prints in
        // flight and the launch build keeps its silence where it matters.
        if (!controller.health().armed && controller.mission_ms() >= next_summary_ms) {
            print_startup_summary(config, controller);
            next_summary_ms = controller.mission_ms() + kSummaryRepeatMs;
        }
        watchdog_update();
#endif
        // The tick is configuration, not a literal: validate_config() checks it against
        // both the sensor period and the GPS UART FIFO drain time.
        tick_delay_ms(config.loop_tick_ms);
    }

    return 0;
}
