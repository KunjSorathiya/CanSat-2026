#include "flight/config.hpp"
#include "flight/controller.hpp"
#include "flight/pico/pico_hal.hpp"

// The ground-command password lives in a gitignored header and never in the repository.
// Without it this build has no uplink at all -- no command window, and calibration and arming
// straight after power-on, exactly as before the window existed. Copy
// flight/local_secrets.example.hpp to flight/local_secrets.hpp and set a password to enable it.
#if __has_include("flight/local_secrets.hpp")
#include "flight/local_secrets.hpp"
#define CANSAT_HAVE_LOCAL_SECRETS 1
#endif

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

#ifdef CANSAT_HAVE_LOCAL_SECRETS
constexpr bool same_text(const char* a, const char* b) {
    return *a == *b && (*a == '\0' || same_text(a + 1, b + 1));
}
constexpr std::size_t text_length(const char* a) { return *a == '\0' ? 0 : 1 + text_length(a + 1); }
// A flight build with the uplink must not carry a password anyone who has read the repository
// knows. Refused at compile time, so it cannot be discovered on the pad.
static_assert(!same_text(cansat_local::kCommandPassword, "SET-ME") &&
                  !same_text(cansat_local::kCommandPassword, "change-me") &&
                  text_length(cansat_local::kCommandPassword) >= 8,
              "set your own ground-command password (8+ characters) in flight/local_secrets.hpp");
#endif

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
    // 700 ms -- 1.43 Hz. The 50 % duty cap puts the floor at 689 ms for the 213-byte
    // rich packet every normal-flight packet now is, and 700 ms takes it at 49 % duty --
    // under the limit, though not by much. It is not a free parameter: transmit_gps, the packet budget and this period
    // move together, and validate_config() refuses a combination where they disagree. See
    // documentation/design/link-budget.md before changing any of them.
    config.telemetry_period_ms = cansat::link::kTelemetryPeriodMs;
    config.radio_mode = flight::RadioMode::test;  // switch to ::official for launch
#ifdef CANSAT_HAVE_LOCAL_SECRETS
    // The uplink, and with it the pre-arm command window: five minutes from power-on to send
    // MAX_RATE, after which the vehicle recalibrates on the pad and arms. The drone must not
    // lift off until it has -- a launch inside the window is not detected.
    config.allow_ground_commands = true;
    config.command_password = cansat_local::kCommandPassword;
#endif
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
    // The rate is printed in Hz as well as in milliseconds, and it is not decoration: the
    // failure this summary is most likely to be consulted about is "the link is running at
    // 1 Hz and I do not know why", whose usual cause is an image flashed before the period
    // changed. A line that says 1.43 Hz settles that in one glance.
    // The LIVE period, not the configured one. After an accepted MAX_RATE command the
    // configured value is no longer what the vehicle is doing, and a summary that kept
    // quoting it would answer the question this line exists for with the one number that
    // has since become wrong.
    const std::uint32_t period_ms = controller.telemetry_period_ms();
    if (controller.health().rate_maxed) {
        // After MAX_RATE the period alternates by slot, so the live period is whichever slot
        // this print happened to land in. The pattern and its real rate are what matter.
        std::printf(" team %s | radio %s | telemetry MAX-RATE %lu/%lu/%lu ms (%.2f Hz)\n",
                    config.team_id.c_str(),
                    config.radio_mode == flight::RadioMode::official ? "OFFICIAL 0xA5"
                                                                     : "TEST 0xF3",
                    static_cast<unsigned long>(cansat::link::kMaxRateRichSlotMs),
                    static_cast<unsigned long>(cansat::link::kMaxRateLeanSlotMs),
                    static_cast<unsigned long>(cansat::link::kMaxRateLeanSlotMs),
                    3000.0 / static_cast<double>(cansat::link::kMaxRateCycleMs));
    } else {
        std::printf(" team %s | radio %s | telemetry every %lu ms (%.2f Hz)\n",
                    config.team_id.c_str(),
                    config.radio_mode == flight::RadioMode::official ? "OFFICIAL 0xA5"
                                                                     : "TEST 0xF3",
                    static_cast<unsigned long>(period_ms),
                    period_ms == 0 ? 0.0 : 1000.0 / static_cast<double>(period_ms));
    }
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

    // The latch belongs on this line rather than in a fault: it is not a failure, it is a
    // state the vehicle cannot leave, and the operator who is about to wonder why the other
    // button does nothing should be able to read the answer here.
    std::printf("\n state %s | armed %s | calibrated %s | rate %s\n", flight::to_string(h.state),
                h.armed ? "yes" : "no", h.calibrated ? "yes" : "no",
                h.rate_maxed ? "COMMANDED MAX - uplink closed" : "normal");
    // What the uplink has actually done, which is the one thing an operator cannot see
    // from the ground. "The button did nothing" has three causes that want three different
    // fixes -- the vehicle never heard the frame, heard it and refused it, or accepted it and
    // then something else happened -- and without these two counters they all look alike.
    if (!config.allow_ground_commands) {
        std::printf(" uplink    disabled in this build (no flight/local_secrets.hpp)\n");
    } else if (h.command_window_open) {
        const unsigned long left_s = (h.command_window_left_ms + 999) / 1000;
        std::printf(" uplink    OPEN for %lu:%02lu more -- not arming until it closes"
                    " | commands accepted %lu, refused %lu\n",
                    left_s / 60, left_s % 60,
                    static_cast<unsigned long>(h.ground_commands_accepted),
                    static_cast<unsigned long>(h.ground_commands_ignored));
    } else {
        std::printf(" uplink    CLOSED%s -- recalibrating, then arming"
                    " | commands accepted %lu, refused %lu\n",
                    h.rate_maxed ? " by MAX_RATE" : "",
                    static_cast<unsigned long>(h.ground_commands_accepted),
                    static_cast<unsigned long>(h.ground_commands_ignored));
    }

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
