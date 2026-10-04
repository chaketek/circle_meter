#include "page_common.h"

#include <cstdio>

#include "theme.h"

namespace cm::ui {

// UTF-8 の度記号。"\xB0C" と続けると 16 進エスケープが "B0C" と読まれるので、リテラルを分ける
#define CM_DEG "\xC2\xB0"

void formatMain(char* buf, size_t size, const DisplayValues& v, const Config& cfg) {
    if (!v.hasLambda) {
        std::snprintf(buf, size, "--");
    } else if (cfg.showAfr) {
        std::snprintf(buf, size, "%.1f", lambdaToAfr(v.lambda, cfg.stoich));
    } else {
        std::snprintf(buf, size, "%.3f", v.lambda);
    }
}

uint32_t mainColorHex(const DisplayValues& v) {
    if (!v.hasLambda || v.lambdaStale) {
        return color::kDisabled;
    }
    if (v.zone == LambdaZone::LeanHeavy) {
        return color::kLeanHeavy;  // DOC-23 §3.3: 白。ただし LEAN_HEAVY のときはゾーン色
    }
    return color::kText;
}

void formatEgt(char* buf, size_t size, const DisplayValues& v) {
    if (v.hasEgt) {
        std::snprintf(buf, size, "%d" CM_DEG "C", static_cast<int>(v.egtC + 0.5f));
    } else {
        std::snprintf(buf, size, "--" CM_DEG "C");
    }
}

uint32_t egtColorHex(const DisplayValues& v, bool blinkOn) {
    if (!v.hasEgt || v.egtStale) {
        return color::kDisabled;
    }
    if (v.egtLevel == EgtLevel::Danger) {
        return blinkOn ? color::kDanger : 0x601010;  // 2 Hz 点滅（SWR-46）
    }
    if (v.egtLevel == EgtLevel::Warn) {
        return color::kWarn;
    }
    return color::kEgtNormal;
}

bool egtAlarmOn(const DisplayValues& v, bool blinkOn) {
    return v.hasEgt && !v.egtStale && v.egtLevel == EgtLevel::Danger && blinkOn;
}

}  // namespace cm::ui
