#include "flight/scheduler.hpp"

namespace flight {

PeriodicTask::PeriodicTask(std::uint32_t period_ms, std::uint64_t first_due_ms)
    : period_ms_(period_ms), next_ms_(first_due_ms) {}

void PeriodicTask::configure(std::uint32_t period_ms, std::uint64_t first_due_ms) {
    period_ms_ = period_ms;
    next_ms_ = first_due_ms;
}

void PeriodicTask::set_period(std::uint32_t period_ms) {
    period_ms_ = period_ms;
}

void PeriodicTask::reschedule(std::uint64_t from_ms, std::uint32_t period_ms) {
    period_ms_ = period_ms;
    next_ms_ = from_ms + period_ms;
}

bool PeriodicTask::due(std::uint64_t now_ms) {
    if (period_ms_ == 0) {
        return false;
    }
    if (now_ms < next_ms_) {
        return false;
    }
    next_ms_ += period_ms_;
    if (next_ms_ <= now_ms) {
        next_ms_ = now_ms + period_ms_;
    }
    return true;
}

}  // namespace flight
