#include "needle.h"

#include <cmath>

#include "ring_geometry.h"
#include "theme.h"

namespace cm::ui {

namespace {

constexpr float kRedrawThreshDeg = 0.2f;  // これ未満の変化は描き直さない（先端で約 0.8 px）

lv_area_t toArea(const lv_area_t& coords, const IntRect& r) {
    lv_area_t a;
    a.x1 = coords.x1 + r.x1;
    a.y1 = coords.y1 + r.y1;
    a.x2 = coords.x1 + r.x2;
    a.y2 = coords.y1 + r.y2;
    return a;
}

IntRect segment(float deg, int k) {
    return ring_geometry::needleSegmentBounds(layout::kCx, layout::kCy, deg, Needle::kBaseR, Needle::kTipR,
                                              Needle::kBaseW, k, Needle::kSegments, 2);
}

}  // namespace

void Needle::create(lv_obj_t* parent) {
    m_obj = lv_obj_create(parent);
    lv_obj_remove_style_all(m_obj);
    lv_obj_set_pos(m_obj, 0, 0);
    lv_obj_set_size(m_obj, layout::kWidth, layout::kHeight);
    lv_obj_remove_flag(m_obj, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_add_event_cb(m_obj, &Needle::onDraw, LV_EVENT_DRAW_MAIN, this);
}

void Needle::invalidateNeedle(float angleDeg) {
    lv_area_t coords;
    lv_obj_get_coords(m_obj, &coords);
    for (int k = 0; k < kSegments; ++k) {
        const lv_area_t a = toArea(coords, segment(angleDeg, k));
        lv_obj_invalidate_area(m_obj, &a);
    }
}

void Needle::invalidateMove(float fromDeg, float toDeg) {
    lv_area_t coords;
    lv_obj_get_coords(m_obj, &coords);
    for (int k = 0; k < kSegments; ++k) {
        const lv_area_t a = toArea(coords, ring_geometry::unionOf(segment(fromDeg, k), segment(toDeg, k)));
        lv_obj_invalidate_area(m_obj, &a);
    }
}

void Needle::set(float angleDeg, uint32_t colorHex, bool visible) {
    if (visible != m_visible || colorHex != m_colorHex) {
        // 出す / 消す / 色が変わる: 旧位置と新位置の両方を描き直す
        if (m_visible) {
            invalidateNeedle(m_angleDeg);
        }
        if (visible) {
            invalidateNeedle(angleDeg);
        }
        m_visible  = visible;
        m_colorHex = colorHex;
        m_angleDeg = angleDeg;
        return;
    }
    if (!visible || std::fabs(angleDeg - m_angleDeg) < kRedrawThreshDeg) {
        return;  // 前回描いた位置のまま（微小な変化は積もってから描く）
    }
    invalidateMove(m_angleDeg, angleDeg);
    m_angleDeg = angleDeg;
}

void Needle::onDraw(lv_event_t* e) {
    auto* self = static_cast<Needle*>(lv_event_get_user_data(e));
    if (!self->m_visible) {
        return;
    }
    lv_area_t coords;
    lv_obj_get_coords(self->m_obj, &coords);
    const float cx  = static_cast<float>(coords.x1 + layout::kCx);
    const float cy  = static_cast<float>(coords.y1 + layout::kCy);
    const float rad = self->m_angleDeg * ring_geometry::kPi / 180.0f;
    const float dx = std::cos(rad), dy = std::sin(rad);
    const float nx = -dy, ny = dx;
    const float hw = 0.5f * kBaseW;

    lv_draw_triangle_dsc_t tri;
    lv_draw_triangle_dsc_init(&tri);
    tri.bg_color = lv_color_hex(self->m_colorHex);
    tri.bg_opa   = LV_OPA_COVER;
    tri.p[0].x   = static_cast<int32_t>(std::lround(cx + kTipR * dx));
    tri.p[0].y   = static_cast<int32_t>(std::lround(cy + kTipR * dy));
    tri.p[1].x   = static_cast<int32_t>(std::lround(cx + kBaseR * dx + hw * nx));
    tri.p[1].y   = static_cast<int32_t>(std::lround(cy + kBaseR * dy + hw * ny));
    tri.p[2].x   = static_cast<int32_t>(std::lround(cx + kBaseR * dx - hw * nx));
    tri.p[2].y   = static_cast<int32_t>(std::lround(cy + kBaseR * dy - hw * ny));
    lv_draw_triangle(lv_event_get_layer(e), &tri);
}

}  // namespace cm::ui
