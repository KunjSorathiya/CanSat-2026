#pragma once

#include <cstddef>
#include <cstdint>

#include "cansat/link_profile.hpp"

namespace cansat {

// Radio parameters shared by the flight computer and the ground-station bridge. Every
// default comes from the single link profile in `cansat/link_profile.hpp` so that the two
// ends of the link cannot drift apart — a modem mismatch is a silent, total link failure.
struct Sx1278Settings {
    std::uint32_t frequency_hz = link::kFrequencyHz;
    std::int8_t tx_power_dbm = link::kTxPowerDbm;  // 2..17 via PA_BOOST, up to 20 with PA_DAC
    std::uint8_t spreading_factor = link::kSpreadingFactor;  // 6..12
    std::uint32_t bandwidth_hz = link::kBandwidthHz;
    std::uint8_t coding_rate = link::kCodingRate;  // 5..8  => 4/5 .. 4/8
    std::uint16_t preamble_length = link::kPreambleSymbols;
    bool enable_crc = link::kEnableCrc;
    std::uint8_t sync_word = link::kTestSyncWord;  // rulebook: test = 0xF3, official = 0xA5
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

    // Where a packet handed to start_transmit() is.
    enum class TxStatus : std::uint8_t { idle, busy, done, failed };

    // Non-blocking transmit, in two halves. start_transmit() loads the FIFO and keys the
    // transmitter, then returns at once: the modem sends the packet on its own. It refuses a
    // second packet while one is on the air. poll_transmit() reports `busy` until the packet
    // ends, then `done` or `failed` exactly once, and `idle` when nothing is in flight. Both
    // the timeout and the airtime floor below need the HAL's millis() clock; without one the
    // blocking transmit() is the only bounded way to send.
    //
    // This is what the flight computer uses, because the blocking form stops the caller for
    // the whole airtime: in the 2026-09-10 range-test log the 33 ms sensor task ran 15 times
    // per 700 ms packet instead of 21, and about 11 times a second after MAX_RATE.
    bool start_transmit(const std::uint8_t* data, std::size_t len);
    TxStatus poll_transmit(std::uint32_t timeout_ms);
    bool tx_busy() const { return tx_active_; }

    // Blocking transmit with a bounded timeout, built on the two halves above. Returns false
    // on timeout or if the modem is not ready. `len` is clamped to 255 (LoRa FIFO limit).
    // The ground bridge and the bring-up image use it; a caller with nothing else to do loses
    // nothing by waiting.
    bool transmit(const std::uint8_t* data, std::size_t len, std::uint32_t timeout_ms = 2000);

    void start_receive();  // enter continuous RX (not while a packet is on the air)
    // Non-blocking: returns payload length (0 if nothing) into `out` (capacity `cap`).
    std::size_t poll_receive(std::uint8_t* out, std::size_t cap);

    // Packets that arrived longer than the caller's buffer and were cut to fit. A
    // truncated payload is framed and CRC'd by the bridge like any other, so it reaches
    // the ground station looking like a valid frame carrying a malformed packet -- the
    // wrong diagnosis, pointing at the vehicle rather than at the receive path. Never
    // reached with the buffers this project uses; counted so it cannot become silent if
    // one of them shrinks.
    std::uint32_t truncated_receives() const { return truncated_receives_; }
    int last_rssi_dbm() const { return last_rssi_dbm_; }
    float last_snr_db() const { return last_snr_db_; }
    std::uint8_t chip_version() const { return version_; }
    // Re-read register 0x42 over the bus, right now, and return what came back.
    //
    // chip_version() returns what was read at begin(). This asks again, which is a
    // different question: it is one small SPI transaction whose correct answer is known in
    // advance, so it doubles as a probe of whether the bus is still trustworthy. The
    // microSD shares SPI0 and has nothing buffering MISO, so a card that keeps driving the
    // line after its chip select goes high corrupts the radio's next transaction -- and the
    // symptom looks like a dead radio rather than a storage problem. Used by the bring-up
    // diagnostic to interleave radio reads with card activity; the flight firmware does not
    // call it.
    std::uint8_t probe_version();

    // IRQ_FLAGS as it stood when a transmit last timed out, and how many have timed out.
    //
    // A failed transmit looks the same from outside whatever caused it, and the two causes
    // want opposite investigations. IRQ_TX_DONE (0x08) set here means the radio finished
    // and DIO0 did not report it: a wiring fault. Clear means the transmission never
    // completed at all, which is the modem or the supply feeding it.
    std::uint8_t last_tx_irq_flags() const { return last_tx_irq_flags_; }
    std::uint32_t tx_timeouts() const { return tx_timeouts_; }
    // RegOpMode and RegVersion, read in the same breath as the IRQ flags above.
    //
    // These separate three faults that all present as "the transmit failed". A version that
    // is not 0x12 means SPI itself was broken at that instant. An OpMode with bit 7 clear
    // means the modem left LoRa mode, which only a reset does - the module browned out and
    // lost its configuration. An OpMode still reading LoRa TX means the chip took the job
    // and never finished it, which is the PLL, the PA, or the rail feeding them.
    std::uint8_t last_tx_op_mode() const { return last_tx_op_mode_; }
    std::uint8_t last_tx_version() const { return last_tx_version_; }
    // Transmits that reported done faster than their own airtime allows, and were refused
    // for it. Non-zero means the completion signal is lying - in practice a DIO0 line that
    // is not connected and floats high. Worse than a timeout, because it is silent.
    std::uint32_t tx_impossibly_fast() const { return tx_impossibly_fast_; }
    bool healthy() const { return healthy_; }

private:
    void write_reg(std::uint8_t reg, std::uint8_t value);
    std::uint8_t read_reg(std::uint8_t reg);
    void write_fifo(const std::uint8_t* data, std::size_t len);
    void read_fifo(std::uint8_t* out, std::size_t len);
    void set_mode(std::uint8_t mode);
    void capture_tx_state();
    TxStatus check_transmit(std::uint32_t elapsed_ms, std::uint32_t timeout_ms);
    void end_transmit();
    bool apply_settings(const Sx1278Settings& settings);
    std::uint32_t now_ms();
    LoraModemParams modem_params() const;
    void sleep(std::uint32_t ms);

    Sx1278Hal hal_{};
    Sx1278Settings settings_{};
    bool healthy_ = false;
    bool receiving_ = false;
    // The packet on the air: whether there is one, whether RX was running when it started
    // (and so must be resumed when it ends), when it started and how long it is.
    bool tx_active_ = false;
    bool tx_was_receiving_ = false;
    std::uint32_t tx_start_ms_ = 0;
    std::size_t tx_len_ = 0;
    std::uint8_t version_ = 0;
    std::uint8_t last_tx_irq_flags_ = 0;
    std::uint8_t last_tx_op_mode_ = 0;
    std::uint8_t last_tx_version_ = 0;
    std::uint32_t tx_timeouts_ = 0;
    std::uint32_t tx_impossibly_fast_ = 0;
    std::uint32_t truncated_receives_ = 0;
    int last_rssi_dbm_ = 0;
    float last_snr_db_ = 0.0f;
};

}  // namespace cansat
