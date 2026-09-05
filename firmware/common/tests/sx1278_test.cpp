// Host tests for the SX1278 / RA-02 LoRa driver, against a fake register bank.
//
// The driver reaches hardware only through the `Sx1278Hal` callback struct, so the whole
// register sequence can be executed here: no radio, no Pico, no SDK. That does not prove
// the module behaves as the datasheet says — only a real RA-02 can do that — but it does
// prove the driver writes the registers it means to write, in the order the datasheet
// requires, and that every wait is bounded.

#include "cansat/link_profile.hpp"
#include "cansat/sx1278.hpp"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <vector>

namespace {

int g_failures = 0;
int g_checks = 0;

#define CHECK(cond)                                                                 \
    do {                                                                           \
        ++g_checks;                                                                \
        if (!(cond)) {                                                             \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n"; \
            ++g_failures;                                                          \
        }                                                                          \
    } while (0)

// A simulated SX1278: 128 registers plus a 256-byte FIFO, driven through the same SPI
// framing the real part uses (bit 7 of the address selects a write).
struct FakeRadio {
    std::uint8_t reg[0x80] = {};
    std::uint8_t fifo[256] = {};
    std::uint8_t fifo_ptr = 0;

    bool selected = false;
    bool reset_level = true;
    bool dio0 = false;
    // A real radio asserts TxDone after the packet's airtime, not instantly. When this is
    // set, DIO0 comes up only once that much simulated time has passed since MODE_TX --
    // which is what makes an honest transmission distinguishable from a floating line
    // reading high. Zero keeps the old immediate behaviour for tests that want it.
    std::uint32_t dio0_delay_ms = 0;
    std::uint32_t tx_started_ms = 0;
    std::uint32_t millis = 0;

    // Observability for the assertions.
    std::vector<std::uint8_t> mode_history;
    std::vector<std::uint8_t> written_payload;
    int reset_pulses = 0;
    std::uint32_t total_delay_ms = 0;

    // A module that loses power mid-transmit comes back reset: out of LoRa mode with its
    // whole configuration gone. From outside that is indistinguishable from a radio that is
    // simply slow, which is exactly why the driver reads RegOpMode at the failure. Zero
    // disables. Non-zero makes every access after that simulated time see a reset part.
    std::uint32_t brownout_after_ms = 0;

    // Address latched at the start of the current transfer.
    int pending_addr = -1;
    bool pending_write = false;

    FakeRadio() {
        reg[0x42] = 0x12;  // REG_VERSION: SX1276/78 silicon
        reg[0x0C] = 0x20;  // REG_LNA reset value
    }

    void transfer(const std::uint8_t* tx, std::uint8_t* rx, std::size_t len) {
        if (brownout_after_ms != 0 && millis >= brownout_after_ms) {
            reg[0x01] = 0x09;  // FSK standby - the reset default, with the LoRa bit clear
        }
        for (std::size_t i = 0; i < len; ++i) {
            const std::uint8_t out_byte = tx ? tx[i] : 0x00;
            if (pending_addr < 0) {
                pending_write = (out_byte & 0x80) != 0;
                pending_addr = out_byte & 0x7F;
                if (rx) rx[i] = 0x00;
                continue;
            }
            if (pending_addr == 0x00) {  // FIFO
                if (pending_write) {
                    fifo[fifo_ptr++] = out_byte;
                } else if (rx) {
                    rx[i] = fifo[fifo_ptr++];
                }
                continue;
            }
            if (pending_write) {
                write_register(static_cast<std::uint8_t>(pending_addr), out_byte);
            } else if (rx) {
                rx[i] = reg[pending_addr];
            }
        }
    }

    void write_register(std::uint8_t addr, std::uint8_t value) {
        if (addr == 0x01) {  // REG_OP_MODE
            mode_history.push_back(value);
            if ((value & 0x07) == 0x03) tx_started_ms = millis;  // entered TX
        }
        if (addr == 0x0D) {  // REG_FIFO_ADDR_PTR
            fifo_ptr = value;
        }
        if (addr == 0x12) {  // REG_IRQ_FLAGS is write-1-to-clear
            reg[addr] = static_cast<std::uint8_t>(reg[addr] & ~value);
            return;
        }
        reg[addr] = value;
    }
};

// ---- HAL glue ---------------------------------------------------------------------
void hal_select(void* ctx, bool select) {
    auto* radio = static_cast<FakeRadio*>(ctx);
    radio->selected = select;
    if (!select) {
        radio->pending_addr = -1;  // transaction ends on deselect
    }
}
void hal_transfer(void* ctx, const std::uint8_t* tx, std::uint8_t* rx, std::size_t len) {
    static_cast<FakeRadio*>(ctx)->transfer(tx, rx, len);
}
void hal_set_reset(void* ctx, bool level) {
    auto* radio = static_cast<FakeRadio*>(ctx);
    if (radio->reset_level && !level) ++radio->reset_pulses;
    radio->reset_level = level;
}
void hal_delay(void* ctx, std::uint32_t ms) {
    auto* radio = static_cast<FakeRadio*>(ctx);
    radio->total_delay_ms += ms;
    radio->millis += ms;
}
bool hal_dio0(void* ctx) {
    auto* r = static_cast<FakeRadio*>(ctx);
    if (r->dio0_delay_ms == 0) return r->dio0;
    return r->millis - r->tx_started_ms >= r->dio0_delay_ms;
}
std::uint32_t hal_millis(void* ctx) { return static_cast<FakeRadio*>(ctx)->millis; }

cansat::Sx1278Hal make_hal(FakeRadio& radio, bool with_dio0 = true,
                           bool with_clock = true) {
    cansat::Sx1278Hal hal;
    hal.ctx = &radio;
    hal.select = hal_select;
    hal.transfer = hal_transfer;
    hal.set_reset = hal_set_reset;
    hal.delay_ms = hal_delay;
    hal.read_dio0 = with_dio0 ? hal_dio0 : nullptr;
    hal.millis = with_clock ? hal_millis : nullptr;
    return hal;
}

// ---- tests ------------------------------------------------------------------------

void test_begin_requires_the_right_silicon() {
    FakeRadio radio;
    cansat::Sx1278 driver;
    CHECK(driver.begin(make_hal(radio), cansat::Sx1278Settings{}));
    CHECK(driver.healthy());
    CHECK(radio.reset_pulses == 1);  // hardware reset was pulsed low

    // A part that reports anything but 0x12 must be rejected, not configured blindly.
    FakeRadio wrong;
    wrong.reg[0x42] = 0x00;  // e.g. nothing on the bus, MISO reads low
    cansat::Sx1278 rejected;
    CHECK(!rejected.begin(make_hal(wrong), cansat::Sx1278Settings{}));
    CHECK(!rejected.healthy());

    FakeRadio floating;
    floating.reg[0x42] = 0xFF;  // MISO floating high
    cansat::Sx1278 rejected2;
    CHECK(!rejected2.begin(make_hal(floating), cansat::Sx1278Settings{}));
}

void test_begin_requires_a_complete_hal() {
    FakeRadio radio;
    cansat::Sx1278 driver;
    cansat::Sx1278Hal hal = make_hal(radio);
    hal.transfer = nullptr;
    CHECK(!driver.begin(hal, cansat::Sx1278Settings{}));
}

void test_lora_mode_is_entered_from_sleep() {
    // The datasheet allows the LongRangeMode bit to be changed only in sleep mode.
    FakeRadio radio;
    cansat::Sx1278 driver;
    CHECK(driver.begin(make_hal(radio), cansat::Sx1278Settings{}));
    CHECK(!radio.mode_history.empty());
    // Every mode write carries the LoRa bit, and the first is sleep.
    for (const std::uint8_t mode : radio.mode_history) {
        CHECK((mode & 0x80) != 0);
    }
    CHECK((radio.mode_history.front() & 0x07) == 0x00);  // sleep
    CHECK((radio.mode_history.back() & 0x07) == 0x01);   // standby when begin() returns
}

void test_project_settings_reach_the_registers() {
    FakeRadio radio;
    cansat::Sx1278 driver;
    cansat::Sx1278Settings settings;  // defaults come from the shared link profile
    CHECK(driver.begin(make_hal(radio), settings));

    // Frequency: Frf = f / (32 MHz / 2^19) = 433e6 / 61.03515625 = 7094272 = 0x6C4000.
    const std::uint32_t frf = (static_cast<std::uint32_t>(radio.reg[0x06]) << 16) |
                              (static_cast<std::uint32_t>(radio.reg[0x07]) << 8) |
                              radio.reg[0x08];
    CHECK(frf == 0x6C4000u);

    // MODEM_CONFIG_1: bandwidth index 7 (125 kHz), coding rate 4/5 -> 1, explicit header.
    CHECK(radio.reg[0x1D] == ((7u << 4) | (1u << 1) | 0u));

    // MODEM_CONFIG_2: SF7 with the payload CRC enabled.
    CHECK(radio.reg[0x1E] == ((7u << 4) | 0x04));

    // MODEM_CONFIG_3: 1.024 ms symbols, so low-data-rate optimisation stays off; AGC on.
    CHECK(radio.reg[0x26] == 0x04);

    // Preamble, sync word, and the rulebook's test identity.
    CHECK(radio.reg[0x20] == 0x00);
    CHECK(radio.reg[0x21] == 8);
    CHECK(radio.reg[0x39] == cansat::link::kTestSyncWord);

    // PA: 17 dBm on PA_BOOST without PA_DAC -> OutputPower = 15.
    CHECK(radio.reg[0x4D] == 0x84);
    CHECK(radio.reg[0x09] == (0x80u | 15u));

    // Both FIFO base addresses at zero: the full buffer is available each way.
    CHECK(radio.reg[0x0E] == 0x00);
    CHECK(radio.reg[0x0F] == 0x00);
}

void test_high_power_uses_pa_dac() {
    FakeRadio radio;
    cansat::Sx1278 driver;
    cansat::Sx1278Settings settings;
    settings.tx_power_dbm = 20;
    CHECK(driver.begin(make_hal(radio), settings));
    CHECK(radio.reg[0x4D] == 0x87);            // PA_DAC enabled
    CHECK(radio.reg[0x09] == (0x80u | 15u));   // Pout = 5 + 15 = 20 dBm
}

void test_out_of_range_settings_are_clamped_not_wrapped() {
    FakeRadio radio;
    cansat::Sx1278 driver;
    cansat::Sx1278Settings settings;
    settings.spreading_factor = 200;
    settings.coding_rate = 99;
    settings.tx_power_dbm = 127;
    CHECK(driver.begin(make_hal(radio), settings));
    CHECK((radio.reg[0x1E] >> 4) == 12);            // clamped to SF12
    CHECK(((radio.reg[0x1D] >> 1) & 0x07) == 4);    // clamped to 4/8
    CHECK((radio.reg[0x09] & 0x0F) <= 15);          // never overflows the field
}

void test_low_data_rate_optimisation_follows_the_symbol_time() {
    FakeRadio slow;
    cansat::Sx1278 driver;
    cansat::Sx1278Settings settings;
    settings.spreading_factor = 12;  // 32.768 ms symbols at 125 kHz
    CHECK(driver.begin(make_hal(slow), settings));
    CHECK((slow.reg[0x26] & 0x08) != 0);

    FakeRadio fast;
    cansat::Sx1278 driver2;
    settings.spreading_factor = 9;  // 4.096 ms symbols
    CHECK(driver2.begin(make_hal(fast), settings));
    CHECK((fast.reg[0x26] & 0x08) == 0);
}

void test_sf6_gets_its_special_detection_settings() {
    FakeRadio radio;
    cansat::Sx1278 driver;
    cansat::Sx1278Settings settings;
    settings.spreading_factor = 6;
    CHECK(driver.begin(make_hal(radio), settings));
    CHECK(radio.reg[0x31] == 0xC5);
    CHECK(radio.reg[0x37] == 0x0C);

    FakeRadio normal;
    cansat::Sx1278 driver2;
    settings.spreading_factor = 7;
    CHECK(driver2.begin(make_hal(normal), settings));
    CHECK(normal.reg[0x31] == 0xC3);
    CHECK(normal.reg[0x37] == 0x0A);
}

void test_transmit_loads_the_fifo_and_completes_on_dio0() {
    FakeRadio radio;
    cansat::Sx1278 driver;
    CHECK(driver.begin(make_hal(radio), cansat::Sx1278Settings{}));

    // TxDone after a realistic airtime. A 19-byte packet takes about 51 ms at SF7/125 kHz,
    // and the driver now refuses anything that claims to finish in under half of that -
    // see test_a_transmit_that_finishes_faster_than_its_airtime_is_refused for why.
    radio.dio0_delay_ms = 60;
    const char* packet = "CAN-Team-01; P-001;";
    const std::size_t len = std::strlen(packet);
    CHECK(driver.transmit(reinterpret_cast<const std::uint8_t*>(packet), len, 2000));

    CHECK(radio.reg[0x22] == len);                       // payload length register
    CHECK(std::memcmp(radio.fifo, packet, len) == 0);    // payload actually in the FIFO
    CHECK(radio.reg[0x40] == 0x40);                      // DIO0 mapped to TxDone
    // Mode sequence ends in standby, having passed through TX.
    bool saw_tx = false;
    for (const std::uint8_t mode : radio.mode_history) {
        if ((mode & 0x07) == 0x03) saw_tx = true;
    }
    CHECK(saw_tx);
    CHECK((radio.mode_history.back() & 0x07) == 0x01);
}

void test_transmit_times_out_instead_of_hanging() {
    FakeRadio radio;
    cansat::Sx1278 driver;
    CHECK(driver.begin(make_hal(radio), cansat::Sx1278Settings{}));

    radio.dio0 = false;  // TxDone never arrives
    const std::uint8_t payload[] = {1, 2, 3};
    const std::uint32_t before = radio.millis;
    CHECK(!driver.transmit(payload, sizeof(payload), 50));
    CHECK(radio.millis >= before);                       // the clock was consulted
    CHECK((radio.mode_history.back() & 0x07) == 0x01);   // left in standby, not stuck in TX
}

void test_transmit_is_bounded_without_a_clock_too() {
    // With no millis() the driver must fall back to counting its own delays.
    FakeRadio radio;
    cansat::Sx1278 driver;
    CHECK(driver.begin(make_hal(radio, true, false), cansat::Sx1278Settings{}));
    radio.dio0 = false;
    const std::uint32_t before = radio.total_delay_ms;
    CHECK(!driver.transmit(reinterpret_cast<const std::uint8_t*>("x"), 1, 20));
    CHECK(radio.total_delay_ms - before >= 20);
}

void test_transmit_rejects_nonsense_and_an_unhealthy_radio() {
    FakeRadio radio;
    cansat::Sx1278 driver;
    CHECK(driver.begin(make_hal(radio), cansat::Sx1278Settings{}));
    radio.dio0 = true;
    CHECK(!driver.transmit(nullptr, 10, 100));
    const std::uint8_t payload[] = {1};
    CHECK(!driver.transmit(payload, 0, 100));

    FakeRadio dead;
    dead.reg[0x42] = 0x00;
    cansat::Sx1278 unhealthy;
    unhealthy.begin(make_hal(dead), cansat::Sx1278Settings{});
    CHECK(!unhealthy.transmit(payload, 1, 100));
}

void test_receive_returns_a_payload_and_drops_a_crc_failure() {
    FakeRadio radio;
    cansat::Sx1278 driver;
    CHECK(driver.begin(make_hal(radio), cansat::Sx1278Settings{}));
    driver.start_receive();
    CHECK((radio.mode_history.back() & 0x07) == 0x05);  // continuous RX
    CHECK(radio.reg[0x40] == 0x00);                     // DIO0 mapped to RxDone

    const char* packet = "CAN-Team-01; P-042;";
    const std::uint8_t len = static_cast<std::uint8_t>(std::strlen(packet));
    std::memcpy(radio.fifo, packet, len);
    radio.reg[0x10] = 0x00;  // FIFO_RX_CURRENT_ADDR
    radio.reg[0x13] = len;   // RX_NB_BYTES
    radio.reg[0x12] = 0x40;  // IRQ: RxDone

    std::uint8_t out[256] = {};
    const std::size_t got = driver.poll_receive(out, sizeof(out));
    CHECK(got == len);
    CHECK(std::memcmp(out, packet, len) == 0);
    CHECK(radio.reg[0x12] == 0x00);  // IRQ flags cleared

    // Nothing pending now.
    CHECK(driver.poll_receive(out, sizeof(out)) == 0);

    // A frame whose payload CRC failed must be dropped, not handed up as telemetry.
    std::memcpy(radio.fifo, "corrupt", 7);
    radio.reg[0x13] = 7;
    radio.reg[0x12] = 0x40 | 0x20;  // RxDone + PayloadCrcError
    CHECK(driver.poll_receive(out, sizeof(out)) == 0);
    CHECK(radio.reg[0x12] == 0x00);
}

void test_a_payload_longer_than_the_buffer_is_cut_and_counted() {
    // A truncated payload is framed and CRC'd by the bridge exactly like a whole one, so
    // it reaches the ground station as a valid frame carrying a malformed packet -- a
    // diagnosis pointing at the vehicle when the fault is in the receive path. The buffers
    // this project uses make it unreachable; the counter is what keeps it from ever being
    // silent if one of them shrinks.
    FakeRadio radio;
    cansat::Sx1278 driver;
    CHECK(driver.begin(make_hal(radio), cansat::Sx1278Settings{}));
    driver.start_receive();
    CHECK(driver.truncated_receives() == 0);

    for (int i = 0; i < 40; ++i) radio.fifo[i] = static_cast<std::uint8_t>('a' + (i % 26));
    radio.reg[0x13] = 40;    // RX_NB_BYTES: forty bytes waiting
    radio.reg[0x12] = 0x40;  // RxDone

    std::uint8_t small[8] = {};
    const std::size_t got = driver.poll_receive(small, sizeof(small));
    CHECK(got == sizeof(small));
    CHECK(driver.truncated_receives() == 1);
    CHECK(std::memcmp(small, radio.fifo, sizeof(small)) == 0);

    // A packet that fits is not counted, and does not reset the count of ones that did not.
    radio.reg[0x13] = 4;
    radio.reg[0x12] = 0x40;
    std::uint8_t room[64] = {};
    CHECK(driver.poll_receive(room, sizeof(room)) == 4);
    CHECK(driver.truncated_receives() == 1);
}

void test_receive_needs_receive_mode_and_a_buffer() {
    FakeRadio radio;
    cansat::Sx1278 driver;
    CHECK(driver.begin(make_hal(radio), cansat::Sx1278Settings{}));
    std::uint8_t out[8] = {};
    radio.reg[0x12] = 0x40;
    CHECK(driver.poll_receive(out, sizeof(out)) == 0);  // start_receive() never called
    driver.start_receive();
    CHECK(driver.poll_receive(nullptr, 8) == 0);
    CHECK(driver.poll_receive(out, 0) == 0);
}

void test_rssi_uses_the_low_frequency_offset_at_433_mhz() {
    // Semtech 5.5.5: -164 dBm offset below 525 MHz, -157 dBm above 862 MHz. The RA-02 is
    // a 433 MHz module, so using the HF offset would report 7 dB more signal than exists.
    FakeRadio radio;
    cansat::Sx1278 driver;
    CHECK(driver.begin(make_hal(radio), cansat::Sx1278Settings{}));  // 433 MHz default
    driver.start_receive();

    radio.fifo[0] = 'x';
    radio.reg[0x13] = 1;
    radio.reg[0x10] = 0x00;
    radio.reg[0x19] = 40;    // SNR raw 40 -> +10.0 dB
    radio.reg[0x1A] = 100;   // RSSI raw
    radio.reg[0x12] = 0x40;

    std::uint8_t out[8] = {};
    CHECK(driver.poll_receive(out, sizeof(out)) == 1);
    CHECK(driver.last_snr_db() > 9.9f && driver.last_snr_db() < 10.1f);
    CHECK(driver.last_rssi_dbm() == -64);  // -164 + 100, positive SNR adds nothing

    // A negative SNR is subtracted, per the datasheet's weak-signal correction.
    radio.reg[0x19] = static_cast<std::uint8_t>(static_cast<std::int8_t>(-40));  // -10 dB
    radio.reg[0x1A] = 100;
    radio.reg[0x12] = 0x40;
    radio.reg[0x13] = 1;
    CHECK(driver.poll_receive(out, sizeof(out)) == 1);
    CHECK(driver.last_rssi_dbm() == -74);

    // A 868 MHz configuration would legitimately use the high-frequency offset.
    FakeRadio hf;
    cansat::Sx1278 hf_driver;
    cansat::Sx1278Settings hf_settings;
    hf_settings.frequency_hz = 868000000;
    CHECK(hf_driver.begin(make_hal(hf), hf_settings));
    hf_driver.start_receive();
    hf.reg[0x13] = 1;
    hf.reg[0x19] = 40;
    hf.reg[0x1A] = 100;
    hf.reg[0x12] = 0x40;
    CHECK(hf_driver.poll_receive(out, sizeof(out)) == 1);
    CHECK(hf_driver.last_rssi_dbm() == -57);
}

void test_sync_word_can_be_switched_for_the_official_launch() {
    FakeRadio radio;
    cansat::Sx1278 driver;
    CHECK(driver.begin(make_hal(radio), cansat::Sx1278Settings{}));
    CHECK(radio.reg[0x39] == 0xF3);
    CHECK(driver.set_sync_word(cansat::link::kOfficialSyncWord));
    CHECK(radio.reg[0x39] == 0xA5);

    // The switch is read back, so a dead SPI bus reports failure instead of pretending.
    cansat::Sx1278 unhealthy;
    CHECK(!unhealthy.set_sync_word(0xA5));
}

void test_reconfigure_reapplies_every_setting() {
    FakeRadio radio;
    cansat::Sx1278 driver;
    CHECK(driver.begin(make_hal(radio), cansat::Sx1278Settings{}));
    driver.start_receive();

    cansat::Sx1278Settings faster;
    faster.spreading_factor = 9;
    faster.bandwidth_hz = 250000;
    CHECK(driver.reconfigure(faster));
    CHECK((radio.reg[0x1E] >> 4) == 9);
    CHECK((radio.reg[0x1D] >> 4) == 8);  // 250 kHz is index 8

    // Reconfiguring leaves receive mode, so the caller must restart it deliberately.
    std::uint8_t out[8] = {};
    radio.reg[0x12] = 0x40;
    radio.reg[0x13] = 1;
    CHECK(driver.poll_receive(out, sizeof(out)) == 0);
}

}  // namespace

// probe_version() must go to the bus, not return a cached value.
//
// It exists so Gate 7 can ask the radio "are you still readable?" while the microSD is
// active on the same SPI0. A cached answer would always say yes and the check would be
// worthless -- the failure it guards against is precisely the bus going wrong after
// begin() succeeded.
void test_probe_version_reads_the_bus_rather_than_a_cached_value() {
    FakeRadio radio;
    cansat::Sx1278 sx;
    CHECK(sx.begin(make_hal(radio), cansat::Sx1278Settings{}));
    CHECK(sx.chip_version() == 0x12);

    // Corrupt the register behind the driver's back, exactly as a card holding MISO would.
    radio.reg[0x42] = 0x00;
    CHECK(sx.probe_version() == 0x00);
    CHECK(sx.chip_version() == 0x00);  // and the cached value follows the bus

    radio.reg[0x42] = 0x12;
    CHECK(sx.probe_version() == 0x12);
}

// A transmit that reports done faster than its own airtime is refused.
//
// This is not a hypothetical. On the bench a DIO0 line that had come loose floated high,
// and the driver read "done" the instant it polled: a 255-byte packet whose airtime is
// 399.6 ms returned success in 6.5 ms, and a 15-second burst claimed 2804 packets at 187
// per second. Every one reported sent. None were transmitted.
//
// A timeout is loud - the caller sees false. This was silent, and a vehicle would fly the
// whole mission reporting a healthy radio and an empty sky. The airtime model is the only
// thing that can catch it, and it is trustworthy enough to: pinned against published
// reference vectors, and measured against this radio to within 1.8 %.
void test_a_transmit_that_finishes_faster_than_its_airtime_is_refused() {
    FakeRadio radio;
    radio.dio0 = true;   // the loose wire, floating high: "done" the instant it is polled
    cansat::Sx1278 sx;
    CHECK(sx.begin(make_hal(radio), cansat::Sx1278Settings{}));

    const std::uint8_t payload[206] = {};
    CHECK(!sx.transmit(payload, sizeof(payload)));
    CHECK(sx.tx_impossibly_fast() == 1);
    CHECK(sx.tx_timeouts() == 0);   // it did not time out; it returned far too early
}

// And the guard must not reject an honest transmission. A radio whose DIO0 asserts after a
// realistic delay still succeeds, or the fix would ground the vehicle it was meant to save.
void test_a_normal_transmit_is_still_accepted() {
    FakeRadio radio;
    radio.dio0_delay_ms = 330;   // a 206-byte packet's real airtime at SF7/125 kHz
    cansat::Sx1278 sx;
    CHECK(sx.begin(make_hal(radio), cansat::Sx1278Settings{}));
    const std::uint8_t payload[206] = {};
    CHECK(sx.transmit(payload, sizeof(payload)));
    CHECK(sx.tx_impossibly_fast() == 0);
    CHECK(sx.tx_timeouts() == 0);
}

// "The transmit failed" is one sentence for three faults that want opposite
// investigations, and the registers read at the moment of the failure are what separate
// them. This pins the healthy-bus, still-trying case: the chip is the one we configured,
// still in LoRa TX, and simply never finished. That points at the PLL, the PA or the rail.
void test_a_timeout_captures_the_registers_that_name_the_fault() {
    FakeRadio radio;   // DIO0 never asserts
    cansat::Sx1278 sx;
    CHECK(sx.begin(make_hal(radio), cansat::Sx1278Settings{}));

    const std::uint8_t payload[206] = {};
    CHECK(!sx.transmit(payload, sizeof(payload), 500));
    CHECK(sx.tx_timeouts() == 1);
    CHECK(sx.last_tx_irq_flags() == 0x00);  // TxDone never set: it did not finish
    CHECK(sx.last_tx_op_mode() == 0x83);    // LoRa TX: the chip accepted the job
    CHECK(sx.last_tx_version() == 0x12);    // and SPI was working throughout
}

// The second of the three: the module browned out mid-transmit and came back reset. The
// IRQ flags look identical to the case above - nothing set, nothing finished - and only
// RegOpMode says the part is no longer in the mode we put it in. Without this the evening
// goes into the RF side of a fault that is actually a power connection.
void test_a_module_that_resets_mid_transmit_is_visible_in_op_mode() {
    FakeRadio radio;
    radio.brownout_after_ms = 200;
    cansat::Sx1278 sx;
    CHECK(sx.begin(make_hal(radio), cansat::Sx1278Settings{}));

    const std::uint8_t payload[206] = {};
    CHECK(!sx.transmit(payload, sizeof(payload), 500));
    CHECK(sx.tx_timeouts() == 1);
    CHECK((sx.last_tx_op_mode() & 0x80) == 0);  // no longer in LoRa mode: it was reset
    CHECK(sx.last_tx_version() == 0x12);        // the bus itself is fine
}

// The third: SPI to the radio was broken at that instant, so nothing about the transmit is
// implicated at all. On this vehicle the microSD shares the bus, and a card that keeps
// driving MISO corrupts the radio rather than itself - which presents as a dead radio.
void test_a_broken_bus_at_the_failure_shows_in_the_version() {
    FakeRadio radio;
    cansat::Sx1278 sx;
    CHECK(sx.begin(make_hal(radio), cansat::Sx1278Settings{}));
    radio.reg[0x42] = 0x00;  // MISO stuck low from the moment the transmit starts

    const std::uint8_t payload[206] = {};
    CHECK(!sx.transmit(payload, sizeof(payload), 500));
    CHECK(sx.tx_timeouts() == 1);
    CHECK(sx.last_tx_version() == 0x00);
}

int main() {
    test_begin_requires_the_right_silicon();
    test_begin_requires_a_complete_hal();
    test_lora_mode_is_entered_from_sleep();
    test_project_settings_reach_the_registers();
    test_high_power_uses_pa_dac();
    test_out_of_range_settings_are_clamped_not_wrapped();
    test_low_data_rate_optimisation_follows_the_symbol_time();
    test_sf6_gets_its_special_detection_settings();
    test_transmit_loads_the_fifo_and_completes_on_dio0();
    test_transmit_times_out_instead_of_hanging();
    test_transmit_is_bounded_without_a_clock_too();
    test_transmit_rejects_nonsense_and_an_unhealthy_radio();
    test_receive_returns_a_payload_and_drops_a_crc_failure();
    test_a_payload_longer_than_the_buffer_is_cut_and_counted();
    test_receive_needs_receive_mode_and_a_buffer();
    test_rssi_uses_the_low_frequency_offset_at_433_mhz();
    test_sync_word_can_be_switched_for_the_official_launch();
    test_reconfigure_reapplies_every_setting();

    test_probe_version_reads_the_bus_rather_than_a_cached_value();
    test_a_transmit_that_finishes_faster_than_its_airtime_is_refused();
    test_a_normal_transmit_is_still_accepted();
    test_a_timeout_captures_the_registers_that_name_the_fault();
    test_a_module_that_resets_mid_transmit_is_visible_in_op_mode();
    test_a_broken_bus_at_the_failure_shows_in_the_version();

    std::cout << (g_checks - g_failures) << "/" << g_checks << " checks passed\n";
    if (g_failures != 0) {
        std::cerr << g_failures << " FAILURE(S)\n";
        return 1;
    }
    std::cout << "sx1278 tests passed\n";
    return 0;
}
