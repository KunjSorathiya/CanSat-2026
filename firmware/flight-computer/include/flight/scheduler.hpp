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
