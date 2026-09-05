#pragma once

#include <cstdint>

namespace flight {

// Locates a pre-allocated file on a FAT32 volume and reports where its data physically
// lives, as a short list of extents.
//
// WHY THIS EXISTS. The flight log is a raw append-only block log with no filesystem
// (RawBlockLog), because FAT metadata updates are not atomic: a brownout or a hard landing
// during one can lose or corrupt the whole recording. That safety is worth keeping. But a
// card written that way does not mount on a PC, and flight data nobody can read is not
// evidence.
//
// The resolution is to do the filesystem work *before* the flight, on a PC, and none of it
// during. tools/prepare_sd_card.py creates a fixed-size FLIGHT.CSV. This class then finds
// where that file's blocks physically live, and the log writes straight into them. The
// directory entry, the FAT chain and the file size never change while the vehicle is
// flying, so there is no metadata to corrupt, and the card still mounts afterwards.
//
// WHY EXTENTS RATHER THAN ONE RANGE. An earlier version demanded the file be contiguous and
// refused anything else. That was safe but operationally wrong: the file has to be recreated
// before every flight, and a filesystem is under no obligation to hand back one unbroken run
// -- on the delivered card it did so once and then split the next attempt in two. "Reformat
// the card and try again" is exactly the pre-flight step that gets skipped at six in the
// morning, and skipping it costs the entire log.
//
// So the chain is followed rather than assumed. Consecutive clusters are coalesced into
// runs, and a file is refused only when it is broken into more runs than kMaxExtents -- a
// genuinely unhealthy card rather than an ordinary one. Sixteen extents is 128 bytes of
// state and covers far worse fragmentation than a freshly prepared card will ever show.
//
// Read-only, and no dependency on the Pico SDK: the same code runs against a synthetic
// image in the host tests.
class FatVolume {
public:
    // A file needing more runs than this is refused rather than tracked. The bound exists
    // so the mapping stays a fixed-size array in a vehicle with no dynamic allocation.
    static constexpr int kMaxExtents = 16;

    struct Io {
        void* ctx = nullptr;
        bool (*read_block)(void* ctx, std::uint32_t lba, std::uint8_t* out512) = nullptr;
    };

    struct Extent {
        std::uint32_t first_lba = 0;
        std::uint32_t block_count = 0;
    };

    struct Layout {
        Extent extents[kMaxExtents]{};
        int extent_count = 0;
        std::uint32_t total_blocks = 0;

        // Map a logical 512-byte block within the file onto its LBA on the card.
        //
        // This is the whole point of the extent list: the log addresses blocks 0..N-1 of a
        // file and never needs to know they are not consecutive on the medium. Returns
        // false for a block past the end of the file, which the caller must treat as the
        // region being full rather than wrapping to the start.
        bool to_lba(std::uint32_t logical_block, std::uint32_t& lba) const;
    };

    enum class Status {
        ok,
        read_failed,      // the card did not answer
        not_fat32,        // no FAT32 volume, or a sector size this code does not handle
        file_not_found,   // the volume is fine; the pre-allocated file is not on it
        too_fragmented,   // found, but in more separate runs than kMaxExtents
        file_too_small,   // found and usable, but shorter than the log needs
    };

    // `name_83` is the on-disk 8.3 form: eight name bytes then three extension bytes,
    // space-padded, upper case, no dot. "FLIGHT  CSV" is FLIGHT.CSV.
    static Status locate(const Io& io, const char* name_83, std::uint32_t min_blocks,
                         Layout& out);

    static const char* describe(Status status);
};

}  // namespace flight
