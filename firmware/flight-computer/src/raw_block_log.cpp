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

}  // namespace

bool RawBlockLog::write_header() {
    std::uint8_t block[kBlockSize];
    std::memset(block, 0, sizeof(block));
    put_u32(block + 0, kMagic);
    put_u16(block + 4, kVersion);
    put_u16(block + 6, static_cast<std::uint16_t>(kBlockSize));
    put_u32(block + 8, next_lba_);
    put_u32(block + 12, boot_count_);
    put_u32(block + 16, block_count_);
    return io_.write_block(io_.ctx, base_lba_, block);
}

bool RawBlockLog::begin(const Io& io, std::uint32_t base_lba, std::uint32_t block_count) {
    io_ = io;
    base_lba_ = base_lba;
    block_count_ = block_count;
    healthy_ = false;
    if (!io_.read_block || !io_.write_block || block_count < 2) {
        return false;
    }

    std::uint8_t block[kBlockSize];
    if (!io_.read_block(io_.ctx, base_lba_, block)) {
        return false;
    }

    if (get_u32(block + 0) == kMagic && get_u16(block + 4) == kVersion) {
        const std::uint32_t stored_next = get_u32(block + 8);
        boot_count_ = get_u32(block + 12) + 1;
        next_lba_ = (stored_next > base_lba_ && stored_next <= base_lba_ + block_count_)
                        ? stored_next
                        : base_lba_ + 1;
    } else {
        boot_count_ = 1;
        next_lba_ = base_lba_ + 1;
    }

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
    if (n > kBlockSize - 1) {
        n = kBlockSize - 1;
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
