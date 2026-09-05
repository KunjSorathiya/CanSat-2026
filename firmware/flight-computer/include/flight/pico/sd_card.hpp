#pragma once

#include "flight/pico/pico_types.hpp"

#include <cstddef>
#include <cstdint>

namespace flight::pico {

// Hardware bindings the SD driver needs. Supplied by the Pico SDK on the vehicle and by a
// simulated card in host tests, exactly as the SX1278 driver does — the microSD reader is
// the project's highest-risk integration item, so its command sequence is worth executing
// somewhere other than a launch pad.
//
// No callback may block indefinitely.
struct SdCardHal {
    void* ctx = nullptr;
    // Assert (select = true -> CS low) or release the chip-select line.
    void (*select)(void* ctx, bool select) = nullptr;
    // Full-duplex byte exchange. Returns the byte clocked in while `value` goes out.
    std::uint8_t (*transfer)(void* ctx, std::uint8_t value) = nullptr;
    // Set the SPI clock. The card must be initialised at <= 400 kHz.
    void (*set_baudrate)(void* ctx, std::uint32_t hz) = nullptr;
    void (*delay_ms)(void* ctx, std::uint32_t ms) = nullptr;
    // Monotonic millisecond clock, used to bound the initialisation loop.
    std::uint32_t (*millis)(void* ctx) = nullptr;
};

// Minimal SD/SDHC card driver in SPI mode: CMD0/CMD8/ACMD41/CMD58/CMD16 init, then
// single-block CMD17 read and CMD24 write. 512-byte blocks only. No filesystem; pair with
// RawBlockLog for the onboard log.
//
// The breakout on this vehicle is a 2.6-3.6 V SPI module running from the same 3.3 V rail
// as everything else, so there is no supply sequencing to do here and no level shifting to
// account for. CMD8 asks for the 2.7-3.6 V range accordingly.
//
// Every operation is bounded and every failure path returns rather than retrying: losing
// the card must cost the log, never a telemetry packet.
class SdCard {
public:
    static constexpr std::size_t kBlockSize = 512;
    static constexpr std::uint32_t kInitBaud = 400000;
    static constexpr std::uint32_t kRunBaud = 4000000;

    // The clock reads and writes actually run at. It is kRunBaud unless something moves it,
    // and the only thing that does is the bring-up diagnostic: a card that fails a write at
    // 4 MHz and passes the same write at 400 kHz has a signal-integrity problem, and one
    // that fails at both does not. That is a question a handheld meter cannot answer and
    // this can, so the rate has to be reachable from outside.
    void set_run_baud(std::uint32_t hz) { run_baud_ = hz; }
    std::uint32_t run_baud() const { return run_baud_; }

    // Vehicle entry point: builds the Pico SDK bindings and calls begin_with().
    bool begin(spi_bus_t spi, std::uint32_t cs_gpio);

    // Testable entry point: drives the card through the supplied bindings.
    bool begin_with(const SdCardHal& hal);

    bool read_block(std::uint32_t lba, std::uint8_t* out512);
    bool write_block(std::uint32_t lba, const std::uint8_t* in512);
    bool ok() const { return ok_; }
    bool high_capacity() const { return sdhc_; }

    // How far initialisation got, and the last R1 the card returned. "The card failed to
    // initialise" is one bit of information for at least three unrelated faults, and on a
    // bench they need different fixes: no answer at all is wiring or power, a card that
    // answers CMD0 but stalls in ACMD41 is a card problem, and one that reaches CMD16 and
    // fails is a block-length problem. Reported by the bring-up diagnostic; the flight
    // firmware ignores it and simply flies without a log.
    enum class Stage : std::uint8_t {
        not_started,
        bad_hal,        // the caller supplied an incomplete binding
        cmd0_idle,      // GO_IDLE_STATE: the first word the card ever says
        cmd8_ifcond,    // SEND_IF_COND: voltage range and check pattern
        acmd41_ready,   // SD_SEND_OP_COND: leaving idle, the slow one
        cmd58_ocr,      // READ_OCR: block- against byte-addressing
        cmd16_blocklen, // SET_BLOCKLEN: only reached on a standard-capacity card
        complete,
    };

    // Where a write stopped, and what the card said. Five distinct failure points that
    // look like one "write failed" from outside, and they mean different things: a card
    // that will not leave busy, a command it rejects outright, data it refuses, a
    // programming cycle that never ends, or an error it only admits to when asked
    // afterwards. The last of those is where write protection shows up.
    enum class WriteStage : std::uint8_t {
        none,
        busy_before,          // still busy from the previous write before CMD24
        cmd24_rejected,       // WRITE_BLOCK itself refused
        data_rejected,        // the data-response token was not "accepted"
        programming_timeout,  // accepted, then never finished programming
        status_error,         // CMD13 reported an error after the fact
        complete,
    };

    WriteStage write_stage() const { return write_stage_; }
    std::uint8_t last_write_r1() const { return last_write_r1_; }
    std::uint8_t last_write_r2() const { return last_write_r2_; }
    std::uint8_t last_data_response() const { return last_data_response_; }
    static const char* describe(WriteStage stage);

    Stage stage() const { return stage_; }
    std::uint8_t last_r1() const { return last_r1_; }
    // Milliseconds spent in the ACMD41 loop. A healthy card leaves idle in tens of ms.
    std::uint32_t init_wait_ms() const { return init_wait_ms_; }
    // How many CMD0 attempts it took. More than one is normal and not a fault: the
    // first response after power-up carries sticky error bits latched from bus noise.
    int cmd0_attempts() const { return cmd0_attempts_; }
    static const char* describe(Stage stage);

private:
    std::uint32_t run_baud_ = kRunBaud;
    void select(bool on);
    std::uint8_t transfer(std::uint8_t value);
    void clock_bytes(std::size_t n);
    std::uint8_t command(std::uint8_t cmd, std::uint32_t arg, std::uint8_t crc);
    std::uint8_t wait_ready();
    // Deselect and clock one more byte so the card releases MISO. SPI0 is shared with the
    // radio: a card still driving the line corrupts the next radio transaction.
    void release();

    SdCardHal hal_{};
    Stage stage_ = Stage::not_started;
    std::uint8_t last_r1_ = 0xFF;
    std::uint32_t init_wait_ms_ = 0;
    int cmd0_attempts_ = 0;
    WriteStage write_stage_ = WriteStage::none;
    std::uint8_t last_write_r1_ = 0;
    std::uint8_t last_write_r2_ = 0;
    std::uint8_t last_data_response_ = 0;
    bool ok_ = false;
    bool sdhc_ = false;
};

}  // namespace flight::pico
