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
    // >>> SET THIS to the registered competition identifier before any launch or
    //     official test. The formatter rejects the "CAN-Team-XX" placeholder.
    config.team_id = "CAN-Team-25";
    // 1 Hz: the fastest the default SF7/125 kHz modem sustains with margin. See
    // documentation/design/link-budget.md before raising this.
    config.telemetry_period_ms = cansat::link::kTelemetryPeriodMs;
    config.radio_mode = flight::RadioMode::test;  // switch to ::official for launch
    return config;
}

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

    flight::Controller controller(config, imu, barometer, gps, radio, logger, board);

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

    while (true) {
        controller.poll(now_ms());
#ifdef PICO_BUILD
        watchdog_update();
#endif
        // The tick is configuration, not a literal: validate_config() checks it against
        // both the sensor period and the GPS UART FIFO drain time.
        tick_delay_ms(config.loop_tick_ms);
    }

    return 0;
}
