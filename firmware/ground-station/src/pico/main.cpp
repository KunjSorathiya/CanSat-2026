#include "cansat/link_profile.hpp"
#include "cansat/sx1278.hpp"
#include "ground/framing.hpp"

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <string>

#ifdef PICO_BUILD
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "hardware/watchdog.h"
#include "pico/stdlib.h"
#ifdef PICO_STDIO_USB
#include "pico/stdio_usb.h"
#endif
#endif

// The ground-station Pico is the physical LoRa bridge: RA-02 -> Pico -> USB serial -> PC.
// It reuses the project GPIO map (documentation/hardware/pico-gpio-map.md).
namespace {
constexpr std::uint32_t PIN_SCK = 18;
constexpr std::uint32_t PIN_MOSI = 19;
constexpr std::uint32_t PIN_MISO = 16;
constexpr std::uint32_t PIN_CS = 17;
constexpr std::uint32_t PIN_RESET = 20;
constexpr std::uint32_t PIN_DIO0 = 21;

// Switch to cansat::link::kOfficialSyncWord for the official launch configuration.
constexpr std::uint8_t SYNC_WORD = cansat::link::kTestSyncWord;  // rulebook: pre-launch testing
constexpr std::uint32_t STATUS_PERIOD_MS = 1000;
constexpr std::uint8_t MAX_RADIO_FAILURES = 20;

#ifdef PICO_BUILD
void radio_select(void*, bool select) { gpio_put(PIN_CS, select ? 0 : 1); }
void radio_transfer(void*, const std::uint8_t* tx, std::uint8_t* rx, std::size_t len) {
    if (tx && rx) {
        spi_write_read_blocking(spi0, tx, rx, len);
    } else if (tx) {
        spi_write_blocking(spi0, tx, len);
    } else if (rx) {
        spi_read_blocking(spi0, 0x00, rx, len);
    }
}
void radio_set_reset(void*, bool level) { gpio_put(PIN_RESET, level ? 1 : 0); }
void radio_delay_ms(void*, std::uint32_t ms) { sleep_ms(ms); }
bool radio_dio0(void*) { return gpio_get(PIN_DIO0) != 0; }
std::uint32_t radio_millis(void*) { return to_ms_since_boot(get_absolute_time()); }
#endif

// The bridge must keep running when the PC application closes, crashes or is unplugged.
// Writing to a USB CDC endpoint with no host attached can block until the SDK's stdout
// timeout expires on every single write, and this bridge runs under a 3 s watchdog: a
// blocking write would turn "the operator closed the dashboard" into a reboot loop.
//
// So output is dropped while no host is listening. Buffering it would be worse — the
// operator wants the packet arriving now, not a backlog from before the laptop woke up,
// and an unbounded backlog on a bridge with 264 kB of RAM is its own failure.
bool host_listening() {
#if defined(PICO_BUILD) && defined(PICO_STDIO_USB)
    return stdio_usb_connected();
#else
    return true;
#endif
}

std::uint32_t g_frames_dropped_no_host = 0;

void emit(const std::string& payload) {
    if (!host_listening()) {
        ++g_frames_dropped_no_host;
        return;
    }
    const std::string frame = ground::frame_encode(payload);
    std::fwrite(frame.data(), 1, frame.size(), stdout);
    std::fflush(stdout);
}

}  // namespace

int main() {
#ifdef PICO_BUILD
    stdio_init_all();
    sleep_ms(500);

    spi_init(spi0, 4 * 1000 * 1000);
    spi_set_format(spi0, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    gpio_set_function(PIN_SCK, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MOSI, GPIO_FUNC_SPI);
    gpio_set_function(PIN_MISO, GPIO_FUNC_SPI);
    gpio_init(PIN_CS); gpio_set_dir(PIN_CS, GPIO_OUT); gpio_put(PIN_CS, 1);
    gpio_init(PIN_RESET); gpio_set_dir(PIN_RESET, GPIO_OUT); gpio_put(PIN_RESET, 1);
    gpio_init(PIN_DIO0); gpio_set_dir(PIN_DIO0, GPIO_IN);

    cansat::Sx1278Hal hal;
    hal.select = radio_select;
    hal.transfer = radio_transfer;
    hal.set_reset = radio_set_reset;
    hal.delay_ms = radio_delay_ms;
    hal.read_dio0 = radio_dio0;
    hal.millis = radio_millis;

    // Every modem parameter defaults from cansat/link_profile.hpp, the same single
    // definition the flight computer uses. The two ends must agree exactly: a mismatched
    // spreading factor or bandwidth receives nothing and looks like dead hardware.
    cansat::Sx1278Settings settings;
    settings.sync_word = SYNC_WORD;

    cansat::Sx1278 radio;
    bool up = radio.begin(hal, settings);
    if (up) {
        radio.start_receive();
    }
    // Carry the sync word on the first line too, so an operator watching the console come
    // up sees the truth immediately rather than a second later.
    {
        char online[64];
        std::snprintf(online, sizeof(online), "#bridge=online radio=%d sync=0x%02X",
                      up ? 1 : 0, static_cast<unsigned>(SYNC_WORD));
        emit(online);
    }

    std::uint8_t buffer[256];
    std::uint32_t frames = 0;
    std::uint32_t radio_failures = 0;
    std::uint32_t last_status = radio_millis(nullptr);

    // 3 s watchdog: a hung bridge reboots and the PC transport reconnects on its own.
    watchdog_enable(3000, true);

    while (true) {
        watchdog_update();
        if (!up) {
            up = radio.begin(hal, settings);
            if (up) radio.start_receive();
            sleep_ms(200);
        } else {
            const std::size_t n = radio.poll_receive(buffer, sizeof(buffer));
            if (n > 0) {
                emit(std::string(reinterpret_cast<const char*>(buffer), n));
                ++frames;
                radio_failures = 0;
            } else if (!radio.healthy()) {
                if (++radio_failures >= MAX_RADIO_FAILURES) {
                    up = false;
                    radio_failures = 0;
                    emit("#radio=lost");
                }
            }
        }

        const std::uint32_t now = radio_millis(nullptr);
        if (now - last_status >= STATUS_PERIOD_MS) {
            last_status = now;
            // 160 rather than 128: the worst case this format can produce is 121
            // characters -- both counters at 4294967295, and an SNR that is whatever
            // float the radio last returned, which formats to 42 characters at its most
            // negative. That fits 128 with seven bytes to spare, which is not enough
            // margin for the next field somebody adds. snprintf would truncate rather
            // than overflow, but a silently truncated status line is a field that
            // vanishes exactly when the link is misbehaving.
            char line[160];
            // The sync word is reported, not assumed. The rulebook uses one word for
            // testing and another for the launch, and the switch is a reflash of both
            // ends; a display that states which word is in use from a compiled-in
            // constant states it correctly right up until the moment it matters.
            std::snprintf(line, sizeof(line),
                          "#state=RX radio=%d frames=%lu dropped=%lu rssi=%d snr=%.1f "
                          "sync=0x%02X",
                          up ? 1 : 0, static_cast<unsigned long>(frames),
                          static_cast<unsigned long>(g_frames_dropped_no_host),
                          radio.last_rssi_dbm(), static_cast<double>(radio.last_snr_db()),
                          static_cast<unsigned>(SYNC_WORD));
            emit(line);
        }
        sleep_ms(2);
    }
#else
    // Host build: no radio hardware. Kept so the file is a valid translation unit.
    emit("#bridge=host-stub");
    return 0;
#endif
}
