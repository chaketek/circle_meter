// SWD-13 / SWR-51 実装
#include "display_text.h"

#include <cstdio>

namespace cm {

void formatSensorStatus(char* buf, size_t size, const LambdaSensorInfo& info, const char* degSuffix) {
    if (buf == nullptr || size == 0) {
        return;
    }
    const int t = static_cast<int>(info.tempC + 0.5f);
    switch (info.state) {
        case LambdaSensorState::NoSignal:
            std::snprintf(buf, size, "NO SIGNAL");
            break;
        case LambdaSensorState::SensorOff:
            std::snprintf(buf, size, "SENSOR OFF");
            break;
        case LambdaSensorState::WarmingUp:
            if (info.hasTemp) {
                std::snprintf(buf, size, "WARMING UP %d%s", t, degSuffix);
            } else {
                std::snprintf(buf, size, "WARMING UP");
            }
            break;
        case LambdaSensorState::Check:
            if (info.hasTemp) {
                std::snprintf(buf, size, "SENSOR CHECK %d%s", t, degSuffix);
            } else {
                std::snprintf(buf, size, "SENSOR CHECK");
            }
            break;
        case LambdaSensorState::FaultNoHeat:
            std::snprintf(buf, size, "SENSOR FAULT: NO HEAT");
            break;
        case LambdaSensorState::FaultOverheat:
            std::snprintf(buf, size, "SENSOR FAULT: OVERHEAT");
            break;
        case LambdaSensorState::FaultUnderheat:
            std::snprintf(buf, size, "SENSOR FAULT: UNDERHEAT");
            break;
        case LambdaSensorState::NoWbo:
            std::snprintf(buf, size, "NO WBO DATA");
            break;
        default:
            buf[0] = '\0';
            break;
    }
}

}  // namespace cm
