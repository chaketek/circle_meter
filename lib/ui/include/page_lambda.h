// SWA-21: PAGE_LAMBDA（λ リング + 大数値 + EGT）  DOC-23 §3 / §4 / §5
#pragma once

#include "big_number.h"
#include "ipage.h"
#include "lambda_ring.h"
#include "display_filter.h"

namespace cm::ui {

class PageLambda : public IPage {
public:
    const char* id() const override { return "PAGE_LAMBDA"; }
    void onCreate(lv_obj_t* parent, const Config& cfg) override;
    void onShow() override;
    void onHide() override;
    void onUpdate(const Snapshot& snap, const Config& cfg) override;
    bool onEvent(InputEvent ev) override;

    /// 下端に出す診断文字列（fps など）。空なら何も出さない。NO SIGNAL のほうが優先される。
    void setDiagText(const char* text) override;

private:
    void applyMainFont(bool showAfr);
    void setText(lv_obj_t* label, char* last, size_t lastSize, const char* text);
    void setColor(lv_obj_t* label, uint32_t* last, uint32_t hex);

    lv_obj_t* m_root      = nullptr;
    lv_obj_t* m_freshness = nullptr;
    lv_obj_t* m_unit      = nullptr;
    BigNumber m_mainNum;
    BigNumber m_egtNum;
    lv_obj_t* m_status = nullptr;  // 下部: 診断文字列
    lv_obj_t* m_sensor = nullptr;  // 中央: λ センサの状態（SWR-51）
    LambdaRing m_ring;

    bool m_showAfr = true;
    char m_diag[24]{};

    // SWR-24 / RSK-01: 値の取り出し・LPF・鮮度の判定は全デザインで共通（DisplayFilter）
    DisplayFilter m_filter;

    // 前回設定した内容。変わらないものを LVGL に渡さない（無効化 = 再描画を避ける）
    char m_lastFreshness[8]{};
    char m_lastUnit[8]{};
    char m_lastStatus[32]{};
    uint32_t m_lastStatusColor = 0xFFFFFFFF;
    char m_lastSensor[32]{};
    uint32_t m_lastSensorColor = 0xFFFFFFFF;
};

}  // namespace cm::ui
