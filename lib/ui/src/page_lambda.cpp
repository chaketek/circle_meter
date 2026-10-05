#include "page_lambda.h"

#include <cstdio>
#include <cstring>

#include "display_policy.h"
#include "page_common.h"
#include "fonts/cm_fonts.h"
#include "theme.h"
#include "units.h"

namespace cm::ui {

namespace {

// BigNumber の幅は「等幅セルの総幅」以上にする。セルが領域の外へはみ出すと、無効化が領域に切り詰められて
// 端の数 px を描き残す。総幅 = 数字セル（その書体で最も広い数字）x 桁数 + 記号の送り幅（tools/make_font.py の
// 書体で計算）: AFR "20.0" = 318 px、λ "0.880" = 325 px、EGT "960°C" = 244 px。
constexpr int kMainWidthAfr    = 320;
constexpr int kMainWidthLambda = 328;
constexpr int kEgtWidth        = 248;

const char* freshnessText(Freshness f) {
    switch (f) {
        case Freshness::Fresh:
            return "OK";
        case Freshness::Stale:
            return "STALE";
        default:
            return "LOST";
    }
}

lv_obj_t* makeLabel(lv_obj_t* parent, const lv_font_t* font, uint32_t hex, int width, int centerY) {
    lv_obj_t* l = lv_label_create(parent);
    lv_obj_set_style_text_font(l, font, 0);
    lv_obj_set_style_text_color(l, lv_color_hex(hex), 0);
    lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
    lv_label_set_long_mode(l, LV_LABEL_LONG_CLIP);
    lv_obj_set_width(l, width);
    // 数字専用フォントは行の高さが字の高さに一致するので、垂直中心にそのまま合わせられる
    lv_obj_set_pos(l, layout::kCx - width / 2, centerY - lv_font_get_line_height(font) / 2);
    return l;
}

}  // namespace

void PageLambda::onCreate(lv_obj_t* parent, const Config& cfg) {
    m_root = lv_obj_create(parent);
    lv_obj_remove_style_all(m_root);
    lv_obj_set_pos(m_root, 0, 0);
    lv_obj_set_size(m_root, layout::kWidth, layout::kHeight);
    lv_obj_remove_flag(m_root, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));

    // リングを先に作る（Z 順で最背面）。はみ出した文字がリングの上に載り、リングに隠されない
    m_ring.create(m_root, cfg);

    m_showAfr   = cfg.showAfr;
    m_freshness = makeLabel(m_root, &lv_font_montserrat_24, color::kTextSub, 200, layout::kFreshnessY);
    m_unit      = makeLabel(m_root, &lv_font_montserrat_32, color::kTextSub, 240, layout::kUnitY);
    m_mainNum.create(m_root, m_showAfr ? &cm_font_afr : &cm_font_lambda,
                     m_showAfr ? kMainWidthAfr : kMainWidthLambda, layout::kMainY);
    m_egtNum.create(m_root, &cm_font_egt, kEgtWidth, layout::kEgtY);
    m_status = makeLabel(m_root, &lv_font_montserrat_24, color::kTextSub, 340, layout::kStatusY);

    // LVGL のラベルは作成直後に "Text" と表示する。空の保持配列を参照させて消しておく
    lv_label_set_text_static(m_freshness, m_lastFreshness);
    lv_label_set_text_static(m_unit, m_lastUnit);
    lv_label_set_text_static(m_status, m_lastStatus);
}

void PageLambda::onShow() {
    lv_obj_remove_flag(m_root, LV_OBJ_FLAG_HIDDEN);
}

void PageLambda::onHide() {
    lv_obj_add_flag(m_root, LV_OBJ_FLAG_HIDDEN);
}

bool PageLambda::onEvent(InputEvent) {
    return false;  // タッチ操作は InputDriver 実装後（SWD-06）
}

void PageLambda::setDiagText(const char* text) {
    std::snprintf(m_diag, sizeof(m_diag), "%s", text != nullptr ? text : "");
}

void PageLambda::applyMainFont(bool showAfr) {
    m_showAfr = showAfr;
    m_mainNum.setFont(showAfr ? &cm_font_afr : &cm_font_lambda, showAfr ? kMainWidthAfr : kMainWidthLambda);
}

void PageLambda::setText(lv_obj_t* label, char* last, size_t lastSize, const char* text) {
    if (std::strncmp(last, text, lastSize) == 0) {
        return;
    }
    // 切り詰め前提のコピー（呼び出し側のバッファは表示に必要な最大長より大きくしてある）
    size_t i = 0;
    for (; i + 1 < lastSize && text[i] != '\0'; ++i) {
        last[i] = text[i];
    }
    last[i] = '\0';
    lv_label_set_text_static(label, last);  // 保持している配列を参照させる。LVGL ヒープへ確保しない
}

void PageLambda::setColor(lv_obj_t* label, uint32_t* last, uint32_t hex) {
    if (*last == hex) {
        return;
    }
    *last = hex;
    lv_obj_set_style_text_color(label, lv_color_hex(hex), 0);
}

void PageLambda::onUpdate(const Snapshot& snap, const Config& cfg) {
    if (cfg.showAfr != m_showAfr) {
        applyMainFont(cfg.showAfr);
    }
    const DisplayValues v = m_filter.update(snap, cfg);  // Lost なら hasLambda = false（RSK-01）
    const bool blinkOn    = egtBlinkOn(snap.takenAtMs);
    char buf[16];

    // ---- λ リング
    const uint32_t ringHex = (v.hasLambda && !v.lambdaStale) ? zoneColorHex(v.zone) : color::kDisabled;
    m_ring.set(v.ringRatio, ringHex, v.hasLambda);

    // ---- 主数値・単位・鮮度
    formatMain(buf, sizeof(buf), v, cfg);
    m_mainNum.setText(buf, mainColorHex(v));
    setText(m_unit, m_lastUnit, sizeof(m_lastUnit), cfg.showAfr ? "AFR" : "LAMBDA");
    setText(m_freshness, m_lastFreshness, sizeof(m_lastFreshness), freshnessText(v.lambdaFr));

    // ---- EGT
    formatEgt(buf, sizeof(buf), v);
    m_egtNum.setText(buf, egtColorHex(v, blinkOn));
    m_ring.setAlarm(egtAlarmOn(v, blinkOn));

    // ---- 下端: λ が無効ならその理由（SWR-51: NO SIGNAL / 停止中 / 加熱中 / 故障）、なければ診断文字列
    if (!v.hasLambda && v.sensor.state != LambdaSensorState::Ok) {
        char st[32];
        formatSensorStatus(st, sizeof(st), v.sensor);
        setText(m_status, m_lastStatus, sizeof(m_lastStatus), st);
        setColor(m_status, &m_lastStatusColor, sensorStatusColorHex(v.sensor));
    } else {
        setText(m_status, m_lastStatus, sizeof(m_lastStatus), m_diag);
        setColor(m_status, &m_lastStatusColor, color::kTextSub);
    }
}

}  // namespace cm::ui
