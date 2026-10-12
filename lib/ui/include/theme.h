// SWA-23: 配色・レイアウト定数（DOC-23 §2 / §8）
//
// 座標は 480x480 の絶対値。240x240（M5Dial）時代の寸法をちょうど 2 倍にして導出した（DOC-23 §2）。
// 画面の見え方を変える変更は、必ず DOC-23 を先に更新すること。
#pragma once

#include <cstdint>

#include "lvgl.h"
#include "theme_colors.h"  // 配色は LVGL に依存しないので両機種で共有する（SWR-68）
#include "units.h"

namespace cm::ui {

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
