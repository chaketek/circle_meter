// SWD-12: 指針式デザイン A の文字盤（DOC-23 §3.5）
//
// リム・5 色の色帯・目盛り・14.7 の三角・目盛り数字・銘板を、1 つの描画イベントで描く（DEC-10）。
// 静的なので、自分から無効化するのは EGT DANGER のリム点滅（SWR-46）と設定変更のときだけ。
// 針が動いたときは、針の通った小さな矩形の中だけがこの描画イベントで描き直される（LVGL がクリップする）。
#pragma once

#include <cstdint>

#include "config.h"
#include "lvgl.h"

namespace cm::ui {

class DialFace {
public:
    void create(lv_obj_t* parent, const Config& cfg);
    /// ゾーン境界・レンジ・理論空燃比が変わったとき。全体を描き直す。
    void configure(const Config& cfg);
    /// EGT DANGER の 2 Hz 点滅（SWR-46）。リムだけを分割して無効化する。
    void setAlarm(bool on);

    /// λ（0.0-1.0 の比率ではなく値）を角度 [度] にする。針と目盛りで同じ変換を使う。
    float angleOfLambda(float lambda) const;

private:
    static void onDraw(lv_event_t* e);
    void draw(lv_layer_t* layer, int32_t originX, int32_t originY) const;

    lv_obj_t* m_obj = nullptr;
    Config m_cfg{};
    bool m_alarm = false;
};

}  // namespace cm::ui
