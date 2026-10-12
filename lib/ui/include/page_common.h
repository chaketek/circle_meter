// SWA-21: λ ページの全デザインで共通の表示規則（DOC-23 §3.3 / §4）
//
// 数値の書式・色と EGT の警告色を 1 か所に置き、デザインによって規則が食い違わないようにする。
#pragma once

#include <cstddef>
#include <cstdint>

#include "config.h"
#include "display_filter.h"

namespace cm::ui {

/// 主数値の文字列。AFR は小数 1 桁、λ は小数 3 桁、信号喪失なら "--"。
void formatMain(char* buf, size_t size, const DisplayValues& v, const Config& cfg);

/// 主数値の色。通常は白、LEAN_HEAVY は赤、Stale / Lost は灰。
uint32_t mainColorHex(const DisplayValues& v);

/// EGT の文字列（"845°C" / "--°C"）。
void formatEgt(char* buf, size_t size, const DisplayValues& v);

/// EGT の色。警告は黄、危険は赤の 2 Hz 点滅（SWR-46）、Stale / Lost は灰。
uint32_t egtColorHex(const DisplayValues& v, bool blinkOn);

/// SWR-51 / DOC-23 §6.1: λ が無効なときに出す文字列。Ok なら空文字列。
/// buf は 32 バイト以上（最長 "SENSOR FAULT: UNDERHEAT" = 23 文字）。
void formatSensorStatus(char* buf, size_t size, const LambdaSensorInfo& info);

/// formatSensorStatus の文字列の色。
uint32_t sensorStatusColorHex(const LambdaSensorInfo& info);

/// EGT 危険の警告帯を今このフレームで点灯させるか。
bool egtAlarmOn(const DisplayValues& v, bool blinkOn);

}  // namespace cm::ui
