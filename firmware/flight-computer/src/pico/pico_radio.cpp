#include "flight/pico/pico_hal.hpp"

#include <cstdint>
#include <cstring>

#ifdef PICO_BUILD
#include "hardware/gpio.h"
#include "hardware/spi.h"
#include "pico/stdlib.h"
#endif

namespace flight {

#ifdef PICO_BUILD

namespace {
void radio_select(void*, bool select) {
    gpio_put(BoardPins::lora_cs, select ? 0 : 1);
}
void radio_transfer(void*, const std::uint8_t* tx, std::uint8_t* rx, std::size_t len) {
    if (tx != nullptr && rx != nullptr) {
        spi_write_read_blocking(spi0, tx, rx, len);
    } else if (tx != nullptr) {
        spi_write_blocking(spi0, tx, len);
    } else if (rx != nullptr) {
        spi_read_blocking(spi0, 0x00, rx, len);
    }
}
void radio_set_reset(void*, bool level) {
    gpio_put(BoardPins::lora_reset, level ? 1 : 0);
}
void radio_delay_ms(void*, std::uint32_t ms) { sleep_ms(ms); }
bool radio_read_dio0(void*) { return gpio_get(BoardPins::lora_dio0) != 0; }
std::uint32_t radio_millis(void*) { return to_ms_since_boot(get_absolute_time()); }

cansat::Sx1278Settings settings_from(const Configuration& config, std::uint8_t sync_word) {
    cansat::Sx1278Settings s;
    s.frequency_hz = config.radio.frequency_hz;
    s.tx_power_dbm = config.radio.tx_power_dbm;
    s.spreading_factor = config.radio.spreading_factor;
    s.bandwidth_hz = config.radio.bandwidth_hz;
    s.coding_rate = config.radio.coding_rate;
    s.preamble_length = config.radio.preamble_length;
    s.enable_crc = config.radio.enable_crc;
    s.sync_word = sync_word;
    return s;
}
}  // namespace

bool PicoRadio::initialize(std::uint8_t sync_word) {
    pico_buses_init();

    gpio_init(BoardPins::lora_cs);
    gpio_set_dir(BoardPins::lora_cs, GPIO_OUT);
    gpio_put(BoardPins::lora_cs, 1);
    gpio_init(BoardPins::lora_reset);
    gpio_set_dir(BoardPins::lora_reset, GPIO_OUT);
    gpio_put(BoardPins::lora_reset, 1);
    gpio_init(BoardPins::lora_dio0);
    gpio_set_dir(BoardPins::lora_dio0, GPIO_IN);
    gpio_init(BoardPins::lora_dio1);
    gpio_set_dir(BoardPins::lora_dio1, GPIO_IN);

    cansat::Sx1278Hal hal;
    hal.ctx = nullptr;
    hal.select = radio_select;
    hal.transfer = radio_transfer;
    hal.set_reset = radio_set_reset;
    hal.delay_ms = radio_delay_ms;
    hal.read_dio0 = radio_read_dio0;
    hal.millis = radio_millis;

    healthy_ = radio_.begin(hal, settings_from(config_, sync_word));
    return healthy_;
}

bool PicoRadio::transmit(const std::string& packet) {
    if (!healthy_) return false;
    const bool ok = radio_.transmit(reinterpret_cast<const std::uint8_t*>(packet.data()),
                                    packet.size(), 2000);
    if (!ok) {
        healthy_ = radio_.healthy();
    }
    // Transmitting leaves the modem in standby. If the controller is going to listen at
    // all, the receiver has to be running before it asks -- and putting it back here, right
    // after the packet is away, gives the whole gap between packets as the listening
    // window rather than the few milliseconds around the poll.
    if (listening_) radio_.start_receive();
    return ok;
}

bool PicoRadio::poll_receive(std::string& out) {
    if (!healthy_) return false;
    // The first poll is what turns the receiver on, and only the controller's gate can
    // reach this function -- so a vehicle with allow_ground_commands false never enters
    // RX at all, and its radio behaves exactly as it did before any of this existed.
    if (!listening_) {
        listening_ = true;
        radio_.start_receive();
        return false;
    }
    std::uint8_t buffer[256];
    const std::size_t n = radio_.poll_receive(buffer, sizeof(buffer));
    if (n == 0) return false;
    out.assign(reinterpret_cast<const char*>(buffer), n);
    return true;
}

#else  // host stubs

bool PicoRadio::initialize(std::uint8_t) {
    healthy_ = false;
    return false;
}
bool PicoRadio::transmit(const std::string&) { return false; }
bool PicoRadio::poll_receive(std::string&) { return false; }

#endif

}  // namespace flight
