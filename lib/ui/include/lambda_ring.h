// SWD-08 / SWA-22: λ リング（外周のバーグラフ）
//
// 【なぜ lv_arc を使わないか】DEC-02: 5 色ゾーン + 目盛 + 先端マーカーを 1 つの描画イベントで描く。
// 【なぜ差分無効化か】480x480 の RGB パネルは全面再描画で 16 fps（OPN-12 実測）。値の変化に応じて
// 「前回の角度から今回の角度までの扇形」だけを lv_obj_invalidate_area() し、定常時の描画量を
// 画面の数 % に抑える（DEC-05）。ゾーン色が変わった時だけ全体を再描画する。
//
// DEC-02 は lv_canvas を想定していたが、canvas は 450 KB のバッファを PSRAM に持ち、LVGL の描画のたびに
// PSRAM から SRAM へ読み戻すので、パネル DMA との帯域競合（RSK-14）を増やす。描画イベントでの直接描画は
// その往復が無く、差分無効化の効果は同じ。
#pragma once

#include <cstdint>

#include "config.h"
#include "lvgl.h"

namespace cm::ui {

class LambdaRing {
public:
    /// 画面全体（480x480）を覆う透明なオブジェクトを parent の下に作る。
    void create(lv_obj_t* parent, const Config& cfg);

    /// ゾーン境界・リング範囲が変わったとき（設定変更時）。目盛の位置を計算し直して全体を再描画する。
    void configure(const Config& cfg);

    /// 毎フレーム呼ぶ。変化が無ければ何もしない。
    /// @param ratio    0.0-1.0（ringRatio() でクランプ済みであること。範囲外でもここでクランプする）
    /// @param fillHex  塗り色（ゾーン色）0xRRGGBB
    /// @param valid    false なら塗りも先端マーカーも出さない（信号喪失。RSK-01）
    void set(float ratio, uint32_t fillHex, bool valid);

    /// EGT DANGER の外周警告帯（SWR-46）。
    void setAlarm(bool on);

    lv_obj_t* obj() const { return m_obj; }
    void setHidden(bool hidden);

private:
    static void onDraw(lv_event_t* e);
    void draw(lv_layer_t* layer, int32_t originX, int32_t originY) const;
    void invalidateSweep(float fromDeg, float toDeg);
    /// 環状部だけを角度ごとの小さな矩形に分けて無効化する。lv_obj_invalidate() は 480x480 全体になり、
    /// リングが変わるたびに全画面（約 78 ms）を描き直すことになる（OPN-12 の実測）
    void invalidateRing(float fromDeg, float toDeg, int rInner, int rOuter);
    void invalidateAll();
    static float effectiveSweepDeg(float ratio);

    static constexpr int kZoneTicks  = 4;
    static constexpr int kScaleTicks = 6;

    lv_obj_t* m_obj    = nullptr;
    float m_ratio      = 0.0f;
    uint32_t m_fillHex = 0;
    bool m_valid       = false;
    bool m_alarm       = false;
    float m_zoneRatio[kZoneTicks]{};
};

}  // namespace cm::ui
