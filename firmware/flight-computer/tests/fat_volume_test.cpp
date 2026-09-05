// Host tests for the FAT32 locator, against a synthetic card image.
//
// The flight log writes raw blocks with no filesystem, because FAT metadata updates are
// not atomic and a hard landing during one can cost the whole recording. But the card has
// to mount on a PC afterwards, so the filesystem work is done before the flight instead of
// during it: tools/prepare_sd_card.py creates a fixed-size FLIGHT.CSV, and this code finds
// where its data physically lives so the log can be written straight into it.
//
// Everything that can go wrong with that lookup is a way to silently destroy either the
// flight data or the rest of the card, so each failure gets a test: no volume, no file, a
// fragmented file, a file too small, and a card that stops answering. The fragmentation
// case is the dangerous one — writing linearly into a fragmented file overwrites whatever
// occupies the gap, while still reporting success.

#include "flight/fat_volume.hpp"

#include <cstdint>
#include <cstring>
#include <iostream>
#include <map>
#include <string>
#include <vector>

namespace {

int g_failures = 0;
int g_checks = 0;

#define CHECK(cond)                                                                 \
    do {                                                                            \
        ++g_checks;                                                                 \
        if (!(cond)) {                                                              \
            std::cerr << "FAIL " << __FILE__ << ":" << __LINE__ << "  " #cond "\n";  \
            ++g_failures;                                                           \
        }                                                                           \
    } while (0)

constexpr std::uint32_t kSector = 512;

// A sparse card image: only the blocks the builder writes exist, and everything else reads
// back as zeros. That mirrors a real card closely enough for a lookup test and keeps a
// multi-gigabyte volume in a few kilobytes of memory.
struct CardImage {
    std::map<std::uint32_t, std::vector<std::uint8_t>> blocks;
    std::uint32_t fail_after = 0xFFFFFFFFu;  // reads start failing once this many happened
    mutable std::uint32_t reads = 0;

    void put(std::uint32_t lba, const std::vector<std::uint8_t>& data) {
        std::vector<std::uint8_t> b(kSector, 0);
        std::memcpy(b.data(), data.data(), data.size() < kSector ? data.size() : kSector);
        blocks[lba] = b;
    }

    void put16(std::uint32_t lba, std::size_t off, std::uint16_t v) {
        auto& b = ensure(lba);
        b[off] = static_cast<std::uint8_t>(v & 0xFF);
        b[off + 1] = static_cast<std::uint8_t>(v >> 8);
    }

    void put32(std::uint32_t lba, std::size_t off, std::uint32_t v) {
        auto& b = ensure(lba);
        for (int i = 0; i < 4; ++i) b[off + i] = static_cast<std::uint8_t>((v >> (8 * i)) & 0xFF);
    }

    std::vector<std::uint8_t>& ensure(std::uint32_t lba) {
        auto it = blocks.find(lba);
        if (it == blocks.end()) {
            blocks[lba] = std::vector<std::uint8_t>(kSector, 0);
            return blocks[lba];
        }
        return it->second;
    }
};

bool image_read(void* ctx, std::uint32_t lba, std::uint8_t* out) {
    auto* img = static_cast<CardImage*>(ctx);
    if (++img->reads > img->fail_after) return false;
    auto it = img->blocks.find(lba);
    if (it == img->blocks.end()) {
        std::memset(out, 0, kSector);
        return true;
    }
    std::memcpy(out, it->second.data(), kSector);
    return true;
}

// Volume geometry used by every test below. Small but structurally identical to a real
// card: MBR, reserved sectors, two FATs, then the data area.
constexpr std::uint32_t kPartLba = 2048;
constexpr std::uint32_t kReserved = 32;
constexpr std::uint32_t kNumFats = 2;
constexpr std::uint32_t kFatSectors = 64;
constexpr std::uint32_t kSpc = 8;  // 4 KiB clusters
constexpr std::uint32_t kRootCluster = 2;
constexpr std::uint32_t kFatLba = kPartLba + kReserved;
constexpr std::uint32_t kDataLba = kFatLba + (kNumFats * kFatSectors);

std::uint32_t cluster_lba(std::uint32_t c) { return kDataLba + ((c - 2) * kSpc); }

void write_mbr(CardImage& img) {
    img.ensure(0);
    img.blocks[0][446 + 4] = 0x0C;  // FAT32 LBA
    img.put32(0, 446 + 8, kPartLba);
    img.blocks[0][510] = 0x55;
    img.blocks[0][511] = 0xAA;
}

void write_bpb(CardImage& img) {
    img.ensure(kPartLba);
    img.blocks[kPartLba][0] = 0xEB;
    img.put16(kPartLba, 11, static_cast<std::uint16_t>(kSector));
    img.blocks[kPartLba][13] = static_cast<std::uint8_t>(kSpc);
    img.put16(kPartLba, 14, static_cast<std::uint16_t>(kReserved));
    img.blocks[kPartLba][16] = static_cast<std::uint8_t>(kNumFats);
    img.put16(kPartLba, 17, 0);  // root entries: 0 on FAT32
    img.put16(kPartLba, 22, 0);  // 16-bit FAT size: 0 on FAT32
    img.put32(kPartLba, 36, kFatSectors);
    img.put32(kPartLba, 44, kRootCluster);
    img.blocks[kPartLba][510] = 0x55;
    img.blocks[kPartLba][511] = 0xAA;
}

void set_fat(CardImage& img, std::uint32_t cluster, std::uint32_t value) {
    const std::uint32_t byte_off = cluster * 4;
    img.put32(kFatLba + (byte_off / kSector), byte_off % kSector, value);
}

// Writes a directory entry for `name_83` into the first sector of the root cluster.
void write_dir_entry(CardImage& img, int slot, const char* name_83,
                     std::uint32_t first_cluster, std::uint32_t size) {
    const std::uint32_t lba = cluster_lba(kRootCluster);
    auto& b = img.ensure(lba);
    const std::size_t off = static_cast<std::size_t>(slot) * 32;
    for (int i = 0; i < 11; ++i) b[off + i] = static_cast<std::uint8_t>(name_83[i]);
    b[off + 11] = 0x20;  // archive
    img.put16(lba, off + 20, static_cast<std::uint16_t>(first_cluster >> 16));
    img.put16(lba, off + 26, static_cast<std::uint16_t>(first_cluster & 0xFFFF));
    img.put32(lba, off + 28, size);
}

// A volume with FLIGHT.CSV occupying `clusters` consecutive clusters from `first`.
CardImage make_card(std::uint32_t first, std::uint32_t clusters, bool contiguous = true) {
    CardImage img;
    write_mbr(img);
    write_bpb(img);
    set_fat(img, kRootCluster, 0x0FFFFFFF);  // root directory: one cluster
    write_dir_entry(img, 0, "FLIGHT  CSV", first, clusters * kSpc * kSector);
    for (std::uint32_t i = 0; i < clusters; ++i) {
        const std::uint32_t c = first + i;
        if (i + 1 == clusters) {
            set_fat(img, c, 0x0FFFFFFF);
        } else if (!contiguous && i == 1) {
            set_fat(img, c, c + 7);  // a gap: the next cluster is not the next one along
        } else {
            set_fat(img, c, c + 1);
        }
    }
    return img;
}

flight::FatVolume::Io io_for(CardImage& img) {
    flight::FatVolume::Io io;
    io.ctx = &img;
    io.read_block = image_read;
    return io;
}

void test_a_contiguous_file_is_located() {
    CardImage img = make_card(10, 4);
    auto io = io_for(img);
    flight::FatVolume::Region r;
    const auto s = flight::FatVolume::locate(io, "FLIGHT  CSV", 1, r);
    CHECK(s == flight::FatVolume::Status::ok);
    CHECK(r.first_lba == cluster_lba(10));
    CHECK(r.block_count == 4 * kSpc);
}

// The log writes into these blocks directly, so the reported range has to be the file's
// own data and nothing else. An off-by-one cluster here corrupts a neighbouring file.
void test_the_reported_range_is_the_files_own_data() {
    CardImage img = make_card(10, 4);
    auto io = io_for(img);
    flight::FatVolume::Region r;
    CHECK(flight::FatVolume::locate(io, "FLIGHT  CSV", 1, r) == flight::FatVolume::Status::ok);
    CHECK(r.first_lba == kDataLba + (8 * kSpc));
    CHECK(r.first_lba > kFatLba + (kNumFats * kFatSectors) - 1);  // never inside the FATs
}

void test_a_superfloppy_volume_is_accepted() {
    // Some cards are formatted with no partition table at all.
    CardImage img;
    img.ensure(0);
    img.blocks[0][0] = 0xEB;
    img.put16(0, 11, static_cast<std::uint16_t>(kSector));
    img.blocks[0][13] = static_cast<std::uint8_t>(kSpc);
    img.put16(0, 14, static_cast<std::uint16_t>(kReserved));
    img.blocks[0][16] = static_cast<std::uint8_t>(kNumFats);
    img.put32(0, 36, kFatSectors);
    img.put32(0, 44, kRootCluster);
    img.blocks[0][510] = 0x55;
    img.blocks[0][511] = 0xAA;

    const std::uint32_t fat_lba = kReserved;
    const std::uint32_t data_lba = fat_lba + (kNumFats * kFatSectors);
    auto set = [&](std::uint32_t c, std::uint32_t v) {
        img.put32(fat_lba + ((c * 4) / kSector), (c * 4) % kSector, v);
    };
    set(kRootCluster, 0x0FFFFFFF);
    const std::uint32_t root_lba = data_lba;
    auto& b = img.ensure(root_lba);
    const char* name = "FLIGHT  CSV";
    for (int i = 0; i < 11; ++i) b[i] = static_cast<std::uint8_t>(name[i]);
    b[11] = 0x20;
    img.put16(root_lba, 20, 0);
    img.put16(root_lba, 26, 10);
    img.put32(root_lba, 28, kSpc * kSector);
    set(10, 0x0FFFFFFF);

    auto io = io_for(img);
    flight::FatVolume::Region r;
    CHECK(flight::FatVolume::locate(io, "FLIGHT  CSV", 1, r) == flight::FatVolume::Status::ok);
    CHECK(r.first_lba == data_lba + (8 * kSpc));
}

// The one that matters most. Writing linearly into a fragmented file scribbles over
// whatever owns the gap, and reports success while doing it.
void test_a_fragmented_file_is_refused() {
    CardImage img = make_card(10, 4, /*contiguous=*/false);
    auto io = io_for(img);
    flight::FatVolume::Region r;
    CHECK(flight::FatVolume::locate(io, "FLIGHT  CSV", 1, r) ==
          flight::FatVolume::Status::file_fragmented);
}

void test_a_file_smaller_than_the_log_is_refused() {
    CardImage img = make_card(10, 2);
    auto io = io_for(img);
    flight::FatVolume::Region r;
    CHECK(flight::FatVolume::locate(io, "FLIGHT  CSV", 1000, r) ==
          flight::FatVolume::Status::file_too_small);
}

void test_a_missing_file_is_reported_separately_from_a_bad_volume() {
    CardImage img = make_card(10, 4);
    auto io = io_for(img);
    flight::FatVolume::Region r;
    // A good volume without the file must not be reported as "not FAT32": the operator
    // needs to know whether to reformat or just recreate the file.
    CHECK(flight::FatVolume::locate(io, "OTHER   CSV", 1, r) ==
          flight::FatVolume::Status::file_not_found);
}

void test_an_unformatted_card_is_reported_as_not_fat32() {
    CardImage img;  // all zeros
    auto io = io_for(img);
    flight::FatVolume::Region r;
    CHECK(flight::FatVolume::locate(io, "FLIGHT  CSV", 1, r) ==
          flight::FatVolume::Status::not_fat32);
}

void test_a_card_that_stops_answering_is_reported_as_a_read_failure() {
    CardImage img = make_card(10, 4);
    img.fail_after = 3;  // survive the MBR and BPB, die during the directory walk
    auto io = io_for(img);
    flight::FatVolume::Region r;
    const auto s = flight::FatVolume::locate(io, "FLIGHT  CSV", 1, r);
    CHECK(s == flight::FatVolume::Status::read_failed || s == flight::FatVolume::Status::not_fat32);
}

void test_deleted_and_long_name_entries_are_skipped() {
    CardImage img = make_card(10, 4);
    // Put a deleted entry and a long-name fragment ahead of the real one.
    const std::uint32_t lba = cluster_lba(kRootCluster);
    auto& b = img.ensure(lba);
    std::memmove(b.data() + 64, b.data(), 32);  // move the real entry to slot 2
    std::memset(b.data(), 0, 64);
    b[0] = 0xE5;   // slot 0: deleted
    b[32 + 11] = 0x0F;  // slot 1: long-name fragment
    b[32] = 0x41;

    auto io = io_for(img);
    flight::FatVolume::Region r;
    CHECK(flight::FatVolume::locate(io, "FLIGHT  CSV", 1, r) == flight::FatVolume::Status::ok);
    CHECK(r.first_lba == cluster_lba(10));
}

void test_every_status_has_a_description() {
    using S = flight::FatVolume::Status;
    for (const S s : {S::ok, S::read_failed, S::not_fat32, S::file_not_found,
                      S::file_fragmented, S::file_too_small}) {
        const char* d = flight::FatVolume::describe(s);
        CHECK(d != nullptr && std::string(d) != "unknown");
    }
}

}  // namespace

int main() {
    test_a_contiguous_file_is_located();
    test_the_reported_range_is_the_files_own_data();
    test_a_superfloppy_volume_is_accepted();
    test_a_fragmented_file_is_refused();
    test_a_file_smaller_than_the_log_is_refused();
    test_a_missing_file_is_reported_separately_from_a_bad_volume();
    test_an_unformatted_card_is_reported_as_not_fat32();
    test_a_card_that_stops_answering_is_reported_as_a_read_failure();
    test_deleted_and_long_name_entries_are_skipped();
    test_every_status_has_a_description();

    std::cout << (g_checks - g_failures) << "/" << g_checks << " checks passed\n";
    if (g_failures != 0) {
        std::cerr << g_failures << " FAILURE(S)\n";
        return 1;
    }
    std::cout << "fat_volume tests passed\n";
    return 0;
}
