#include "ground/framing.hpp"

#include <cassert>
#include <iostream>
#include <string>

namespace {

int failures = 0;
#define CHECK(cond)                                                            \
    do {                                                                      \
        if (!(cond)) {                                                        \
            std::cerr << "FAIL " << __LINE__ << ": " #cond "\n";             \
            ++failures;                                                       \
        }                                                                     \
    } while (0)

std::string feed_all(ground::FrameReader& reader, const std::string& bytes,
                     ground::FrameReader::Status& last, std::string& payload) {
    last = ground::FrameReader::Status::none;
    for (char c : bytes) {
        std::string out;
        auto s = reader.feed(c, out);
        if (s != ground::FrameReader::Status::none) {
            last = s;
            payload = out;
        }
    }
    return payload;
}

void test_roundtrip() {
    const std::string packet =
        "CAN-Team-01; P-001; Ti-00:00:01:000; A-10.0; Pr-101325.00; T-25.0; "
        "Ro-1.0; Pi-2.0; Ya-3.0; AX-0.10; AY-0.20; AZ-9.80;";
    const std::string frame = ground::frame_encode(packet);
    CHECK(frame.front() == '$');
    CHECK(frame.back() == '\n');

    ground::FrameReader reader;
    ground::FrameReader::Status last;
    std::string payload;
    feed_all(reader, frame, last, payload);
    CHECK(last == ground::FrameReader::Status::ok);
    CHECK(payload == packet);
    CHECK(reader.frames_ok() == 1);
    CHECK(reader.crc_errors() == 0);
}

void test_crc_detects_corruption() {
    std::string frame = ground::frame_encode("hello world payload");
    // Flip a byte in the payload region (after the second comma).
    const auto second_comma = frame.find(',', frame.find(',') + 1);
    frame[second_comma + 3] ^= 0x40;

    ground::FrameReader reader;
    ground::FrameReader::Status last;
    std::string payload;
    feed_all(reader, frame, last, payload);
    CHECK(last == ground::FrameReader::Status::crc_error);
    CHECK(reader.crc_errors() == 1);
    CHECK(reader.frames_ok() == 0);
}

void test_resync_after_garbage() {
    ground::FrameReader reader;
    ground::FrameReader::Status last;
    std::string payload;
    const std::string stream =
        std::string("garbage!!\x00 noise ") + ground::frame_encode("PAYLOAD-A") +
        "\xff\xfe" + ground::frame_encode("PAYLOAD-B");
    feed_all(reader, stream, last, payload);
    CHECK(reader.frames_ok() == 2);
    CHECK(payload == "PAYLOAD-B");
}

// A header truncated mid-CRC -- a dropped byte, a receiver that lost lock -- is followed
// immediately by the next frame's own '$'. Discarding that byte as part of the resync
// would cost the following frame as well, turning one corrupted header into two lost
// packets.
void test_a_truncated_header_does_not_swallow_the_next_frame() {
    ground::FrameReader reader;
    ground::FrameReader::Status last;
    std::string payload;
    // "$9,ab" is a header cut off inside its CRC field; the next frame starts right after.
    feed_all(reader, std::string("$9,ab") + ground::frame_encode("PAYLOAD-B"), last, payload);
    CHECK(reader.frames_ok() == 1);
    CHECK(payload == "PAYLOAD-B");
    CHECK(reader.resyncs() == 1);

    // The same for a length field interrupted by a new frame.
    ground::FrameReader second;
    feed_all(second, std::string("$12") + ground::frame_encode("PAYLOAD-C"), last, payload);
    CHECK(second.frames_ok() == 1);
    CHECK(payload == "PAYLOAD-C");
}

void test_payload_with_newline_survives() {
    // Length-based read means an embedded newline in a (corrupt) payload is still framed.
    const std::string packet = "abc\ndef";
    ground::FrameReader reader;
    ground::FrameReader::Status last;
    std::string payload;
    feed_all(reader, ground::frame_encode(packet), last, payload);
    CHECK(last == ground::FrameReader::Status::ok);
    CHECK(payload == packet);
}

void test_known_crc_vector() {
    // CRC-16/CCITT-FALSE("123456789") == 0x29B1
    CHECK(ground::crc16_ccitt(std::string("123456789")) == 0x29B1);
}

}  // namespace

int main() {
    test_roundtrip();
    test_crc_detects_corruption();
    test_resync_after_garbage();
    test_a_truncated_header_does_not_swallow_the_next_frame();
    test_payload_with_newline_survives();
    test_known_crc_vector();
    if (failures == 0) {
        std::cout << "ground framing tests passed\n";
        return 0;
    }
    std::cerr << failures << " ground framing test failure(s)\n";
    return 1;
}
