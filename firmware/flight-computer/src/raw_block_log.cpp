#include "flight/raw_block_log.hpp"

#include <cstring>

namespace flight {

namespace {

void put_u16(std::uint8_t* p, std::uint16_t v) {
    p[0] = static_cast<std::uint8_t>(v);
    p[1] = static_cast<std::uint8_t>(v >> 8);
}
void put_u32(std::uint8_t* p, std::uint32_t v) {
    p[0] = static_cast<std::uint8_t>(v);
    p[1] = static_cast<std::uint8_t>(v >> 8);
    p[2] = static_cast<std::uint8_t>(v >> 16);
    p[3] = static_cast<std::uint8_t>(v >> 24);
}
std::uint16_t get_u16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}
std::uint32_t get_u32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

// Header layout, all little-endian:
//   0  magic          4
//   4  version        2
//   6  block size     2
//   8  next free lba  4
//  12  boot count     4
//  16  block count    4
//  20  sequence       4
//  24  checksum       4   sum of bytes 0..23, so a torn write is detectable
constexpr std::size_t kOffMagic = 0;
constexpr std::size_t kOffVersion = 4;
constexpr std::size_t kOffBlockSize = 6;
constexpr std::size_t kOffNextLba = 8;
constexpr std::size_t kOffBootCount = 12;
constexpr std::size_t kOffBlockCount = 16;
constexpr std::size_t kOffSequence = 20;
constexpr std::size_t kOffChecksum = 24;
constexpr std::size_t kChecksummedBytes = 24;

std::uint32_t header_checksum(const std::uint8_t* block) {
    std::uint32_t sum = 0;
    for (std::size_t i = 0; i < kChecksummedBytes; ++i) {
        sum += block[i];
    }
    return sum;
}

}  // namespace

bool RawBlockLog::reset() {
    if (!healthy_) return false;
    next_lba_ = base_lba_ + kHeaderBlocks;
    truncated_records_ = 0;
    // Arm the scrub over everything the log could have written. The header is rewritten
    // below, so the log is empty from this instant; the scrub only removes bytes that are
    // already unreachable.
    scrub_lba_ = base_lba_ + block_count_;
    // The header sequence keeps climbing -- write_header() increments it -- so the reader's
    // "take the copy with the higher sequence" rule still picks the newest, and an erase
    // cannot be mistaken for a torn write that should be ignored.
    return write_header();
}

bool RawBlockLog::scrub_step(std::uint32_t max_blocks) {
    if (!scrubbing()) return false;
    // Spaces, not zeros. These blocks live inside a .csv that opens in a spreadsheet, and
    // a wall of NULs makes some editors treat the file as binary and truncate it -- the
    // same reason write_header() pads with spaces.
    std::uint8_t block[kBlockSize];
    std::memset(block, ' ', sizeof(block));
    block[kBlockSize - 1] = '\n';

    for (std::uint32_t done = 0; done < max_blocks && scrubbing(); ++done) {
        const std::uint32_t lba = scrub_lba_ - 1;
        if (!io_.write_block(io_.ctx, lba, block)) {
            // A card that has stopped answering is not a reason to keep hammering it. The
            // log itself is already reset and consistent; abandoning the scrub loses
            // nothing a reader could have seen.
            scrub_lba_ = next_lba_;
            return false;
        }
        scrub_lba_ = lba;
    }
    return scrubbing();
}

bool RawBlockLog::write_header() {
    std::uint8_t block[kBlockSize];
    // Space-pad and newline-terminate the unused tail. The log now lives inside a
    // pre-allocated .csv on a FAT32 card so it can be read on a PC, and these two header
    // blocks are the first two lines of that file. Padding with spaces rather than NULs
    // makes them one printable line each instead of a wall of NULs that some editors
    // truncate the file at. The checksum covers only bytes 0..23, so the padding is free.
    std::memset(block, ' ', sizeof(block));
    block[kBlockSize - 1] = '\n';
    std::memset(block, 0, kOffChecksum + 4);
    put_u32(block + kOffMagic, kMagic);
    put_u16(block + kOffVersion, kVersion);
    put_u16(block + kOffBlockSize, static_cast<std::uint16_t>(kBlockSize));
    put_u32(block + kOffNextLba, next_lba_);
    put_u32(block + kOffBootCount, boot_count_);
    put_u32(block + kOffBlockCount, block_count_);
    put_u32(block + kOffSequence, ++header_sequence_);
    put_u32(block + kOffChecksum, header_checksum(block));

    // Alternate between the two copies. A power failure can corrupt only the copy being
    // written; the other still carries the previous, complete resume point.
    const std::uint32_t target = base_lba_ + (header_sequence_ % kHeaderBlocks);
    return io_.write_block(io_.ctx, target, block);
}

bool RawBlockLog::read_header(std::uint32_t lba, std::uint32_t& sequence,
                              std::uint32_t& next_lba, std::uint32_t& boot_count) const {
    std::uint8_t block[kBlockSize];
    if (!io_.read_block(io_.ctx, lba, block)) {
        return false;
    }
    if (get_u32(block + kOffMagic) != kMagic || get_u16(block + kOffVersion) != kVersion) {
        return false;
    }
    if (get_u16(block + kOffBlockSize) != static_cast<std::uint16_t>(kBlockSize)) {
        return false;
    }
    if (get_u32(block + kOffChecksum) != header_checksum(block)) {
        return false;  // torn write: this copy was interrupted
    }
    sequence = get_u32(block + kOffSequence);
    next_lba = get_u32(block + kOffNextLba);
    boot_count = get_u32(block + kOffBootCount);
    return true;
}

bool RawBlockLog::begin(const Io& io, std::uint32_t base_lba, std::uint32_t block_count) {
    io_ = io;
    base_lba_ = base_lba;
    block_count_ = block_count;
    healthy_ = false;
    header_sequence_ = 0;
    truncated_records_ = 0;
    if (!io_.read_block || !io_.write_block || block_count < kHeaderBlocks + 1) {
        return false;
    }

    // Take whichever header copy is valid; if both are, take the newer one.
    const std::uint32_t first_record = base_lba_ + kHeaderBlocks;
    bool resumed = false;
    std::uint32_t best_sequence = 0;
    for (std::uint32_t i = 0; i < kHeaderBlocks; ++i) {
        std::uint32_t sequence = 0;
        std::uint32_t stored_next = 0;
        std::uint32_t stored_boots = 0;
        if (!read_header(base_lba_ + i, sequence, stored_next, stored_boots)) {
            continue;
        }
        if (resumed && sequence <= best_sequence) {
            continue;
        }
        best_sequence = sequence;
        boot_count_ = stored_boots + 1;
        next_lba_ = (stored_next >= first_record && stored_next <= base_lba_ + block_count_)
                        ? stored_next
                        : first_record;
        resumed = true;
    }

    if (!resumed) {
        boot_count_ = 1;
        next_lba_ = first_record;
    }
    header_sequence_ = best_sequence;

    healthy_ = write_header();
    return healthy_;
}

bool RawBlockLog::append_line(const char* text, std::size_t len) {
    if (!healthy_ || text == nullptr) {
        return false;
    }
    if (next_lba_ >= base_lba_ + block_count_) {
        return false;  // region full: stop rather than overwrite flight data
    }

    std::uint8_t block[kBlockSize];
    std::memset(block, ' ', sizeof(block));
    std::size_t n = len;
    if (n > kMaxRecordBytes) {
        n = kMaxRecordBytes;
        ++truncated_records_;
    }
    std::memcpy(block, text, n);
    block[n] = '\n';

    if (!io_.write_block(io_.ctx, next_lba_, block)) {
        return false;
    }
    ++next_lba_;
    return write_header();
}

}  // namespace flight
