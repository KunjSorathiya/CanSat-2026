#include "flight/fat_volume.hpp"

#include <cstddef>
#include <cstring>

namespace flight {
namespace {

constexpr std::size_t kSector = 512;

std::uint16_t rd16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>(p[0] | (p[1] << 8));
}

std::uint32_t rd32(const std::uint8_t* p) {
    return static_cast<std::uint32_t>(p[0]) | (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) | (static_cast<std::uint32_t>(p[3]) << 24);
}

// A card partitioned the usual way carries an MBR; one formatted as a "superfloppy" puts
// the BPB straight at LBA 0. Both are common on microSD cards, so both are accepted.
// Returns the LBA the volume starts at.
bool find_volume_start(const FatVolume::Io& io, std::uint32_t& volume_lba) {
    std::uint8_t sector[kSector];
    if (!io.read_block(io.ctx, 0, sector)) return false;
    if (sector[510] != 0x55 || sector[511] != 0xAA) return false;

    // A BPB has a jump instruction at byte 0 and a non-zero sector size. If LBA 0 looks
    // like one, take it as the volume itself.
    const bool looks_like_bpb =
        (sector[0] == 0xEB || sector[0] == 0xE9) && rd16(sector + 11) == kSector;
    if (looks_like_bpb) {
        volume_lba = 0;
        return true;
    }

    // Otherwise scan the four MBR partition entries for the first FAT32 one.
    for (int i = 0; i < 4; ++i) {
        const std::uint8_t* entry = sector + 446 + (i * 16);
        const std::uint8_t type = entry[4];
        if (type == 0x0B || type == 0x0C) {  // FAT32 CHS / FAT32 LBA
            volume_lba = rd32(entry + 8);
            return true;
        }
    }
    return false;
}

struct Bpb {
    std::uint32_t volume_lba = 0;
    std::uint32_t fat_lba = 0;
    std::uint32_t data_lba = 0;
    std::uint32_t sectors_per_cluster = 0;
    std::uint32_t root_cluster = 0;
};

bool read_bpb(const FatVolume::Io& io, std::uint32_t volume_lba, Bpb& out) {
    std::uint8_t s[kSector];
    if (!io.read_block(io.ctx, volume_lba, s)) return false;
    if (rd16(s + 11) != kSector) return false;          // bytes per sector
    if (rd16(s + 17) != 0) return false;                // root entry count: 0 on FAT32
    if (rd16(s + 22) != 0) return false;                // 16-bit FAT size: 0 on FAT32

    const std::uint32_t spc = s[13];
    const std::uint32_t reserved = rd16(s + 14);
    const std::uint32_t num_fats = s[16];
    const std::uint32_t fat_size = rd32(s + 36);
    if (spc == 0 || reserved == 0 || num_fats == 0 || fat_size == 0) return false;

    out.volume_lba = volume_lba;
    out.sectors_per_cluster = spc;
    out.fat_lba = volume_lba + reserved;
    out.data_lba = out.fat_lba + (num_fats * fat_size);
    out.root_cluster = rd32(s + 44);
    return out.root_cluster >= 2;
}

std::uint32_t cluster_lba(const Bpb& b, std::uint32_t cluster) {
    return b.data_lba + ((cluster - 2) * b.sectors_per_cluster);
}

// Reads one FAT32 entry. The top four bits are reserved and must be masked off before the
// value is compared with anything.
bool fat_entry(const FatVolume::Io& io, const Bpb& b, std::uint32_t cluster,
               std::uint32_t& next) {
    const std::uint32_t byte_offset = cluster * 4;
    std::uint8_t sector[kSector];
    if (!io.read_block(io.ctx, b.fat_lba + (byte_offset / kSector), sector)) return false;
    next = rd32(sector + (byte_offset % kSector)) & 0x0FFFFFFFu;
    return true;
}

bool name_matches(const std::uint8_t* entry, const char* name_83) {
    for (int i = 0; i < 11; ++i) {
        if (static_cast<char>(entry[i]) != name_83[i]) return false;
    }
    return true;
}

}  // namespace

FatVolume::Status FatVolume::locate(const Io& io, const char* name_83,
                                    std::uint32_t min_blocks, Layout& out) {
    if (io.read_block == nullptr || name_83 == nullptr) return Status::not_fat32;
    for (int i = 0; i < 11; ++i) {
        if (name_83[i] == '\0') return Status::not_fat32;  // short 8.3 name
    }

    std::uint32_t volume_lba = 0;
    if (!find_volume_start(io, volume_lba)) return Status::not_fat32;

    Bpb bpb;
    if (!read_bpb(io, volume_lba, bpb)) return Status::not_fat32;

    // Walk the root directory's cluster chain looking for the entry. Long-file-name
    // entries (attribute 0x0F) are skipped: the prep script writes a plain 8.3 name, so
    // the short entry is always present and is the one that carries the cluster number.
    std::uint32_t cluster = bpb.root_cluster;
    std::uint32_t first_cluster = 0;
    std::uint32_t file_size = 0;
    bool found = false;
    // A directory chain longer than this on a freshly prepared card means a corrupt FAT,
    // and following it would loop forever.
    constexpr std::uint32_t kMaxDirClusters = 4096;

    for (std::uint32_t visited = 0; !found && visited < kMaxDirClusters; ++visited) {
        if (cluster < 2 || cluster >= 0x0FFFFFF8u) break;
        for (std::uint32_t s = 0; s < bpb.sectors_per_cluster && !found; ++s) {
            std::uint8_t sector[kSector];
            if (!io.read_block(io.ctx, cluster_lba(bpb, cluster) + s, sector)) {
                return Status::read_failed;
            }
            for (std::size_t off = 0; off < kSector; off += 32) {
                const std::uint8_t* e = sector + off;
                if (e[0] == 0x00) return Status::file_not_found;  // end of directory
                if (e[0] == 0xE5) continue;                       // deleted
                if ((e[11] & 0x0F) == 0x0F) continue;             // long-name fragment
                if ((e[11] & 0x10) != 0) continue;                // subdirectory
                if (!name_matches(e, name_83)) continue;
                first_cluster = (static_cast<std::uint32_t>(rd16(e + 20)) << 16) | rd16(e + 26);
                file_size = rd32(e + 28);
                found = true;
                break;
            }
        }
        if (found) break;
        std::uint32_t next = 0;
        if (!fat_entry(io, bpb, cluster, next)) return Status::read_failed;
        cluster = next;
    }

    if (!found) return Status::file_not_found;
    if (first_cluster < 2) return Status::file_too_small;

    // Follow the file's chain, coalescing consecutive clusters into runs. Fragmentation is
    // tracked rather than refused: a file recreated before every flight cannot be relied on
    // to come back as one unbroken run, and refusing it turns a routine step into a
    // reformat. What is refused is a file in more runs than can be held.
    const std::uint32_t blocks_per_cluster = bpb.sectors_per_cluster;
    out = Layout{};
    out.extents[0].first_lba = cluster_lba(bpb, first_cluster);
    out.extents[0].block_count = blocks_per_cluster;
    out.extent_count = 1;

    std::uint32_t clusters = 1;
    std::uint32_t current = first_cluster;
    const std::uint32_t max_clusters =
        (file_size / (bpb.sectors_per_cluster * kSector)) + 2;
    while (clusters <= max_clusters) {
        std::uint32_t next = 0;
        if (!fat_entry(io, bpb, current, next)) return Status::read_failed;
        if (next >= 0x0FFFFFF8u) break;      // end of chain
        if (next == current + 1) {
            out.extents[out.extent_count - 1].block_count += blocks_per_cluster;
        } else {
            if (out.extent_count >= kMaxExtents) return Status::too_fragmented;
            out.extents[out.extent_count].first_lba = cluster_lba(bpb, next);
            out.extents[out.extent_count].block_count = blocks_per_cluster;
            ++out.extent_count;
        }
        current = next;
        ++clusters;
    }

    out.total_blocks = 0;
    for (int i = 0; i < out.extent_count; ++i) {
        out.total_blocks += out.extents[i].block_count;
    }
    if (out.total_blocks < min_blocks) return Status::file_too_small;
    return Status::ok;
}

bool FatVolume::Layout::to_lba(std::uint32_t logical_block, std::uint32_t& lba) const {
    std::uint32_t remaining = logical_block;
    for (int i = 0; i < extent_count; ++i) {
        if (remaining < extents[i].block_count) {
            lba = extents[i].first_lba + remaining;
            return true;
        }
        remaining -= extents[i].block_count;
    }
    return false;  // past the end of the file: full, never wrapping
}

const char* FatVolume::describe(Status status) {
    switch (status) {
        case Status::ok: return "ok";
        case Status::read_failed: return "the card stopped answering while being read";
        case Status::not_fat32:
            return "no FAT32 volume with 512-byte sectors - reformat with prepare_sd_card.py";
        case Status::file_not_found:
            return "the volume is fine but the pre-allocated log file is not on it";
        case Status::too_fragmented:
            return "the log file is in more separate runs than the firmware can track - "
                   "reformat the card and recreate it on the empty volume";
        case Status::file_too_small:
            return "the log file is smaller than the log region needs";
    }
    return "unknown";
}

}  // namespace flight
