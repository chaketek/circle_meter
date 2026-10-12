// SWD-13 / SWR-51 / SWR-68: 機種によらない表示文字列
//
// HW / LVGL 非依存（DEC-06）。LCD-2.1（LVGL）と M5Dial（M5GFX）の両方が同じ文言を出すため、ここに置く。
#pragma once

#include <cstddef>

#include "lambda_sensor_state.h"

namespace cm {

/// UTF-8 の度記号つきの温度単位（LVGL のフォントは '°' を収録している）
constexpr const char* kDegCUtf8 =
    "\xC2\xB0"
    "C";
/// ASCII だけのフォント（M5Dial の M5GFX 内蔵フォント）用の温度単位
constexpr const char* kDegCAscii = "C";

/// SWR-51 / DOC-23 §6.1: λ が無効なときに出す文字列。Ok なら空文字列。
/// buf は 32 バイト以上（最長 "SENSOR FAULT: UNDERHEAT" = 23 文字）。
/// degSuffix は温度の後ろに付ける単位（kDegCUtf8 / kDegCAscii）。
void formatSensorStatus(char* buf, size_t size, const LambdaSensorInfo& info,
                        const char* degSuffix = kDegCUtf8);

}  // namespace cm
