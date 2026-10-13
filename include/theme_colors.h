// SWA-23: 配色（DOC-23 §8 / §8.1 / §8.2）
//
// LVGL にも M5GFX にも依存しない。LCD-2.1（lib/ui/theme.h 経由）と M5Dial（src/main.cpp）が同じ値を使い、
// 配布先ユーザーの USER_COLOR_*（user/user_config.h。SWR-67）もどちらにも効く（SWR-68）。
#pragma once

#include <cstdint>

#include "units.h"
#include "user_overrides.h"

namespace cm::ui {

// ---- 配色 DOC-23 §8 / §8.1
// USER_COLOR_* は配布先ユーザーの上書き（user/user_config.h。SWR-67 / SWD-14）。既定値は仕様値のまま
namespace color {
#ifdef USER_COLOR_BG
constexpr uint32_t kBg = USER_COLOR_BG;
#else
constexpr uint32_t kBg = 0x000000;
#endif
#ifdef USER_COLOR_TRACK
constexpr uint32_t kTrack = USER_COLOR_TRACK;
#else
constexpr uint32_t kTrack = 0x1A1A1A;  ///< リング軌道（未到達部）
#endif
#ifdef USER_COLOR_TEXT
constexpr uint32_t kText = USER_COLOR_TEXT;
#else
constexpr uint32_t kText = 0xFFFFFF;
#endif
#ifdef USER_COLOR_TEXT_SUB
constexpr uint32_t kTextSub = USER_COLOR_TEXT_SUB;
#else
constexpr uint32_t kTextSub = 0x8A8A8A;
#endif
constexpr uint32_t kDisabled = 0x5A5A5A;
#ifdef USER_COLOR_RICH_HEAVY
constexpr uint32_t kRichHeavy = USER_COLOR_RICH_HEAVY;
#else
constexpr uint32_t kRichHeavy = 0x2E7DFF;
#endif
#ifdef USER_COLOR_RICH
constexpr uint32_t kRich = USER_COLOR_RICH;
#else
constexpr uint32_t kRich = 0x00C8D7;
#endif
#ifdef USER_COLOR_OPTIMAL
constexpr uint32_t kOptimal = USER_COLOR_OPTIMAL;
#else
constexpr uint32_t kOptimal = 0x17D14B;
#endif
#ifdef USER_COLOR_LEAN
constexpr uint32_t kLean = USER_COLOR_LEAN;
#else
constexpr uint32_t kLean = 0xFFC400;
#endif
#ifdef USER_COLOR_LEAN_HEAVY
constexpr uint32_t kLeanHeavy = USER_COLOR_LEAN_HEAVY;
#else
constexpr uint32_t kLeanHeavy = 0xFF2D2D;
#endif
#ifdef USER_COLOR_WARN
constexpr uint32_t kWarn = USER_COLOR_WARN;
#else
constexpr uint32_t kWarn = 0xFFC400;
#endif
#ifdef USER_COLOR_DANGER
constexpr uint32_t kDanger = USER_COLOR_DANGER;
#else
constexpr uint32_t kDanger = 0xFF2D2D;
#endif
#ifdef USER_COLOR_EGT_NORMAL
constexpr uint32_t kEgtNormal = USER_COLOR_EGT_NORMAL;
#else
constexpr uint32_t kEgtNormal = 0xE0E0E0;
#endif
// 指針式 A（DOC-23 §3.5）
#ifdef USER_COLOR_DIAL_BG
constexpr uint32_t kDialBg = USER_COLOR_DIAL_BG;
#else
constexpr uint32_t kDialBg = 0x060606;  ///< 指針式 A の盤面
#endif
#ifdef USER_COLOR_NEEDLE
constexpr uint32_t kNeedle = USER_COLOR_NEEDLE;
#else
constexpr uint32_t kNeedle = 0xFF7014;  ///< 指針式 A の針
#endif
#ifdef USER_COLOR_DIAL_RIM
constexpr uint32_t kDialRim = USER_COLOR_DIAL_RIM;
#else
constexpr uint32_t kDialRim = 0x787878;  ///< 外周線
#endif
#ifdef USER_COLOR_DIAL_TICK
constexpr uint32_t kDialTick = USER_COLOR_DIAL_TICK;
#else
constexpr uint32_t kDialTick = 0xEBEBEB;  ///< 目盛り・ゾーン境界の三角
#endif
#ifdef USER_COLOR_DIAL_NUMBER
constexpr uint32_t kDialNumber = USER_COLOR_DIAL_NUMBER;
#else
constexpr uint32_t kDialNumber = 0xF0F0F0;  ///< 目盛り数字
#endif
#ifdef USER_COLOR_DIAL_LABEL
constexpr uint32_t kDialLabel = USER_COLOR_DIAL_LABEL;
#else
constexpr uint32_t kDialLabel = 0xC8C8C8;  ///< 銘板「A/F」
#endif
#ifdef USER_COLOR_DIAL_SMALL
constexpr uint32_t kDialSmall = USER_COLOR_DIAL_SMALL;
#else
constexpr uint32_t kDialSmall = 0x8C8C8C;  ///< 銘板「AIR FUEL RATIO」
#endif
constexpr uint32_t kDialEgtLabel = 0x828282;  ///< 盤面の「EGT」
constexpr uint32_t kDialDiag     = 0x6A6A6A;  ///< 指針式 A の診断文字列
}  // namespace color

constexpr uint32_t zoneColorHex(LambdaZone z) {
    switch (z) {
        case LambdaZone::RichHeavy:
            return color::kRichHeavy;
        case LambdaZone::Rich:
            return color::kRich;
        case LambdaZone::Optimal:
            return color::kOptimal;
        case LambdaZone::Lean:
            return color::kLean;
        default:
            return color::kLeanHeavy;
    }
}

}  // namespace cm::ui
