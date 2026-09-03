#pragma once

#include <cstddef>
#include <cstdint>

namespace cansat {

// Radio parameters shared by the flight computer and the ground-station bridge. Only the
// sync words are fixed by the competition rulebook; every other value is a provisional
// engineering default and must be confirmed against the RA-02 carrier and competition
// guidance before flight.
struct Sx1278Settings {
    std::uint32_t frequency_hz = 433000000;
    std::int8_t tx_power_dbm = 17;      // 2..17 via PA_BOOST, up to 20 with PA_DAC
    std::uint8_t spreading_factor = 9;  // 6..12
    std::uint32_t bandwidth_hz = 125000;
    std::uint8_t coding_rate = 5;       // 5..8  => 4/5 .. 4/8
    std::uint16_t preamble_length = 8;
    bool enable_crc = true;
    std::uint8_t sync_word = 0xF3;      // rulebook: test = 0xF3, official = 0xA5
};

// Hardware bindings the driver needs. Supplied by the platform (Pico SDK on the vehicle,
// a fake register bank in host tests). No callback may block indefinitely.
struct Sx1278Hal {
    void* ctx = nullptr;
    // Assert (select=true -> NSS low) or release the chip-select line.
    void (*select)(void* ctx, bool select) = nullptr;
    // Full-duplex SPI transfer of `len` bytes. rx may be nullptr to discard input.
    void (*transfer)(void* ctx, const std::uint8_t* tx, std::uint8_t* rx, std::size_t len) = nullptr;
    // Drive the RESET line (level=false -> module held in reset).
    void (*set_reset)(void* ctx, bool level) = nullptr;
    void (*delay_ms)(void* ctx, std::uint32_t ms) = nullptr;
    // Optional: read DIO0 (true when the mapped IRQ, e.g. TxDone/RxDone, is asserted).
    bool (*read_dio0)(void* ctx) = nullptr;
    // Optional: monotonic millisecond clock for timeouts. If null, delay_ms is used to
    // bound waits instead.
    std::uint32_t (*millis)(void* ctx) = nullptr;
};

class Sx1278 {
public:
    Sx1278() = default;

    // Bind hardware and bring the modem up in LoRa standby with `settings` applied.
    bool begin(const Sx1278Hal& hal, const Sx1278Settings& settings);

    bool set_sync_word(std::uint8_t sync_word);
    bool reconfigure(const Sx1278Settings& settings);  // full re-init with new parameters

    // Blocking transmit with a bounded timeout. Returns false on timeout or if the modem
    // is not ready. `len` is clamped to 255 (LoRa FIFO limit).
    bool transmit(const std::uint8_t* data, std::size_t len, std::uint32_t timeout_ms = 2000);

    void start_receive();  // enter continuous RX
    // Non-blocking: returns payload length (0 if nothing) into `out` (capacity `cap`).
    std::size_t poll_receive(std::uint8_t* out, std::size_t cap);

    int last_rssi_dbm() const { return last_rssi_dbm_; }
    float last_snr_db() const { return last_snr_db_; }
    std::uint8_t chip_version() const { return version_; }
    bool healthy() const { return healthy_; }

private:
    void write_reg(std::uint8_t reg, std::uint8_t value);
    std::uint8_t read_reg(std::uint8_t reg);
    void write_fifo(const std::uint8_t* data, std::size_t len);
    void read_fifo(std::uint8_t* out, std::size_t len);
    void set_mode(std::uint8_t mode);
    bool apply_settings(const Sx1278Settings& settings);
    std::uint32_t now_ms();
    void sleep(std::uint32_t ms);

    Sx1278Hal hal_{};
    Sx1278Settings settings_{};
    bool healthy_ = false;
    bool receiving_ = false;
    std::uint8_t version_ = 0;
    int last_rssi_dbm_ = 0;
    float last_snr_db_ = 0.0f;
};

}  // namespace cansat
