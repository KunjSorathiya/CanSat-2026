#pragma once

#include "flight/config.hpp"

#include <string>
#include <vector>

namespace flight {

struct Fault {
    FaultSeverity severity;
    std::string message;
};

class FaultManager {
public:
    void report(FaultSeverity severity, const std::string& message);
    bool has_critical() const;
    const std::vector<Fault>& faults() const;

private:
    std::vector<Fault> faults_;
};

}  // namespace flight
