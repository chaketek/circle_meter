// SWD-06: 入力イベント（HW / LVGL 非依存）
//
// タッチの座標・押下時間から決まる抽象イベント。ページは LVGL のイベントではなく、これを受け取る。
// 判定の実装（InputDriver）は後続フェーズ。型だけ先に置いて、ページの IPage::onEvent() の署名を固定する。
#pragma once

#include <cstdint>

namespace cm {

enum class InputEvent : uint8_t { None = 0, SwipeLeft, SwipeRight, Tap, LongPress };

}  // namespace cm
