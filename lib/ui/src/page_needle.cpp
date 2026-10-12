#include "page_needle.h"

#include <cstdio>
#include <cstring>

#include "display_policy.h"
#include "fonts/cm_fonts.h"
#include "page_common.h"
#include "theme.h"

namespace cm::ui {

namespace {

// DOC-23 §3.5
constexpr uint32_t kNeedleHex = 0xFF7014;
constexpr int kMainY          = 226;
constexpr int kEgtY           = 300;
// λ センサの状態は主数値と EGT の間（DOC-23 §6.1）。以前の (240, 362) は左下の目盛り数字「10」と重なった
constexpr int kStatusY = 262;
constexpr int kDiagY   = 452;
// BigNumber の幅は等幅セルの総幅以上（SWD-10）。B612 Mono 70 px の数字セルは 46 px:
// λ "1.000" = 46 x 4 + 0.62 x 46 = 213 px。EGT 34 px の数字セルは 22 px: "960°C" = 22 x 4 + 0.70 x 22 = 103
// px
constexpr int kMainWidth  = 224;
constexpr int kEgtWidth   = 120;
constexpr float kDotScale = 0.62f;  // SWR-50: 等幅書体の '.' と '°' だけ詰める
constexpr float kDegScale = 0.70f;

lv_obj_t* makeLabel(lv_obj_t* parent, const lv_font_t* font, uint32_t hex, int width, int centerY) {
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(hex), 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(l, width);
    lv_obj_set_pos(l, layout::kCx - width / 2, centerY - lv_font_get_line_height(font) / 2);
    return l;
}

}  // namespace

void PageNeedle::onCreate(lv_obj_t* parent, const Config& cfg) {
    m_root = lv_obj_create(parent);
    lv_obj_remove_style_all(m_root);
    lv_obj_set_pos(m_root, 0, 0);
    lv_obj_set_size(m_root, layout::kWidth, layout::kHeight);
    lv_obj_set_style_bg_color(m_root, lv_color_hex(0x060606), 0);  // DOC-23 §3.5 の背景
    lv_obj_set_style_bg_opa(m_root, LV_OPA_COVER, 0);
    lv_obj_remove_flag(m_root, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));

    // Z 順: 文字盤 -> 針 -> 数値（針は中心から浮かせてあり、数値とは重ならない）
    m_face.create(m_root, cfg);
    m_needle.create(m_root);
    m_mainNum.create(m_root, &cm_font_b612_main, kMainWidth, kMainY);
    m_mainNum.setCellScale('.', kDotScale);
    m_egtNum.create(m_root, &cm_font_b612_egt, kEgtWidth, kEgtY);
    m_egtNum.setCellScale(0xB0, kDegScale);  // '°'

    m_status = makeLabel(m_root, &cm_font_b612_label, color::kDanger, 300, kStatusY);
    m_diag   = makeLabel(m_root, &cm_font_b612_small, 0x6A6A6A, 200, kDiagY);
    lv_label_set_text_static(m_status, m_lastStatus);  // 作成直後の "Text" を消す
    lv_label_set_text_static(m_diag, m_lastDiag);
}

void PageNeedle::onShow() {
    lv_obj_remove_flag(m_root, LV_OBJ_FLAG_HIDDEN);
}

void PageNeedle::onHide() {
    lv_obj_add_flag(m_root, LV_OBJ_FLAG_HIDDEN);
}

void PageNeedle::setLabel(lv_obj_t* label, char* last, size_t size, const char* text) {
    if (std::strncmp(last, text, size) == 0) {
        return;
    }
    std::snprintf(last, size, "%s", text);
    lv_label_set_text_static(label, last);
}

void PageNeedle::setDiagText(const char* text) {
    setLabel(m_diag, m_lastDiag, sizeof(m_lastDiag), text != nullptr ? text : "");
}

void PageNeedle::onUpdate(const Snapshot& snap, const Config& cfg) {
    const DisplayValues v = m_filter.update(snap, cfg);  // Lost なら hasLambda = false（RSK-01）
    const bool blinkOn    = egtBlinkOn(snap.takenAtMs);
    char buf[16];

    // 針: Lost では出さない（古い位置を指したままにしない。SWR-49）。Stale は灰色
    m_needle.set(m_face.angleOfLambda(v.lambda), v.lambdaStale ? color::kDisabled : kNeedleHex, v.hasLambda);

    formatMain(buf, sizeof(buf), v, cfg);
    m_mainNum.setText(buf, mainColorHex(v));
    formatEgt(buf, sizeof(buf), v);
    m_egtNum.setText(buf, egtColorHex(v, blinkOn));
    m_face.setAlarm(egtAlarmOn(v, blinkOn));

    // SWR-51: λ が無効ならその理由（NO SIGNAL / 停止中 / 加熱中 / 故障）
    char st[32] = "";
    if (!v.hasLambda && v.sensor.state != LambdaSensorState::Ok) {
        formatSensorStatus(st, sizeof(st), v.sensor);
    }
    const uint32_t stHex = sensorStatusColorHex(v.sensor);
    if (stHex != m_lastStatusHex) {
        m_lastStatusHex = stHex;
        lv_obj_set_style_text_color(m_status, lv_color_hex(stHex), 0);
    }
    setLabel(m_status, m_lastStatus, sizeof(m_lastStatus), st);
}

}  // namespace cm::ui
