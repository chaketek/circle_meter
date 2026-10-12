// SWD-12: 指針式デザイン A の針（DOC-23 §3.5 / DEC-10）
//
// 中心から浮かせた二等辺三角形（半径 kBaseR で幅 kBaseW、半径 kTipR で尖る）を lv_draw_triangle 1 枚で描く。
// 2 枚の三角形に分けると、共有する辺の両側にアンチエイリアスが掛かって継ぎ目が線として見えるため 1
// 枚にしている。
//
// 無効化は旧位置と新位置の針を長さ方向に kSegments 分割し、区間ごとに旧・新の外接矩形の和を出す
// （ring_geometry::needleSegmentBounds、UT-20）。斜めの針の外接矩形 1 つで無効化すると面積が倍以上になる。
#pragma once

#include <cstdint>

#include "lvgl.h"

namespace cm::ui {

class Needle {
public:
    static constexpr float kBaseR  = 110.0f;
    static constexpr float kTipR   = 224.0f;
    static constexpr float kBaseW  = 9.0f;
    static constexpr int kSegments = 6;

    void create(lv_obj_t* parent);

    /// @param angleDeg 針の角度（LVGL 準拠: 0° = 3 時、時計回り）
    /// @param visible  false なら針を出さない（信号喪失: RSK-01 / SWR-49）
    void set(float angleDeg, uint32_t colorHex, bool visible);

private:
    static void onDraw(lv_event_t* e);
    void invalidateNeedle(float angleDeg);
    void invalidateMove(float fromDeg, float toDeg);

    lv_obj_t* m_obj     = nullptr;
    float m_angleDeg    = 0.0f;
    uint32_t m_colorHex = 0;
    bool m_visible      = false;
};

}  // namespace cm::ui
