// DOC-23 §4.1 / §10: 表示に関する判断（HW / LVGL 非依存）
//
// 描画コードに閾値や周期を埋め込むと PC でテストできないので、判断だけをここに集める。
#pragma once

#include <cstdint>

namespace cm {

/// SYS-20 / DOC-23 §10: 輝度レベル 1-5 をバックライトのデューティ [%] にする。範囲外はクランプ。
constexpr uint8_t brightnessPercent(uint8_t level) {
    constexpr uint8_t kTable[5] = {10, 25, 50, 75, 100};
    if (level < 1) {
        level = 1;
    }
    if (level > 5) {
        level = 5;
    }
    return kTable[level - 1];
}

/// SWR-46 / DOC-23 §4.1: EGT DANGER の点滅は 2 Hz（250 ms 点灯 / 250 ms 消灯）。
/// 位相を時刻から決めるので、状態を持たず、描画が遅れても周期がずれない。
constexpr bool egtBlinkOn(uint32_t nowMs) {
    return ((nowMs / 250u) & 1u) == 0u;
}

}  // namespace cm
