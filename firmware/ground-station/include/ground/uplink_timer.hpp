#pragma once

#include <cstdint>
#include <string>

namespace ground {

// When the bridge may put a command on the air.
//
// The radio is half duplex at both ends. While the bridge transmits a command -- about 110 ms
// for a MAX_RATE frame at SF7/125 kHz -- it cannot hear the vehicle, and while the vehicle
// transmits it cannot hear the bridge. A command relayed the instant the PC sends it lands at
// a random point in the vehicle's cycle: either on top of a telemetry packet, which the bridge
// then misses (both bench runs of MAX_RATE lost exactly one packet, 1 in 544 and 1 in 327),
// or while the vehicle is on the air, in which case the vehicle misses the command.
//
// So the bridge waits for the gap. The vehicle listens from the end of each packet until it
// polls just before its next one, so a command sent kAfterPacketMs after a packet is heard in
// full while the vehicle is listening, and is over long before the vehicle's next packet
// starts: in the 700 ms schedule the command window runs on, that is at least ~375 ms later.
// If the vehicle is not heard at all -- out of range, or powered off -- the command goes
// anyway after kMaxHoldMs, which is no worse than it used to be.
class UplinkTimer {
public:
    // After the vehicle's packet ends, time for it to leave TX and key its receiver: one 2 ms
    // loop tick, with room for a sensor read or an SD write in progress.
    static constexpr std::uint32_t kAfterPacketMs = 20;
    // A vehicle that has not been heard for this long is not going to open a gap to wait for.
    static constexpr std::uint32_t kMaxHoldMs = 1500;

    // Holds one command until its moment. False when one is already waiting: the console
    // sends one command at a time, and a second must not silently replace the first.
    bool hold(const std::string& command, std::uint32_t now_ms) {
        if (waiting_) return false;
        pending_ = command;
        waiting_ = true;
        heard_ = false;
        queued_ms_ = now_ms;
        return true;
    }

    // A vehicle packet has just ended. Only a packet heard after the command was queued
    // counts: an earlier one says nothing about where the vehicle is in its cycle now.
    void heard_packet(std::uint32_t now_ms) {
        if (!waiting_) return;
        heard_ = true;
        heard_ms_ = now_ms;
    }

    // Hands the command over, once, when it is time to send it.
    bool due(std::uint32_t now_ms, std::string& out) {
        if (!waiting_) return false;
        const bool in_gap = heard_ && now_ms - heard_ms_ >= kAfterPacketMs;
        const bool given_up = now_ms - queued_ms_ >= kMaxHoldMs;
        if (!in_gap && !given_up) return false;
        out = pending_;
        pending_.clear();
        waiting_ = false;
        return true;
    }

    bool waiting() const { return waiting_; }

private:
    std::string pending_;
    bool waiting_ = false;
    bool heard_ = false;
    std::uint32_t queued_ms_ = 0;
    std::uint32_t heard_ms_ = 0;
};

}  // namespace ground
