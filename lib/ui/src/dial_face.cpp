#include "dial_face.h"

#include <cmath>
#include <cstdio>

#include "fonts/cm_fonts.h"
#include "ring_geometry.h"
#include "theme.h"
#include "units.h"

namespace cm::ui {

namespace {

// DOC-23 §3.5 の寸法
constexpr int kRimR          = 239;
constexpr int kRimW          = 3;
constexpr int kBandR         = 225;
constexpr int kBandW         = 10;
constexpr int kTickOut       = 213;
constexpr int kTickMajorIn   = 192;
constexpr int kTickMinorIn   = 202;
constexpr int kNumeralR      = 160;
constexpr int kMarkApexR     = 187;
constexpr int kMarkBaseR     = 177;
constexpr float kMarkHalfDeg = 2.4f;

constexpr uint32_t kRimHex   = 0x787878;
constexpr uint32_t kTickHex  = 0xEBEBEB;
constexpr uint32_t kNumHex   = 0xF0F0F0;
constexpr uint32_t kLabelHex = 0xC8C8C8;
constexpr uint32_t kSmallHex = 0x8C8C8C;

constexpr float kSectorStepDeg = 15.0f;  // LambdaRing と同じ理由で 24 分割以内（DEC-08）

// 目盛り数字の文字列。描画タスクは後で実行されるので、静的領域に置く
char g_numText[32][4];

lv_point_precise_t polar(int cx, int cy, float deg, float r) {
    const float rad = deg * ring_geometry::kPi / 180.0f;
    lv_point_precise_t p;
    p.x = cx + static_cast<int32_t>(std::lround(r * std::cos(rad)));
    p.y = cy + static_cast<int32_t>(std::lround(r * std::sin(rad)));
    return p;
}

// 描き直す領域（clip）と、要素の外接矩形が重なるか。重ならない要素は描画指示を作らない（DEC-10）。
// 針が動くと小さな矩形が 1 フレームに 5-9 個できるが、そのたびに文字盤の約 40 要素の描画指示を作ると
// 画素数が少なくても 20 ms 以上かかった（実測）。指示の生成と振り分けのコストが支配的だったため。
bool overlaps(const lv_area_t& clip, int32_t x1, int32_t y1, int32_t x2, int32_t y2) {
    return !(x2 < clip.x1 || x1 > clip.x2 || y2 < clip.y1 || y1 > clip.y2);
}

/// clip の中心からの最短・最長距離。円環（半径 rIn-rOut）と重なるかの判定に使う。
bool overlapsAnnulus(const lv_area_t& clip, int cx, int cy, int rIn, int rOut) {
    const int32_t nx = (cx < clip.x1) ? clip.x1 : (cx > clip.x2 ? clip.x2 : cx);
    const int32_t ny = (cy < clip.y1) ? clip.y1 : (cy > clip.y2 ? clip.y2 : cy);
    const int64_t dmin =
        static_cast<int64_t>(nx - cx) * (nx - cx) + static_cast<int64_t>(ny - cy) * (ny - cy);
    const int32_t fx = (cx - clip.x1 > clip.x2 - cx) ? clip.x1 : clip.x2;
    const int32_t fy = (cy - clip.y1 > clip.y2 - cy) ? clip.y1 : clip.y2;
    const int64_t dmax =
        static_cast<int64_t>(fx - cx) * (fx - cx) + static_cast<int64_t>(fy - cy) * (fy - cy);
    return dmin <= static_cast<int64_t>(rOut) * rOut && dmax >= static_cast<int64_t>(rIn) * rIn;
}

void drawText(lv_layer_t* layer, const char* text, const lv_font_t* font, uint32_t hex, int x, int y,
              int halfW) {
    const int lh = lv_font_get_line_height(font);
    if (!overlaps(layer->_clip_area, x - halfW, y - lh / 2, x + halfW, y + lh / 2)) {
        return;
    }
    lv_draw_label_dsc_t d;
    lv_draw_label_dsc_init(&d);
    d.text       = text;
    d.text_local = 0;
    d.font       = font;
    d.color      = lv_color_hex(hex);
    d.opa        = LV_OPA_COVER;
    d.align      = LV_TEXT_ALIGN_CENTER;
    lv_area_t a;
    a.x1 = x - halfW;
    a.x2 = x + halfW;
    a.y1 = y - lh / 2;
    a.y2 = a.y1 + lh - 1;
    lv_draw_label(layer, &d, &a);
}

}  // namespace

void DialFace::create(lv_obj_t* parent, const Config& cfg) {
    m_obj = lv_obj_create(parent);
    lv_obj_remove_style_all(m_obj);
    lv_obj_set_pos(m_obj, 0, 0);
    lv_obj_set_size(m_obj, layout::kWidth, layout::kHeight);
    lv_obj_remove_flag(m_obj, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_add_event_cb(m_obj, &DialFace::onDraw, LV_EVENT_DRAW_MAIN, this);
    for (int i = 0; i < 32; ++i) {
        std::snprintf(g_numText[i], sizeof(g_numText[i]), "%d", i);
    }
    configure(cfg);
}

void DialFace::configure(const Config& cfg) {
    m_cfg = cfg;
    if (m_obj != nullptr) {
        lv_obj_invalidate(m_obj);
    }
}

float DialFace::angleOfLambda(float lambda) const {
    return ring_geometry::angleOfRatio(ringRatio(lambda, m_cfg.ringLo, m_cfg.ringHi),
                                       static_cast<float>(layout::kRingStartDeg),
                                       static_cast<float>(layout::kRingSweepDeg));
}

void DialFace::setAlarm(bool on) {
    if (on == m_alarm) {
        return;
    }
    m_alarm = on;
    lv_area_t coords;
    lv_obj_get_coords(m_obj, &coords);
    // リム（半径 236-239）だけを全周 24 個の矩形に分けて無効化する。lv_obj_invalidate() は全画面になる
    for (int i = 0; i < 24; ++i) {
        const IntRect r =
            ring_geometry::annularSectorBounds(layout::kCx, layout::kCy, kRimR - kRimW, kRimR + 1,
                                               kSectorStepDeg * static_cast<float>(i), kSectorStepDeg, 2);
        lv_area_t a;
        a.x1 = coords.x1 + r.x1;
        a.y1 = coords.y1 + r.y1;
        a.x2 = coords.x1 + r.x2;
        a.y2 = coords.y1 + r.y2;
        lv_obj_invalidate_area(m_obj, &a);
    }
}

void DialFace::onDraw(lv_event_t* e) {
    auto* self = static_cast<DialFace*>(lv_event_get_user_data(e));
    lv_area_t coords;
    lv_obj_get_coords(self->m_obj, &coords);
    self->draw(lv_event_get_layer(e), coords.x1, coords.y1);
}

void DialFace::draw(lv_layer_t* layer, int32_t originX, int32_t originY) const {
    const int cx          = static_cast<int>(originX) + layout::kCx;
    const int cy          = static_cast<int>(originY) + layout::kCy;
    const lv_area_t& clip = layer->_clip_area;
    lv_point_t center;
    center.x = cx;
    center.y = cy;

    // リム（EGT DANGER のときは赤。SWR-46）
    lv_draw_arc_dsc_t arc;
    lv_draw_arc_dsc_init(&arc);
    arc.center      = center;
    arc.radius      = kRimR;
    arc.width       = kRimW;
    arc.start_angle = 0;
    arc.end_angle   = 360;
    arc.color       = lv_color_hex(m_alarm ? color::kDanger : kRimHex);
    arc.opa         = LV_OPA_COVER;
    if (overlapsAnnulus(clip, cx, cy, kRimR - kRimW, kRimR)) {
        lv_draw_arc(layer, &arc);
    }

    // 色帯: 5 ゾーン（DOC-23 §3.2）
    const float bounds[6]     = {m_cfg.ringLo,           m_cfg.zones.richHeavyMax, m_cfg.zones.richMax,
                                 m_cfg.zones.optimalMax, m_cfg.zones.leanMax,      m_cfg.ringHi};
    const LambdaZone zones[5] = {LambdaZone::RichHeavy, LambdaZone::Rich, LambdaZone::Optimal,
                                 LambdaZone::Lean, LambdaZone::LeanHeavy};
    arc.radius                = kBandR;
    arc.width                 = kBandW;
    for (int i = 0; i < 5; ++i) {
        arc.start_angle = static_cast<int32_t>(std::lround(angleOfLambda(bounds[i])));
        arc.end_angle   = static_cast<int32_t>(std::lround(angleOfLambda(bounds[i + 1])));
        arc.color       = lv_color_hex(zoneColorHex(zones[i]));
        if (arc.end_angle > arc.start_angle && overlapsAnnulus(clip, cx, cy, kBandR - kBandW, kBandR)) {
            lv_draw_arc(layer, &arc);
        }
    }

    // 目盛り: AFR 0.5 ごと。整数は長く太い
    const float stoich = m_cfg.stoich;
    const float afrLo  = m_cfg.ringLo * stoich;
    const float afrHi  = m_cfg.ringHi * stoich;
    lv_draw_line_dsc_t line;
    lv_draw_line_dsc_init(&line);
    line.color = lv_color_hex(kTickHex);
    line.opa   = LV_OPA_COVER;
    for (int half = static_cast<int>(std::ceil(afrLo * 2.0f - 1e-3f));
         half <= static_cast<int>(afrHi * 2.0f + 1e-3f); ++half) {
        const float afr   = static_cast<float>(half) * 0.5f;
        const bool major  = (half % 2) == 0;
        const float deg   = angleOfLambda(afr / stoich);
        line.p1           = polar(cx, cy, deg, static_cast<float>(major ? kTickMajorIn : kTickMinorIn));
        line.p2           = polar(cx, cy, deg, static_cast<float>(kTickOut));
        line.width        = major ? 4 : 2;
        const int32_t lx1 = (line.p1.x < line.p2.x ? line.p1.x : line.p2.x) - 3;
        const int32_t lx2 = (line.p1.x > line.p2.x ? line.p1.x : line.p2.x) + 3;
        const int32_t ly1 = (line.p1.y < line.p2.y ? line.p1.y : line.p2.y) - 3;
        const int32_t ly2 = (line.p1.y > line.p2.y ? line.p1.y : line.p2.y) + 3;
        if (overlaps(clip, lx1, ly1, lx2, ly2)) {
            lv_draw_line(layer, &line);
        }
        // 偶数の AFR に数字（λ 表示のときも AFR で刻む。DOC-23 §3.5）
        const int afrInt = half / 2;
        if (major && (afrInt % 2) == 0 && afrInt >= 0 && afrInt < 32) {
            const lv_point_precise_t p = polar(cx, cy, deg, static_cast<float>(kNumeralR));
            drawText(layer, g_numText[afrInt], &cm_font_b612_scale, kNumHex, p.x, p.y, 24);
        }
    }

    // 理論空燃比の三角（λ = 1.0）
    const float degStoich = angleOfLambda(1.0f);
    lv_draw_triangle_dsc_t tri;
    lv_draw_triangle_dsc_init(&tri);
    tri.bg_color = lv_color_hex(kTickHex);
    tri.bg_opa   = LV_OPA_COVER;
    tri.p[0]     = polar(cx, cy, degStoich, static_cast<float>(kMarkApexR));
    tri.p[1]     = polar(cx, cy, degStoich - kMarkHalfDeg, static_cast<float>(kMarkBaseR));
    tri.p[2]     = polar(cx, cy, degStoich + kMarkHalfDeg, static_cast<float>(kMarkBaseR));
    if (overlapsAnnulus(clip, cx, cy, kMarkBaseR - 2, kMarkApexR + 2)) {
        lv_draw_triangle(layer, &tri);
    }

    // 銘板（針が通らない下部の空きと中心付近だけ）
    drawText(layer, "EGT", &cm_font_b612_small, 0x828282, cx, cy + 92, 30);
    drawText(layer, "A/F", &cm_font_b612_label, kLabelHex, cx, cy + 156, 40);
    drawText(layer, "AIR FUEL RATIO", &cm_font_b612_small, kSmallHex, cx, cy + 184, 70);
}

}  // namespace cm::ui
