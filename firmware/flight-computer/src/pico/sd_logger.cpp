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
// Raw card access, addressed by LBA. Used to read the filesystem itself.
bool sd_read_raw(void* ctx, std::uint32_t lba, std::uint8_t* out512) {
    return static_cast<pico::SdCard*>(ctx)->read_block(lba, out512);
}

// What RawBlockLog talks to: a flat array of blocks numbered 0..N-1, which is what the log
// has always assumed it had. The file behind it need not be one run on the card, and the
// log neither knows nor cares -- the translation happens here and nowhere else.
struct LogIo {
    pico::SdCard* card = nullptr;
    const FatVolume::Layout* layout = nullptr;
};

bool sd_read_block(void* ctx, std::uint32_t block, std::uint8_t* out512) {
    auto* io = static_cast<LogIo*>(ctx);
    std::uint32_t lba = 0;
    if (!io->layout->to_lba(block, lba)) return false;  // past the end: full, never wraps
    return io->card->read_block(lba, out512);
}

bool sd_write_block(void* ctx, std::uint32_t block, const std::uint8_t* in512) {
    auto* io = static_cast<LogIo*>(ctx);
    std::uint32_t lba = 0;
    if (!io->layout->to_lba(block, lba)) return false;
    return io->card->write_block(lba, in512);
}

LogIo g_log_io;
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
    fio.read_block = sd_read_raw;
    const FatVolume::Status status =
        FatVolume::locate(fio, kLogFileName83, kMinLogBlocks, layout_);
    locate_status_ = status;
    if (status != FatVolume::Status::ok) {
        healthy_ = false;
        return false;
    }

    // Block 0 is the first block of the file, not of the card. Everything RawBlockLog
    // does is in those terms, so a fragmented file costs it nothing.
    g_log_io.card = &card_;
    g_log_io.layout = &layout_;
    RawBlockLog::Io io;
    io.ctx = &g_log_io;
    io.read_block = sd_read_block;
    io.write_block = sd_write_block;
    healthy_ = log_.begin(io, 0, layout_.total_blocks);
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
