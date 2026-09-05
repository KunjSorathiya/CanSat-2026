// Host tests for the microSD SPI driver, against a simulated card.
//
// The breakout received is a 2.6-3.6 V SPI module, so it runs from the vehicle's 3.3 V
// rail with no level shifting and no rail of its own. What is left is the part this file
// covers: the driver shares SPI0 with the radio, and its command sequence has to be right
// the first time it runs on hardware. The driver reaches hardware through a callback
// struct, so the whole sequence runs here against a card model built from the SD Physical
// Layer Simplified Specification.
//
// This proves the driver issues the right commands, in the right order, with the right
// addressing, and gives up on every failure path. It proves nothing about the physical
// module: current draw, decoupling and MISO release on the shared bus are bench
// measurements, not test assertions.

#include "flight/pico/sd_card.hpp"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
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

constexpr std::size_t kBlock = flight::pico::SdCard::kBlockSize;

// A simulated SD card in SPI mode. Consumes command frames byte by byte and produces the
// R1/R3/R7 responses, data tokens and busy periods a real card would.
class FakeCard {
public:
    enum class Kind { sdhc_v2, sdsc_v2, sdsc_v1, dead, refuses_init };

    explicit FakeCard(Kind kind = Kind::sdhc_v2) : kind_(kind) {}

    // Observability.
    std::vector<std::uint8_t> commands;          // command indices in order
    std::vector<std::uint32_t> command_args;     // and their arguments
    std::map<std::uint32_t, std::vector<std::uint8_t>> blocks;
    int cs_high_clocks_before_first_command = 0;
    int deselect_count = 0;
    bool clocked_after_last_deselect = false;
    std::uint32_t baudrate = 0;
    std::uint32_t max_init_baudrate = 0;
    bool selected = false;
    std::uint32_t millis = 0;
    bool fail_write_response = false;
    bool fail_status = false;
    bool never_sends_data_token = false;
    // A card that has stopped answering: MISO idles high and every byte reads 0xFF. This is
    // what a supply that let go looks like from the driver's side, and it is not the same as
    // a card that answers and refuses.
    bool dead = false;

    void set_select(bool on) {
        if (!on && selected) {
            ++deselect_count;
            clocked_after_last_deselect = false;
        }
        selected = on;
        state_ = State::idle;
    }

    std::uint8_t transfer(std::uint8_t value) {
        if (!selected) {
            if (commands.empty()) ++cs_high_clocks_before_first_command;
            clocked_after_last_deselect = true;
            return 0xFF;
        }
        return exchange(value);
    }

    void set_baudrate(std::uint32_t hz) {
        baudrate = hz;
        if (commands.empty() || !init_done_) {
            if (hz > max_init_baudrate) max_init_baudrate = hz;
        }
    }

private:
    enum class State { idle, args, response, read_data, write_wait_token, write_data,
                       write_response, busy };

    std::uint8_t exchange(std::uint8_t value) {
        if (dead) return 0xFF;
        switch (state_) {
            case State::idle:
                if ((value & 0xC0) == 0x40) {  // command frame: 01xxxxxx
                    current_cmd_ = static_cast<std::uint8_t>(value & 0x3F);
                    arg_ = 0;
                    arg_bytes_ = 0;
                    state_ = State::args;
                }
                return 0xFF;

            case State::args:
                if (arg_bytes_ < 4) {
                    arg_ = (arg_ << 8) | value;
                    ++arg_bytes_;
                    return 0xFF;
                }
                // The CRC byte closes the frame; the response follows.
                begin_response();
                state_ = State::response;
                return 0xFF;

            case State::response:
                if (!response_.empty()) {
                    const std::uint8_t byte = response_.front();
                    response_.erase(response_.begin());
                    if (response_.empty()) state_ = next_after_response_;
                    return byte;
                }
                state_ = next_after_response_;
                return 0xFF;

            case State::read_data:
                if (read_pos_ < read_payload_.size()) {
                    return read_payload_[read_pos_++];
                }
                state_ = State::idle;
                return 0xFF;

            case State::write_wait_token:
                if (value == 0xFE) {
                    state_ = State::write_data;
                    write_buffer_.clear();
                }
                return 0xFF;

            case State::write_data:
                write_buffer_.push_back(value);
                if (write_buffer_.size() == kBlock + 2) {  // block + 2 CRC bytes
                    write_buffer_.resize(kBlock);
                    state_ = State::write_response;
                }
                return 0xFF;

            case State::write_response:
                // A real card answers with the data-response token on the byte *after*
                // the CRC, then holds the line low while it programs the block.
                state_ = State::busy;
                busy_bytes_ = 2;
                if (fail_write_response) {
                    return 0x0D;  // CRC error: the block is not stored
                }
                blocks[pending_lba_] = write_buffer_;
                return 0x05;  // data accepted

            case State::busy:
                if (busy_bytes_ > 0) {
                    --busy_bytes_;
                    return 0x00;  // card is programming
                }
                state_ = State::idle;
                return 0xFF;
        }
        return 0xFF;
    }

    void begin_response() {
        commands.push_back(current_cmd_);
        command_args.push_back(arg_);
        response_.clear();
        next_after_response_ = State::idle;

        if (kind_ == Kind::dead) {
            response_ = {0xFF};  // nothing ever answers
            return;
        }

        switch (current_cmd_) {
            case 0:  // GO_IDLE_STATE
                idle_ = true;
                // A real card latches R1's error bits onto whatever it made of the noise
                // on an undriven bus while the rail was coming up, and holds them until a
                // response is read. Modelled here: the first CMD0 answers idle with those
                // bits still set, and reading it clears them.
                if (sticky_first_r1_ != 0) {
                    response_ = {sticky_first_r1_};
                    sticky_first_r1_ = 0;
                } else {
                    response_ = {0x01};
                }
                break;
            case 8:  // SEND_IF_COND
                if (kind_ == Kind::sdsc_v1) {
                    response_ = {0x05};  // illegal command: a v1 card
                } else {
                    response_ = {0x01, 0x00, 0x00, 0x01, 0xAA};  // R7 echo-back
                }
                break;
            case 55:  // APP_CMD
                response_ = {idle_ ? std::uint8_t{0x01} : std::uint8_t{0x00}};
                break;
            case 41:  // ACMD41
                if (kind_ == Kind::refuses_init) {
                    response_ = {0x01};  // stays busy forever
                } else if (++acmd41_count_ >= 3) {
                    idle_ = false;
                    init_done_ = true;
                    response_ = {0x00};
                } else {
                    response_ = {0x01};
                }
                break;
            case 58: {  // READ_OCR
                const std::uint8_t ccs = (kind_ == Kind::sdhc_v2) ? 0xC0 : 0x80;
                response_ = {0x00, ccs, 0xFF, 0x80, 0x00};
                break;
            }
            case 16:  // SET_BLOCKLEN
                response_ = {0x00};
                break;
            case 17: {  // READ_SINGLE_BLOCK
                response_ = {0x00};
                const std::uint32_t lba = block_index(arg_);
                read_payload_.clear();
                if (never_sends_data_token) {
                    next_after_response_ = State::idle;  // token never arrives
                    break;
                }
                read_payload_.push_back(0xFE);  // start-block token
                auto found = blocks.find(lba);
                if (found != blocks.end()) {
                    read_payload_.insert(read_payload_.end(), found->second.begin(),
                                         found->second.end());
                } else {
                    read_payload_.insert(read_payload_.end(), kBlock, 0x00);
                }
                read_payload_.push_back(0x00);  // CRC
                read_payload_.push_back(0x00);
                read_pos_ = 0;
                next_after_response_ = State::read_data;
                break;
            }
            case 24:  // WRITE_BLOCK
                response_ = {0x00};
                pending_lba_ = block_index(arg_);
                next_after_response_ = State::write_wait_token;
                break;
            case 13:  // SEND_STATUS (R2)
                response_ = {0x00, fail_status ? std::uint8_t{0x40} : std::uint8_t{0x00}};
                break;
            default:
                response_ = {0x00};
                break;
        }
    }

    std::uint32_t block_index(std::uint32_t arg) const {
        return kind_ == Kind::sdhc_v2 ? arg : arg / kBlock;
    }

    Kind kind_;
    // Set by a test to make the first CMD0 answer with sticky error bits set.
public:
    std::uint8_t sticky_first_r1_ = 0;
private:
    State state_ = State::idle;
    State next_after_response_ = State::idle;
    std::uint8_t current_cmd_ = 0;
    std::uint32_t arg_ = 0;
    int arg_bytes_ = 0;
    std::vector<std::uint8_t> response_;
    std::vector<std::uint8_t> read_payload_;
    std::size_t read_pos_ = 0;
    std::vector<std::uint8_t> write_buffer_;
    std::uint32_t pending_lba_ = 0;
    int busy_bytes_ = 0;
    int acmd41_count_ = 0;
    bool idle_ = true;
    bool init_done_ = false;
};

// ---- HAL glue ---------------------------------------------------------------------
void hal_select(void* ctx, bool on) { static_cast<FakeCard*>(ctx)->set_select(on); }
std::uint8_t hal_transfer(void* ctx, std::uint8_t value) {
    return static_cast<FakeCard*>(ctx)->transfer(value);
}
void hal_baud(void* ctx, std::uint32_t hz) { static_cast<FakeCard*>(ctx)->set_baudrate(hz); }
void hal_delay(void* ctx, std::uint32_t ms) { static_cast<FakeCard*>(ctx)->millis += ms; }
std::uint32_t hal_millis(void* ctx) { return static_cast<FakeCard*>(ctx)->millis; }

flight::pico::SdCardHal make_hal(FakeCard& card) {
    flight::pico::SdCardHal hal;
    hal.ctx = &card;
    hal.select = hal_select;
    hal.transfer = hal_transfer;
    hal.set_baudrate = hal_baud;
    hal.delay_ms = hal_delay;
    hal.millis = hal_millis;
    return hal;
}

bool issued(const FakeCard& card, std::uint8_t cmd) {
    for (const std::uint8_t seen : card.commands) {
        if (seen == cmd) return true;
    }
    return false;
}

// ---- tests ------------------------------------------------------------------------

void test_initialisation_sequence_follows_the_specification() {
    FakeCard card;
    flight::pico::SdCard sd;
    CHECK(sd.begin_with(make_hal(card)));
    CHECK(sd.ok());
    CHECK(sd.high_capacity());

    // The card needs at least 74 clocks with CS high before anything else.
    CHECK(card.cs_high_clocks_before_first_command >= 10);

    // CMD0, CMD8, then CMD55/ACMD41 until ready, then CMD58 to read the OCR.
    CHECK(!card.commands.empty());
    CHECK(card.commands.front() == 0);
    CHECK(issued(card, 8));
    CHECK(issued(card, 55));
    CHECK(issued(card, 41));
    CHECK(issued(card, 58));

    // CMD8's argument carries the 2.7-3.6 V range and the 0xAA check pattern. That range
    // is the one this vehicle actually supplies: a 3.3 V rail into a 2.6-3.6 V module.
    // Asking for a range the hardware cannot supply would let a card claim compatibility
    // it does not have.
    for (std::size_t i = 0; i < card.commands.size(); ++i) {
        if (card.commands[i] == 8) CHECK(card.command_args[i] == 0x000001AA);
        // ACMD41 must set HCS for a v2 card, or high-capacity cards never initialise.
        if (card.commands[i] == 41) CHECK((card.command_args[i] & 0x40000000u) != 0);
    }
}

void test_initialisation_runs_slowly_then_speeds_up() {
    FakeCard card;
    flight::pico::SdCard sd;
    CHECK(sd.begin_with(make_hal(card)));
    // A card must be initialised at no more than 400 kHz.
    CHECK(card.max_init_baudrate <= flight::pico::SdCard::kInitBaud);
    // ...and the bus is raised for data transfer afterwards.
    CHECK(card.baudrate == flight::pico::SdCard::kRunBaud);
}

void test_a_standard_capacity_card_is_addressed_in_bytes() {
    // The classic SD bug: SDHC addresses blocks, SDSC addresses bytes. Getting it wrong
    // reads and writes the wrong place, 512 times too far in, and looks like corruption.
    FakeCard card(FakeCard::Kind::sdsc_v2);
    flight::pico::SdCard sd;
    CHECK(sd.begin_with(make_hal(card)));
    CHECK(sd.ok());
    CHECK(!sd.high_capacity());
    CHECK(issued(card, 16));  // SET_BLOCKLEN is required on standard-capacity cards

    std::uint8_t block[kBlock];
    std::memset(block, 0xA5, sizeof(block));
    CHECK(sd.write_block(7, block));
    for (std::size_t i = 0; i < card.commands.size(); ++i) {
        if (card.commands[i] == 24) CHECK(card.command_args[i] == 7u * kBlock);
    }

    std::uint8_t read_back[kBlock] = {};
    CHECK(sd.read_block(7, read_back));
    CHECK(std::memcmp(block, read_back, kBlock) == 0);
}

void test_a_high_capacity_card_is_addressed_in_blocks() {
    FakeCard card(FakeCard::Kind::sdhc_v2);
    flight::pico::SdCard sd;
    CHECK(sd.begin_with(make_hal(card)));
    CHECK(sd.high_capacity());
    CHECK(!issued(card, 16));  // SET_BLOCKLEN is meaningless on SDHC

    std::uint8_t block[kBlock];
    std::memset(block, 0x5A, sizeof(block));
    CHECK(sd.write_block(9, block));
    for (std::size_t i = 0; i < card.commands.size(); ++i) {
        if (card.commands[i] == 24) CHECK(card.command_args[i] == 9u);
    }
}

void test_a_block_round_trips() {
    FakeCard card;
    flight::pico::SdCard sd;
    CHECK(sd.begin_with(make_hal(card)));

    std::uint8_t written[kBlock];
    for (std::size_t i = 0; i < kBlock; ++i) {
        written[i] = static_cast<std::uint8_t>(i * 7 + 3);
    }
    CHECK(sd.write_block(1234, written));

    std::uint8_t read_back[kBlock] = {};
    CHECK(sd.read_block(1234, read_back));
    CHECK(std::memcmp(written, read_back, kBlock) == 0);

    // An untouched block reads as zeros rather than failing.
    std::uint8_t empty[kBlock];
    std::memset(empty, 0xFF, sizeof(empty));
    CHECK(sd.read_block(4321, empty));
    for (std::size_t i = 0; i < kBlock; ++i) CHECK(empty[i] == 0x00);
}

void test_the_bus_is_released_after_every_transaction() {
    // SPI0 is shared with the radio. A card left driving MISO corrupts the radio's next
    // transaction, and the symptom is a dead radio, not a dead card.
    FakeCard card;
    flight::pico::SdCard sd;
    CHECK(sd.begin_with(make_hal(card)));
    CHECK(card.clocked_after_last_deselect);

    std::uint8_t block[kBlock] = {};
    CHECK(sd.write_block(2, block));
    CHECK(card.clocked_after_last_deselect);

    CHECK(sd.read_block(2, block));
    CHECK(card.clocked_after_last_deselect);
}

void test_a_dead_card_fails_instead_of_hanging() {
    FakeCard card(FakeCard::Kind::dead);
    flight::pico::SdCard sd;
    CHECK(!sd.begin_with(make_hal(card)));
    CHECK(!sd.ok());
    CHECK(card.clocked_after_last_deselect);  // still releases the shared bus
}

void test_a_card_that_never_finishes_initialising_times_out() {
    FakeCard card(FakeCard::Kind::refuses_init);
    flight::pico::SdCard sd;
    CHECK(!sd.begin_with(make_hal(card)));
    CHECK(!sd.ok());
    // It gave up on the clock, not after one attempt: the loop ran its full allowance.
    CHECK(card.millis >= 2000);
}

void test_operations_are_refused_before_initialisation() {
    flight::pico::SdCard sd;
    std::uint8_t block[kBlock] = {};
    CHECK(!sd.read_block(0, block));
    CHECK(!sd.write_block(0, block));
}

void test_null_buffers_are_refused() {
    FakeCard card;
    flight::pico::SdCard sd;
    CHECK(sd.begin_with(make_hal(card)));
    CHECK(!sd.read_block(0, nullptr));
    CHECK(!sd.write_block(0, nullptr));
}

void test_an_incomplete_hal_is_refused() {
    FakeCard card;
    flight::pico::SdCard sd;
    flight::pico::SdCardHal hal = make_hal(card);
    hal.transfer = nullptr;
    CHECK(!sd.begin_with(hal));
}

void test_a_rejected_write_is_reported() {
    FakeCard card;
    card.fail_write_response = true;  // card answers with a CRC-error token
    flight::pico::SdCard sd;
    CHECK(sd.begin_with(make_hal(card)));
    std::uint8_t block[kBlock] = {};
    CHECK(!sd.write_block(3, block));
    CHECK(card.clocked_after_last_deselect);
}

void test_a_write_error_reported_by_cmd13_is_not_treated_as_success() {
    FakeCard card;
    card.fail_status = true;  // data accepted, but the status register flags an error
    flight::pico::SdCard sd;
    CHECK(sd.begin_with(make_hal(card)));
    std::uint8_t block[kBlock] = {};
    CHECK(!sd.write_block(4, block));
}

// A write that stops early never reaches the later fields, and whatever the previous write
// left in them is not a measurement of this one. On the bench a write that was refused at
// CMD24 printed "data token = 0xE5" - a byte from an attempt that had nothing to do with it -
// alongside an R1 of 0xFF, and the pair read as a card in a specific and diagnosable state
// rather than a card that had gone silent.
void test_a_failed_write_does_not_report_the_previous_write_bytes() {
    FakeCard card;
    flight::pico::SdCard sd;
    CHECK(sd.begin_with(make_hal(card)));

    std::uint8_t block[kBlock] = {};
    CHECK(sd.write_block(7, block));
    CHECK(sd.last_data_response() == 0x05);   // accepted, and recorded

    card.dead = true;                          // the card stops answering entirely
    CHECK(!sd.write_block(8, block));
    CHECK(sd.write_stage() == flight::pico::SdCard::WriteStage::cmd24_rejected);
    CHECK(sd.last_write_r1() == 0xFF);         // 0xFF is no answer, not a status byte
    CHECK(sd.last_data_response() == 0x00);    // NOT the 0x05 from the write before
    CHECK(sd.last_write_r2() == 0x00);         // CMD13 was never sent
}

void test_a_missing_data_token_fails_the_read() {
    FakeCard card;
    flight::pico::SdCard sd;
    CHECK(sd.begin_with(make_hal(card)));
    card.never_sends_data_token = true;
    std::uint8_t block[kBlock] = {};
    CHECK(!sd.read_block(5, block));
    CHECK(card.clocked_after_last_deselect);
}

void test_a_v1_card_still_initialises() {
    // CMD8 is illegal on a v1 card. That is not a failure: it identifies the card as
    // standard capacity, and ACMD41 must then be sent without the HCS bit.
    FakeCard card(FakeCard::Kind::sdsc_v1);
    flight::pico::SdCard sd;
    CHECK(sd.begin_with(make_hal(card)));
    CHECK(sd.ok());
    CHECK(!sd.high_capacity());
    for (std::size_t i = 0; i < card.commands.size(); ++i) {
        if (card.commands[i] == 41) CHECK((card.command_args[i] & 0x40000000u) == 0);
    }
    CHECK(!issued(card, 58));  // no OCR read on a v1 card
    CHECK(issued(card, 16));
}

}  // namespace

// A card whose first CMD0 answers with sticky error bits set must still initialise.
//
// This is not hypothetical. On the bench a 64 GB SDXC card answered its first CMD0 with
// 0x1F: idle, plus illegal-command, CRC-error, erase-sequence and erase-reset. All four are
// sticky bits latched from bus noise while the rail came up, and all four clear when the
// response is read -- so the second CMD0 is clean. The driver tried once, saw something that
// was not exactly 0x01, and reported a dead card. A perfectly healthy card was rejected.
void test_a_card_whose_first_cmd0_carries_stale_error_bits_still_initialises() {
    for (const std::uint8_t dirty : {0x1Fu, 0x05u, 0x0Du, 0x09u}) {
        FakeCard card(FakeCard::Kind::sdhc_v2);
        card.sticky_first_r1_ = dirty;
        flight::pico::SdCard sd;
        CHECK(sd.begin_with(make_hal(card)));
        CHECK(sd.ok());
        CHECK(sd.high_capacity());
        // It took more than one attempt, and the driver says so rather than hiding it.
        CHECK(sd.cmd0_attempts() >= 2);
        CHECK(sd.stage() == flight::pico::SdCard::Stage::complete);
    }
}

// The retry must not paper over a card that never answers: an undriven bus reads 0xFF, bit 7
// set, which is not an R1 at all. That has to stay a failure, and it has to stop at CMD0.
void test_a_bus_that_never_answers_still_fails_at_cmd0() {
    FakeCard card(FakeCard::Kind::dead);
    flight::pico::SdCard sd;
    CHECK(!sd.begin_with(make_hal(card)));
    CHECK(!sd.ok());
    CHECK(sd.stage() == flight::pico::SdCard::Stage::cmd0_idle);
    CHECK(sd.last_r1() == 0xFF);
}

int main() {
    test_initialisation_sequence_follows_the_specification();
    test_initialisation_runs_slowly_then_speeds_up();
    test_a_standard_capacity_card_is_addressed_in_bytes();
    test_a_high_capacity_card_is_addressed_in_blocks();
    test_a_card_whose_first_cmd0_carries_stale_error_bits_still_initialises();
    test_a_bus_that_never_answers_still_fails_at_cmd0();
    test_a_block_round_trips();
    test_the_bus_is_released_after_every_transaction();
    test_a_dead_card_fails_instead_of_hanging();
    test_a_card_that_never_finishes_initialising_times_out();
    test_operations_are_refused_before_initialisation();
    test_null_buffers_are_refused();
    test_an_incomplete_hal_is_refused();
    test_a_rejected_write_is_reported();
    test_a_write_error_reported_by_cmd13_is_not_treated_as_success();
    test_a_failed_write_does_not_report_the_previous_write_bytes();
    test_a_missing_data_token_fails_the_read();
    test_a_v1_card_still_initialises();

    std::cout << (g_checks - g_failures) << "/" << g_checks << " checks passed\n";
    if (g_failures != 0) {
        std::cerr << g_failures << " FAILURE(S)\n";
        return 1;
    }
    std::cout << "sd_card tests passed\n";
    return 0;
}
