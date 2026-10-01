#include "page_lambda.h"

#include <cstdio>
#include <cstring>

#include "display_policy.h"
#include "fonts/cm_fonts.h"
#include "theme.h"
#include "units.h"

namespace cm::ui {

namespace {

// UTF-8 の度記号。"\xB0C" と続けると 16 進エスケープが "B0C" と読まれるので、リテラルを分ける
#define CM_DEG "\xC2\xB0"

constexpr float kLambdaTauMs = 80.0f;   // SWR-24
constexpr float kEgtTauMs    = 200.0f;  // SWR-24

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
    m_lpfLambda.configure(kLambdaTauMs);
    m_lpfEgt.configure(kEgtTauMs);
    m_status = makeLabel(m_root, &lv_font_montserrat_24, color::kTextSub, 240, layout::kStatusY);

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
    char buf[16];

    const uint32_t dtMs = m_hasLastUpdate ? snap.takenAtMs - m_lastUpdateMs : 0;
    m_lastUpdateMs      = snap.takenAtMs;
    m_hasLastUpdate     = true;

    // ---- λ
    float lambda        = 0.0f;
    const bool hasL     = snap.get(SignalId::Lambda1, lambda);  // Lost なら false（RSK-01）
    const Freshness frL = snap.freshnessOf(SignalId::Lambda1);
    if (hasL) {
        // 喪失から復帰したときは古い値から緩やかに追従させない（Lpf1 の契約）。最初の 1 点で seed する
        if (!m_lambdaSeeded) {
            m_lpfLambda.reset(lambda);
            m_lambdaSeeded = true;
        }
        lambda = m_lpfLambda.update(lambda, dtMs);
    } else {
        m_lambdaSeeded = false;
    }
    const bool staleL     = (frL == Freshness::Stale);
    const LambdaZone zone = zoneOf(lambda, cfg.zones);

    uint32_t ringHex = color::kDisabled;
    if (hasL && !staleL) {
        ringHex = zoneColorHex(zone);
    }
    m_ring.set(ringRatio(lambda, cfg.ringLo, cfg.ringHi), ringHex, hasL);

    if (hasL) {
        if (cfg.showAfr) {
            std::snprintf(buf, sizeof(buf), "%.1f", lambdaToAfr(lambda, cfg.stoich));
        } else {
            std::snprintf(buf, sizeof(buf), "%.3f", lambda);
        }
    } else {
        std::snprintf(buf, sizeof(buf), "--");
    }

    uint32_t mainHex = color::kText;
    if (!hasL || staleL) {
        mainHex = color::kDisabled;
    } else if (zone == LambdaZone::LeanHeavy) {
        mainHex = color::kLeanHeavy;  // DOC-23 §3.3: 白。ただし LEAN_HEAVY のときはゾーン色
    }
    m_mainNum.setText(buf, mainHex);

    setText(m_unit, m_lastUnit, sizeof(m_lastUnit), cfg.showAfr ? "AFR" : "LAMBDA");
    setText(m_freshness, m_lastFreshness, sizeof(m_lastFreshness), freshnessText(frL));

    // ---- EGT
    float egtC      = 0.0f;
    const bool hasE = snap.get(SignalId::Egt1, egtC);
    if (hasE) {
        if (!m_egtSeeded) {
            m_lpfEgt.reset(egtC);
            m_egtSeeded = true;
        }
        egtC = m_lpfEgt.update(egtC, dtMs);
    } else {
        m_egtSeeded = false;
    }
    const bool staleE  = (snap.freshnessOf(SignalId::Egt1) == Freshness::Stale);
    const EgtLevel lvl = hasE ? levelOf(egtC, cfg.egt) : EgtLevel::Normal;
    const bool danger  = hasE && !staleE && lvl == EgtLevel::Danger;
    const bool blinkOn = egtBlinkOn(snap.takenAtMs);

    if (hasE) {
        std::snprintf(buf, sizeof(buf), "%d" CM_DEG "C", static_cast<int>(egtC + 0.5f));
    } else {
        std::snprintf(buf, sizeof(buf), "--" CM_DEG "C");
    }

    uint32_t egtHex = color::kEgtNormal;
    if (!hasE || staleE) {
        egtHex = color::kDisabled;
    } else if (lvl == EgtLevel::Danger) {
        egtHex = blinkOn ? color::kDanger : 0x601010;  // 2 Hz 点滅（SWR-46）
    } else if (lvl == EgtLevel::Warn) {
        egtHex = color::kWarn;
    }
    m_egtNum.setText(buf, egtHex);
    m_ring.setAlarm(danger && blinkOn);

    // ---- 下端: NO SIGNAL が最優先、なければ診断文字列
    if (frL == Freshness::Lost) {
        setText(m_status, m_lastStatus, sizeof(m_lastStatus), "NO SIGNAL");
        setColor(m_status, &m_lastStatusColor, color::kDanger);
    } else {
        setText(m_status, m_lastStatus, sizeof(m_lastStatus), m_diag);
        setColor(m_status, &m_lastStatusColor, color::kTextSub);
    }
}

}  // namespace cm::ui
