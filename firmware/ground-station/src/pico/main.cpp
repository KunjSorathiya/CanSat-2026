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

void emit(const std::string& payload) {
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
    emit(std::string("#bridge=online radio=") + (up ? "1" : "0"));

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
            char line[128];
            std::snprintf(line, sizeof(line),
                          "#state=RX radio=%d frames=%lu rssi=%d snr=%.1f",
                          up ? 1 : 0, static_cast<unsigned long>(frames),
                          radio.last_rssi_dbm(), static_cast<double>(radio.last_snr_db()));
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
