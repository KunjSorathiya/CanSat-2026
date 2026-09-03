#include "ground/framing.hpp"

#include <array>
#include <cctype>

namespace ground {

std::uint16_t crc16_ccitt(const std::uint8_t* data, std::size_t len) {
    std::uint16_t crc = 0xFFFF;
    for (std::size_t i = 0; i < len; ++i) {
        crc ^= static_cast<std::uint16_t>(data[i]) << 8;
        for (int bit = 0; bit < 8; ++bit) {
            crc = (crc & 0x8000) ? static_cast<std::uint16_t>((crc << 1) ^ 0x1021)
                                 : static_cast<std::uint16_t>(crc << 1);
        }
    }
    return crc;
}

std::uint16_t crc16_ccitt(const std::string& data) {
    return crc16_ccitt(reinterpret_cast<const std::uint8_t*>(data.data()), data.size());
}

std::string frame_encode(const std::string& payload) {
    static const char* hex = "0123456789abcdef";
    const std::uint16_t crc = crc16_ccitt(payload);
    std::string out;
    out.reserve(payload.size() + 16);
    out.push_back('$');
    out += std::to_string(payload.size());
    out.push_back(',');
    out.push_back(hex[(crc >> 12) & 0xF]);
    out.push_back(hex[(crc >> 8) & 0xF]);
    out.push_back(hex[(crc >> 4) & 0xF]);
    out.push_back(hex[crc & 0xF]);
    out.push_back(',');
    out += payload;
    out.push_back('\n');
    return out;
}

void FrameReader::reset_frame() {
    state_ = State::idle;
    len_text_.clear();
    crc_text_.clear();
    payload_.clear();
    expected_ = 0;
}

FrameReader::Status FrameReader::feed(char byte, std::string& out) {
    switch (state_) {
        case State::idle:
            if (byte == '$') {
                len_text_.clear();
                crc_text_.clear();
                payload_.clear();
                expected_ = 0;
                state_ = State::len;
            }
            return Status::none;

        case State::len:
            if (byte >= '0' && byte <= '9') {
                len_text_.push_back(byte);
                if (len_text_.size() > 6) {  // absurd length: desync
                    ++resyncs_;
                    reset_frame();
                }
            } else if (byte == ',' && !len_text_.empty()) {
                expected_ = static_cast<std::size_t>(std::stoul(len_text_));
                if (expected_ > kMaxPayload) {
                    ++overflows_;
                    reset_frame();
                    return Status::overflow;
                }
                state_ = State::crc;
            } else if (byte == '$') {
                ++resyncs_;
                len_text_.clear();
            } else {
                ++resyncs_;
                reset_frame();
            }
            return Status::none;

        case State::crc:
            if (std::isxdigit(static_cast<unsigned char>(byte))) {
                crc_text_.push_back(byte);
                if (crc_text_.size() > 4) {
                    ++resyncs_;
                    reset_frame();
                }
            } else if (byte == ',' && crc_text_.size() == 4) {
                state_ = State::payload;
                if (expected_ == 0) {
                    // zero-length payload frame
                    out.clear();
                    const bool ok = crc16_ccitt(out) ==
                                    static_cast<std::uint16_t>(std::stoul(crc_text_, nullptr, 16));
                    reset_frame();
                    if (ok) {
                        ++frames_ok_;
                        return Status::ok;
                    }
                    ++crc_errors_;
                    return Status::crc_error;
                }
            } else {
                ++resyncs_;
                reset_frame();
            }
            return Status::none;

        case State::payload:
            payload_.push_back(byte);
            if (payload_.size() < expected_) {
                return Status::none;
            }
            {
                const std::uint16_t want =
                    static_cast<std::uint16_t>(std::stoul(crc_text_, nullptr, 16));
                const bool ok = crc16_ccitt(payload_) == want;
                out = payload_;
                reset_frame();
                if (ok) {
                    ++frames_ok_;
                    return Status::ok;
                }
                ++crc_errors_;
                return Status::crc_error;
            }
    }
    return Status::none;
}

}  // namespace ground
