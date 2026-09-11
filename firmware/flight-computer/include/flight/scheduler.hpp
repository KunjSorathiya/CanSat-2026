#pragma once

#include <cstdint>

namespace flight {

// Fixed-period task timer for a cooperative, non-blocking scheduler. It never allocates
// and it self-corrects after a loop stall (no burst of catch-up firings).
class PeriodicTask {
public:
    PeriodicTask() = default;
    explicit PeriodicTask(std::uint32_t period_ms, std::uint64_t first_due_ms = 0);

    void configure(std::uint32_t period_ms, std::uint64_t first_due_ms = 0);
    void set_period(std::uint32_t period_ms);
    // Sets the next due time to `from_ms + period_ms`, and the period to `period_ms` for
    // every firing after that. due() advances by the period it held when it fired, which is
    // right for a fixed cadence and wrong for a schedule whose next interval depends on what
    // was just sent: in the max-rate pattern a rich packet needs a 374 ms slot and a lean
    // one 296, and only the caller knows which it just transmitted.
    void reschedule(std::uint64_t from_ms, std::uint32_t period_ms);

    // Returns true at most once per period. When it fires, the next due time advances by
    // one period; if the loop fell behind by more than a period, the next due time is
    // re-anchored to now + period.
    bool due(std::uint64_t now_ms);

    std::uint64_t next_due_ms() const { return next_ms_; }
    std::uint32_t period_ms() const { return period_ms_; }

private:
    std::uint32_t period_ms_ = 0;
    std::uint64_t next_ms_ = 0;
};

}  // namespace flight
