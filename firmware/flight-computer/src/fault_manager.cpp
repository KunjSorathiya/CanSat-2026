#include "flight/fault_manager.hpp"

namespace flight {

void FaultManager::report(FaultSeverity severity, const std::string& message) {
    faults_.push_back({severity, message});
}

bool FaultManager::has_critical() const {
    for (const auto& fault : faults_) {
        if (fault.severity == FaultSeverity::critical) return true;
    }
    return false;
}

const std::vector<Fault>& FaultManager::faults() const { return faults_; }

}  // namespace flight
