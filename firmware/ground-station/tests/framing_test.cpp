#include "ground/framing.hpp"
#include "ground/uplink_timer.hpp"

#include <cassert>
#include <fstream>
#include <iostream>
#include <map>
#include <string>
#include <utility>
#include <vector>

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

// The shared framing fixtures. The PC ground station and the web console replay the same
// file through their own decoders. Three implementations of one wire format disagreeing is
// a ground station that drops frames the bridge sent, or accepts frames it did not -- and
// they did disagree, on how to classify an oversized length field, until this file existed.
std::string from_hex(const std::string& hex) {
    std::string out;
    out.reserve(hex.size() / 2);
    for (std::size_t i = 0; i + 1 < hex.size(); i += 2) {
        out.push_back(static_cast<char>(std::stoi(hex.substr(i, 2), nullptr, 16)));
    }
    return out;
}

std::vector<std::string> split(const std::string& text, char sep) {
    std::vector<std::string> parts;
    std::string current;
    for (char c : text) {
        if (c == sep) { parts.push_back(current); current.clear(); }
        else { current.push_back(c); }
    }
    parts.push_back(current);
    return parts;
}

void test_shared_framing_fixtures(const std::string& repo_root) {
    const std::string path = repo_root + "/test-data/framing-cases.tsv";
    std::ifstream file(path);
    CHECK(file.is_open());
    if (!file.is_open()) {
        std::cerr << "  could not open " << path
                  << " (pass the repository root as argv[1])\n";
        return;
    }

    int cases = 0;
    std::string line;
    while (std::getline(file, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;

        const std::vector<std::string> cols = split(line, '\t');
        CHECK(cols.size() == 4);
        if (cols.size() != 4) continue;
        const std::string& name = cols[0];
        const std::string stream = from_hex(cols[1]);

        std::vector<std::pair<std::string, std::string>> expected;
        if (!cols[2].empty()) {
            for (const std::string& item : split(cols[2], ',')) {
                const std::size_t colon = item.find(':');
                CHECK(colon != std::string::npos);
                if (colon == std::string::npos) continue;
                expected.emplace_back(item.substr(0, colon), from_hex(item.substr(colon + 1)));
            }
        }

        std::map<std::string, unsigned long> totals;
        for (const std::string& part : split(cols[3], ';')) {
            const std::size_t eq = part.find('=');
            if (eq == std::string::npos) continue;
            totals[part.substr(0, eq)] = std::stoul(part.substr(eq + 1));
        }

        ground::FrameReader reader;
        std::vector<std::pair<std::string, std::string>> got;
        for (char c : stream) {
            std::string out;
            const auto status = reader.feed(c, out);
            if (status == ground::FrameReader::Status::ok) got.emplace_back("ok", out);
            else if (status == ground::FrameReader::Status::crc_error) got.emplace_back("crc", out);
        }

        ++cases;
        if (got != expected) {
            std::cerr << "FAIL framing fixture " << name << ": expected " << expected.size()
                      << " event(s), got " << got.size() << "\n";
            ++failures;
            continue;
        }
        CHECK(reader.frames_ok() == totals["frames_ok"]);
        CHECK(reader.crc_errors() == totals["crc_errors"]);
        CHECK(reader.resyncs() == totals["resyncs"]);
        CHECK(reader.overflows() == totals["overflows"]);
    }
    CHECK(cases >= 12);
}

// A command is sent in the gap after a vehicle packet, never on top of one.
void test_a_command_waits_for_the_gap_after_a_packet() {
    using ground::UplinkTimer;
    UplinkTimer timer;
    std::string out;

    CHECK(timer.hold("CMD-1", 1000));
    CHECK(!timer.hold("CMD-2", 1001));          // one at a time, and the first is kept
    CHECK(!timer.due(1100, out));               // nothing heard yet
    timer.heard_packet(1200);                   // a telemetry packet has just ended
    CHECK(!timer.due(1200 + UplinkTimer::kAfterPacketMs - 1, out));  // vehicle still turning round
    CHECK(timer.due(1200 + UplinkTimer::kAfterPacketMs, out));
    CHECK(out == "CMD-1");
    CHECK(!timer.waiting());
    CHECK(!timer.due(9000, out));               // handed over exactly once

    // A packet heard before the command was queued says nothing about the gap now.
    timer.heard_packet(20000);
    CHECK(timer.hold("CMD-3", 20005));
    CHECK(!timer.due(20040, out));

    // A vehicle that is never heard still gets the command, after kMaxHoldMs.
    UplinkTimer silent;
    CHECK(silent.hold("CMD-4", 50000));
    CHECK(!silent.due(50000 + UplinkTimer::kMaxHoldMs - 1, out));
    CHECK(silent.due(50000 + UplinkTimer::kMaxHoldMs, out));
    CHECK(out == "CMD-4");

    // And the millisecond clock wrapping does not strand a command.
    UplinkTimer wrap;
    CHECK(wrap.hold("CMD-5", 0xFFFFFFF0u));
    wrap.heard_packet(0xFFFFFFF8u);
    CHECK(wrap.due(0xFFFFFFF8u + UplinkTimer::kAfterPacketMs, out));
}

}  // namespace

int main(int argc, char** argv) {
    const std::string repo_root = argc > 1 ? argv[1] : ".";
    test_roundtrip();
    test_crc_detects_corruption();
    test_resync_after_garbage();
    test_a_truncated_header_does_not_swallow_the_next_frame();
    test_payload_with_newline_survives();
    test_known_crc_vector();
    test_shared_framing_fixtures(repo_root);
    test_a_command_waits_for_the_gap_after_a_packet();
    if (failures == 0) {
        std::cout << "ground framing tests passed\n";
        return 0;
    }
    std::cerr << failures << " ground framing test failure(s)\n";
    return 1;
}
