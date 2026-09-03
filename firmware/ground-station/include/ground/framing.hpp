#pragma once

#include <cstddef>
#include <cstdint>
#include <string>

namespace ground {

// USB-serial framing for the ground-station Pico -> PC link.
//
// Wire format, one frame:  '$' <len> ',' <crc16-hex4> ',' <payload bytes> '\n'
//   len      : decimal payload length in bytes
//   crc16    : CRC-16/CCITT-FALSE (poly 0x1021, init 0xFFFF) over the raw payload,
//              lower-case 4-hex-digit
//   payload  : exactly <len> bytes, may contain any byte except it is read by length
//
// The CRC lets the PC classify transport corruption separately from a malformed packet.
// A status frame uses payload starting with '#'.

std::uint16_t crc16_ccitt(const std::uint8_t* data, std::size_t len);
std::uint16_t crc16_ccitt(const std::string& data);

// Build one frame around `payload`.
std::string frame_encode(const std::string& payload);

// Incremental frame decoder. Feed received bytes; completed payloads come back via
// take(). Tracks corruption / desync counters for link-health reporting.
class FrameReader {
public:
    enum class Status { none, ok, crc_error, overflow };

    static constexpr std::size_t kMaxPayload = 512;

    // Feed one byte. Returns ok when `out` was filled with a CRC-valid payload; crc_error
    // when a full frame arrived but failed CRC (out holds the raw payload anyway);
    // overflow when a frame exceeded kMaxPayload (frame dropped).
    Status feed(char byte, std::string& out);

    std::uint32_t frames_ok() const { return frames_ok_; }
    std::uint32_t crc_errors() const { return crc_errors_; }
    std::uint32_t overflows() const { return overflows_; }
    std::uint32_t resyncs() const { return resyncs_; }

private:
    enum class State { idle, len, crc, payload };
    void reset_frame();

    State state_ = State::idle;
    std::string len_text_;
    std::string crc_text_;
    std::string payload_;
    std::size_t expected_ = 0;
    std::uint32_t frames_ok_ = 0;
    std::uint32_t crc_errors_ = 0;
    std::uint32_t overflows_ = 0;
    std::uint32_t resyncs_ = 0;
};

}  // namespace ground
