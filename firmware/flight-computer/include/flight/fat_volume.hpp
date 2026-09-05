#pragma once

#include <cstdint>

namespace flight {

// Locates a pre-allocated, contiguous file on a FAT32 volume and reports the block range
// its data occupies.
//
// WHY THIS EXISTS. The flight log is a raw append-only block log with no filesystem
// (RawBlockLog), because FAT metadata updates are not atomic: a brownout or a hard landing
// during one can lose or corrupt the whole recording. That safety is worth keeping. But a
// card written that way does not mount on a PC, and the flight data has to be readable by
// someone who is not holding this repository.
//
// The resolution is to do the filesystem work *before* the flight, on a PC, and none of it
// during. tools/prepare_sd_card.py formats the card and creates a fixed-size FLIGHT.CSV.
// This class then finds where that file's data blocks physically live, and the log writes
// straight into them. The directory entry, the FAT chain and the file size never change
// while the vehicle is flying — so there is no metadata to corrupt, and the card still
// mounts and opens in a spreadsheet afterwards.
//
// CONTIGUITY IS CHECKED, NOT ASSUMED. Writing linearly into a fragmented file would
// scribble over whatever occupies the intervening clusters. A file whose FAT chain is not
// strictly sequential is rejected rather than used.
//
// Read-only, and no dependency on the Pico SDK: the same code runs against a synthetic
// image in the host tests.
class FatVolume {
public:
    struct Io {
        void* ctx = nullptr;
        bool (*read_block)(void* ctx, std::uint32_t lba, std::uint8_t* out512) = nullptr;
    };

    struct Region {
        std::uint32_t first_lba = 0;
        std::uint32_t block_count = 0;
    };

    enum class Status {
        ok,
        read_failed,       // the card did not answer
        not_fat32,         // no FAT32 volume, or a sector size this code does not handle
        file_not_found,    // the volume is fine; the pre-allocated file is not on it
        file_fragmented,   // found, but its clusters are not consecutive
        file_too_small,    // found and contiguous, but shorter than the log needs
    };

    // `name_83` is the on-disk 8.3 form: eight name bytes then three extension bytes,
    // space-padded, upper case, no dot. "FLIGHT  CSV" is FLIGHT.CSV.
    static Status locate(const Io& io, const char* name_83, std::uint32_t min_blocks,
                         Region& out);

    static const char* describe(Status status);
};

}  // namespace flight
