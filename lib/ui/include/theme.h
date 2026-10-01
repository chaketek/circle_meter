// SWA-23: 配色・レイアウト定数（DOC-23 §2 / §8）
//
// 座標は 480x480 の絶対値。240x240（M5Dial）時代の寸法をちょうど 2 倍にして導出した（DOC-23 §2）。
// 画面の見え方を変える変更は、必ず DOC-23 を先に更新すること。
#pragma once

#include <cstdint>

#include "lvgl.h"
#include "units.h"

namespace cm::ui {

// ---- 配色 DOC-23 §8
namespace color {
constexpr uint32_t kBg        = 0x000000;
constexpr uint32_t kTrack     = 0x1A1A1A;  ///< リング軌道（未到達部）
constexpr uint32_t kText      = 0xFFFFFF;
constexpr uint32_t kTextSub   = 0x8A8A8A;
constexpr uint32_t kDisabled  = 0x5A5A5A;
constexpr uint32_t kRichHeavy = 0x2E7DFF;
constexpr uint32_t kRich      = 0x00C8D7;
constexpr uint32_t kOptimal   = 0x17D14B;
constexpr uint32_t kLean      = 0xFFC400;
constexpr uint32_t kLeanHeavy = 0xFF2D2D;
constexpr uint32_t kWarn      = 0xFFC400;
constexpr uint32_t kDanger    = 0xFF2D2D;
constexpr uint32_t kEgtNormal = 0xE0E0E0;
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

// ---- レイアウト DOC-23 §2 / §2.1
namespace layout {
constexpr int kWidth  = 480;
constexpr int kHeight = 480;
constexpr int kCx     = 240;
constexpr int kCy     = 240;

constexpr int kRingOuter  = 236;  ///< λ リング外周（236-240 はベゼル余白。EGT DANGER の警告帯に使う）
constexpr int kRingInner  = 192;  ///< λ リング内周（帯幅 44 px）
constexpr int kAlarmBand  = 5;    ///< DANGER 時に外周を赤く塗る帯の幅（SWR-46）
constexpr int kTickInner  = 180;  ///< ゾーン境界マークの内側
constexpr int kScaleInner = 184;  ///< 目盛の内側

constexpr int kRingStartDeg = 135;  ///< 7 時半方向。角度は LVGL 準拠（0° = 3 時、時計回り）
constexpr int kRingSweepDeg = 270;

// 縦方向のバンド（垂直中心）
constexpr int kFreshnessY = 68;
constexpr int kUnitY      = 104;
constexpr int kMainY      = 208;
constexpr int kEgtY       = 336;
constexpr int kStatusY    = 404;
}  // namespace layout

}  // namespace cm::ui
