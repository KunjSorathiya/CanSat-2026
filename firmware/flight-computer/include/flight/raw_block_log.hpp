#pragma once

#include <cstddef>
#include <cstdint>

namespace flight {

// Append-only text log over a linear array of 512-byte blocks, with no filesystem. Used
// for the onboard microSD log so the flight code carries no FAT dependency.
//
// Region layout, starting at base_lba:
//   block[0], block[1]  two alternating header copies
//   block[2 .. N-1]     one newline-terminated record per block, space padded
//
// A header is rewritten after every record so a brownout or impact reset resumes at the
// correct block instead of overwriting flight data. The two copies alternate, each
// carrying a sequence number: power can only ever interrupt one of them, so the other is
// always intact and the resume point is never lost. With a single header, a power failure
// during that one write would leave no valid header at all — and the next boot would
// restart at block 1 and overwrite the entire flight it had just recorded.
class RawBlockLog {
public:
    static constexpr std::size_t kBlockSize = 512;
    static constexpr std::uint32_t kMagic = 0x54415343;  // 'CSAT' little-endian
    static constexpr std::uint16_t kVersion = 2;         // v2: dual alternating headers
    static constexpr std::uint32_t kHeaderBlocks = 2;
    static constexpr std::size_t kMaxRecordBytes = kBlockSize - 1;

    struct Io {
        void* ctx = nullptr;
        bool (*read_block)(void* ctx, std::uint32_t lba, std::uint8_t* out512) = nullptr;
        bool (*write_block)(void* ctx, std::uint32_t lba, const std::uint8_t* in512) = nullptr;
    };

    bool begin(const Io& io, std::uint32_t base_lba, std::uint32_t block_count);

    // Append one record. Text longer than 511 bytes is truncated. Returns false when the
    // region is full or a block write fails.
    bool append_line(const char* text, std::size_t len);

    // Empties the log: the next record goes back to the first data block and the file
    // reads as containing nothing.
    //
    // **It does not overwrite the old blocks, and it does not need to.** The reader takes
    // the header's `next free` and stops there, which is the same mechanism that makes a
    // 64 MB pre-allocated file readable at all -- everything past the last record has
    // always been whatever was in those blocks before. Zeroing 131,072 blocks to achieve
    // the same visible result would take minutes and buy nothing.
    //
    // The boot count is kept. It is the vehicle's life story rather than the file's, and
    // resetting it would make a card that has been erased indistinguishable from a card
    // that has never been flown.
    bool reset();

    // Overwrites at most `max_blocks` of the not-yet-reused region with spaces, the way
    // `tools/prepare_sd_card.py` leaves a freshly created file. Returns true while more
    // remains. Started by reset(); a no-op once finished.
    //
    // **It scrubs downward, from the end of the region toward the write pointer.** Records
    // append upward, so the two ends move apart and can never land on the same block --
    // which is what lets the log stay open and the vehicle keep logging while a scrub that
    // takes minutes runs underneath it. The card manages 297-367 blocks/s, so 64 MB is
    // about seven minutes of continuous writing at the very best; there is no version of
    // this that is quick, and blocking on it would cost the mandatory 1 Hz downlink and
    // then the watchdog.
    //
    // reset() has already made the log read as empty before the first block is scrubbed,
    // so power lost halfway through leaves a consistent empty log rather than a half-built
    // one. The scrub is cosmetic from that point on: it removes bytes a reader would never
    // have reached anyway.
    bool scrub_step(std::uint32_t max_blocks);
    bool scrubbing() const { return healthy_ && scrub_lba_ > next_lba_; }
    std::uint32_t scrub_blocks_remaining() const {
        return scrubbing() ? (scrub_lba_ - next_lba_) : 0;
    }

    bool healthy() const { return healthy_; }
    bool full() const { return healthy_ && next_lba_ >= base_lba_ + block_count_; }
    std::uint32_t record_count() const {
        const std::uint32_t first = base_lba_ + kHeaderBlocks;
        return (next_lba_ > first) ? (next_lba_ - first) : 0;
    }
    std::uint32_t boot_count() const { return boot_count_; }
    // Records that did not fit in one block and were cut. Never silent: the flight log is
    // evidence, and a shortened record should be visible as such.
    std::uint32_t truncated_records() const { return truncated_records_; }

private:
    bool write_header();
    bool read_header(std::uint32_t lba, std::uint32_t& sequence, std::uint32_t& next_lba,
                     std::uint32_t& boot_count) const;

    Io io_{};
    std::uint32_t base_lba_ = 0;
    std::uint32_t block_count_ = 0;
    std::uint32_t next_lba_ = 0;
    std::uint32_t scrub_lba_ = 0;   // exclusive; walks down toward next_lba_
    std::uint32_t boot_count_ = 0;
    std::uint32_t header_sequence_ = 0;
    std::uint32_t truncated_records_ = 0;
    bool healthy_ = false;
};

}  // namespace flight
