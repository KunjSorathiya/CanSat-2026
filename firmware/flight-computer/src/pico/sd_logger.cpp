#include "flight/pico/pico_hal.hpp"

#include <cstdint>

namespace flight {

namespace {
// Log region on the card, in 512-byte blocks. Block 0 of the region is the header.
// Start past the first 1 MiB so a partition table / boot sector is never touched.
constexpr std::uint32_t kLogBaseLba = 2048;
constexpr std::uint32_t kLogBlockCount = 131072;  // ~64 MiB of records
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
    RawBlockLog::Io io;
    io.ctx = &card_;
    io.read_block = sd_read_block;
    io.write_block = sd_write_block;
    healthy_ = log_.begin(io, kLogBaseLba, kLogBlockCount);
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
