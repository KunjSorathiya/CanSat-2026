#include "flight/pico/pico_hal.hpp"

#include "flight/fat_volume.hpp"

#include <cstdint>

namespace flight {

namespace {
// The log lives inside a pre-allocated file on a FAT32 card, so the card still mounts on a
// PC and the records open in a spreadsheet. See flight/fat_volume.hpp for why the
// filesystem work happens before the flight and none of it during.
//
// The 8.3 on-disk name of that file: eight name bytes, three extension bytes, space
// padded. tools/prepare_sd_card.py creates it.
constexpr const char* kLogFileName83 = "FLIGHT  CSV";

// Smallest region worth flying with, in 512-byte blocks. Two are headers and the rest hold
// one record each, so this is about 8000 records - well over two hours at 1 Hz.
constexpr std::uint32_t kMinLogBlocks = 8192;
}  // namespace

#ifdef PICO_BUILD

namespace {
bool sd_read_block(void* ctx, std::uint32_t lba, std::uint8_t* out512) {
    return static_cast<pico::SdCard*>(ctx)->read_block(lba, out512);
}
bool sd_write_block(void* ctx, std::uint32_t lba, const std::uint8_t* in512) {
    return static_cast<pico::SdCard*>(ctx)->write_block(lba, in512);
}
}  // namespace

bool PicoSdLogger::initialize() {
    pico_buses_init();
    if (!card_.begin(spi0, BoardPins::sd_cs)) {
        healthy_ = false;
        return false;
    }
    // Find the pre-allocated file before writing anything. If it is missing, fragmented
    // or too small, this fails rather than falling back to a fixed LBA: writing raw blocks
    // at a guessed address would destroy the filesystem, which is the one outcome this
    // whole arrangement exists to avoid. A vehicle with no log still flies and still
    // transmits; a vehicle that silently ate the card does not get its data back either
    // way, and takes the operator's confidence with it.
    FatVolume::Io fio;
    fio.ctx = &card_;
    fio.read_block = sd_read_block;
    FatVolume::Region region;
    const FatVolume::Status status =
        FatVolume::locate(fio, kLogFileName83, kMinLogBlocks, region);
    locate_status_ = status;
    if (status != FatVolume::Status::ok) {
        healthy_ = false;
        return false;
    }

    RawBlockLog::Io io;
    io.ctx = &card_;
    io.read_block = sd_read_block;
    io.write_block = sd_write_block;
    healthy_ = log_.begin(io, region.first_lba, region.block_count);
    if (healthy_ && log_.record_count() == 0) {
        // A fresh log gets a column header, so the file opens as a spreadsheet rather than
        // as a wall of unlabelled fields. Written as an ordinary record, so it costs one
        // block and needs no special case anywhere else.
        static constexpr char kCsvHeader[] =
            "team_id,packet,mission_time,altitude_m,pressure_pa,temperature_c,"
            "roll_deg,pitch_deg,yaw_deg,ax_mps2,ay_mps2,az_mps2,"
            "gps_lat,gps_lon,gps_alt_m,mode,faults,cal,armed";
        log_.append_line(kCsvHeader, sizeof(kCsvHeader) - 1);
    }
    return healthy_;
}

bool PicoSdLogger::append(const cansat::TelemetryRecord&, const std::string& packet) {
    if (!healthy_) return false;
    const bool ok = log_.append_line(packet.c_str(), packet.size());
    if (!ok && log_.full()) {
        healthy_ = false;  // region exhausted; controller stops calling us
    }
    return ok;
}

bool PicoSdLogger::flush() {
    // RawBlockLog rewrites its header after every record, so there is nothing buffered.
    return healthy_;
}

#else  // host stubs

bool PicoSdLogger::initialize() {
    healthy_ = false;
    return false;
}
bool PicoSdLogger::append(const cansat::TelemetryRecord&, const std::string&) { return false; }
bool PicoSdLogger::flush() { return false; }

#endif

}  // namespace flight
