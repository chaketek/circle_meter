#include "lambda_ring.h"

#include <cmath>

#include "ring_geometry.h"
#include "theme.h"
#include "units.h"

namespace cm::ui {

namespace {

constexpr float kMinSweepDeg      = 2.0f;  // 下限未満でも最小限の塗りを残す（DOC-23 §3.1）
constexpr float kRedrawThreshDeg  = 0.2f;  // これ未満の変化は再描画しない（外周で約 0.8 px）
constexpr float kInvalidatePadDeg = 1.5f;  // 先端マーカー（幅 3 px）と anti-alias の滲み分
constexpr float kSectorStepDeg = 15.0f;  // 分割の粒度。LVGL の無効領域バッファは 32 個（LV_INV_BUF_SIZE）で、
                                         // 超えると 1 つの大きな矩形に併合されてしまうので 24 個以内に収める

lv_point_precise_t polar(int cx, int cy, float deg, int r) {
    const float rad = deg * ring_geometry::kPi / 180.0f;
    lv_point_precise_t p;
    p.x = cx + static_cast<int32_t>(std::lround(static_cast<float>(r) * std::cos(rad)));
    p.y = cy + static_cast<int32_t>(std::lround(static_cast<float>(r) * std::sin(rad)));
    return p;
}

void drawTick(lv_layer_t* layer, int cx, int cy, float deg, int rIn, int rOut, int width, uint32_t hex) {
    lv_draw_line_dsc_t d;
    lv_draw_line_dsc_init(&d);
    d.p1    = polar(cx, cy, deg, rIn);
    d.p2    = polar(cx, cy, deg, rOut);
    d.width = width;
    d.color = lv_color_hex(hex);
    d.opa   = LV_OPA_COVER;
    lv_draw_line(layer, &d);
}

}  // namespace

float LambdaRing::effectiveSweepDeg(float ratio) {
    const float s = ring_geometry::angleOfRatio(ratio, 0.0f, static_cast<float>(layout::kRingSweepDeg));
    return (s < kMinSweepDeg) ? kMinSweepDeg : s;
}

void LambdaRing::create(lv_obj_t* parent, const Config& cfg) {
    m_obj = lv_obj_create(parent);
    lv_obj_remove_style_all(m_obj);  // 背景・枠・影なし。描画は onDraw が全て行う
    lv_obj_set_pos(m_obj, 0, 0);
    lv_obj_set_size(m_obj, layout::kWidth, layout::kHeight);
    lv_obj_remove_flag(m_obj, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_add_event_cb(m_obj, &LambdaRing::onDraw, LV_EVENT_DRAW_MAIN, this);
    configure(cfg);
}

void LambdaRing::configure(const Config& cfg) {
    m_zoneRatio[0] = ringRatio(cfg.zones.richHeavyMax, cfg.ringLo, cfg.ringHi);
    m_zoneRatio[1] = ringRatio(cfg.zones.richMax, cfg.ringLo, cfg.ringHi);
    m_zoneRatio[2] = ringRatio(cfg.zones.optimalMax, cfg.ringLo, cfg.ringHi);
    m_zoneRatio[3] = ringRatio(cfg.zones.leanMax, cfg.ringLo, cfg.ringHi);
    invalidateAll();
}

void LambdaRing::setHidden(bool hidden) {
    if (hidden) {
        lv_obj_add_flag(m_obj, LV_OBJ_FLAG_HIDDEN);
    } else {
        lv_obj_remove_flag(m_obj, LV_OBJ_FLAG_HIDDEN);
    }
}

void LambdaRing::invalidateAll() {
    if (m_obj != nullptr) {
        lv_obj_invalidate(m_obj);
    }
}

void LambdaRing::invalidateSweep(float fromDeg, float toDeg) {
    const float lo = (fromDeg < toDeg) ? fromDeg : toDeg;
    const float hi = (fromDeg < toDeg) ? toDeg : fromDeg;
    invalidateRing(lo - kInvalidatePadDeg, hi + kInvalidatePadDeg, layout::kRingInner, layout::kRingOuter);
}

void LambdaRing::invalidateRing(float fromDeg, float toDeg, int rInner, int rOuter) {
    lv_area_t coords;
    lv_obj_get_coords(m_obj, &coords);
    // 角度範囲を kSectorStepDeg 刻みに割り、それぞれの外接矩形を無効化する
    const float span = toDeg - fromDeg;
    int steps        = static_cast<int>(std::ceil(span / kSectorStepDeg));
    steps            = (steps < 1) ? 1 : steps;
    const float step = span / static_cast<float>(steps);
    for (int i = 0; i < steps; ++i) {
        const IntRect r = ring_geometry::annularSectorBounds(layout::kCx, layout::kCy, rInner, rOuter,
                                                             fromDeg + step * static_cast<float>(i), step, 2);
        lv_area_t a;
        a.x1 = coords.x1 + r.x1;
        a.y1 = coords.y1 + r.y1;
        a.x2 = coords.x1 + r.x2;
        a.y2 = coords.y1 + r.y2;
        lv_obj_invalidate_area(m_obj, &a);
    }
}

void LambdaRing::set(float ratio, uint32_t fillHex, bool valid) {
    const float r = (ratio < 0.0f) ? 0.0f : (ratio > 1.0f ? 1.0f : ratio);

    // 色・有効性が変わると塗り全体が変わる（ゾーン境界を跨いだ時だけ）。変わるのは開始角から先端までの
    // 塗り部分と先端マーカーだけなので、その範囲の環状部を無効化する（全画面にはしない）
    if (valid != m_valid || fillHex != m_fillHex) {
        const float start  = static_cast<float>(layout::kRingStartDeg);
        const float oldTip = start + effectiveSweepDeg(m_ratio);
        const float newTip = start + effectiveSweepDeg(r);
        m_valid            = valid;
        m_fillHex          = fillHex;
        m_ratio            = r;
        invalidateRing(start - kInvalidatePadDeg, ((oldTip > newTip) ? oldTip : newTip) + kInvalidatePadDeg,
                       layout::kRingInner, layout::kRingOuter);
        return;
    }
    if (!valid) {
        return;
    }

    const float oldDeg = static_cast<float>(layout::kRingStartDeg) + effectiveSweepDeg(m_ratio);
    const float newDeg = static_cast<float>(layout::kRingStartDeg) + effectiveSweepDeg(r);
    if (std::fabs(newDeg - oldDeg) < kRedrawThreshDeg) {
        return;  // 前回描いた値のまま。m_ratio は更新しない（微小な変化が積もれば次に描く）
    }
    invalidateSweep(oldDeg, newDeg);
    m_ratio = r;
}

void LambdaRing::setAlarm(bool on) {
    if (on != m_alarm) {
        m_alarm = on;
        // 警告帯（半径 236-240）だけを、全周を分割して無効化する。外接矩形は全画面だが、帯の画素は約 1%
        invalidateRing(0.0f, 360.0f, layout::kRingOuter, layout::kRingOuter + layout::kAlarmBand + 1);
    }
}

void LambdaRing::onDraw(lv_event_t* e) {
    auto* self        = static_cast<LambdaRing*>(lv_event_get_user_data(e));
    lv_layer_t* layer = lv_event_get_layer(e);
    lv_area_t coords;
    lv_obj_get_coords(self->m_obj, &coords);
    self->draw(layer, coords.x1, coords.y1);
}

void LambdaRing::draw(lv_layer_t* layer, int32_t originX, int32_t originY) const {
    const int cx    = static_cast<int>(originX) + layout::kCx;
    const int cy    = static_cast<int>(originY) + layout::kCy;
    const int ringW = layout::kRingOuter - layout::kRingInner;

    lv_point_t center;
    center.x = cx;
    center.y = cy;

    // 軌道（未到達部も暗色で残す）
    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init(&arc);
    arc.center      = center;
    arc.radius      = layout::kRingOuter;
    arc.width       = ringW;
    arc.start_angle = layout::kRingStartDeg;
    arc.end_angle   = layout::kRingStartDeg + layout::kRingSweepDeg;  // 405° = 45°
    arc.color       = lv_color_hex(color::kTrack);
    arc.opa         = LV_OPA_COVER;
    arc.rounded     = 0;
    lv_draw_arc(layer, &arc);

    // 塗り（現在値のゾーン色の単色。グラデーションなし: DOC-23 §3.1）
    const float sweep = effectiveSweepDeg(m_ratio);
    if (m_valid) {
        arc.end_angle = layout::kRingStartDeg + static_cast<int32_t>(std::lround(sweep));
        arc.color     = lv_color_hex(m_fillHex);
        lv_draw_arc(layer, &arc);
    }

    // 目盛（短め）とゾーン境界（長め）
    const float start = static_cast<float>(layout::kRingStartDeg);
    for (int i = 0; i < kScaleTicks; ++i) {
        const float ratio = static_cast<float>(i) / static_cast<float>(kScaleTicks - 1);
        drawTick(layer, cx, cy, start + layout::kRingSweepDeg * ratio, layout::kScaleInner,
                 layout::kRingInner - 2, 2, color::kTextSub);
    }
    for (int i = 0; i < kZoneTicks; ++i) {
        drawTick(layer, cx, cy, start + layout::kRingSweepDeg * m_zoneRatio[i], layout::kTickInner,
                 layout::kRingInner - 2, 4, color::kText);
    }

    // 現在値マーカー（先端の白い線）
    if (m_valid) {
        drawTick(layer, cx, cy, start + sweep, layout::kRingInner, layout::kRingOuter, 3, color::kText);
    }

    // EGT DANGER の警告帯（SWR-46）。ベゼル余白 236-240 に描く
    if (m_alarm) {
        lv_draw_arc_dsc_t band;
        lv_draw_arc_dsc_init(&band);
        band.center      = center;
        band.radius      = layout::kRingOuter + layout::kAlarmBand - 1;
        band.width       = layout::kAlarmBand;
        band.start_angle = 0;
        band.end_angle   = 360;  // 全周（lv_draw_sw_arc が start + 360 == end を全円として扱う）
        band.color       = lv_color_hex(color::kDanger);
        band.opa         = LV_OPA_COVER;
        lv_draw_arc(layer, &band);
    }
}

}  // namespace cm::ui
