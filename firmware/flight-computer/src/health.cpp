#include "flight/health.hpp"

namespace flight {

const char* to_string(MissionState state) {
    switch (state) {
        case MissionState::init: return "INIT";
        case MissionState::self_test: return "SELF_TEST";
        case MissionState::ready: return "READY";
        case MissionState::flight: return "FLIGHT";
        case MissionState::landed: return "LANDED";
        case MissionState::recovery: return "RECOVERY";
        case MissionState::fault: return "FAULT";
    }
    return "UNKNOWN";
}

}  // namespace flight
