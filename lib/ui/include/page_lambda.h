// SWA-21: PAGE_LAMBDA（λ リング + 大数値 + EGT）  DOC-23 §3 / §4 / §5
#pragma once

#include "big_number.h"
#include "ipage.h"
#include "lambda_ring.h"
#include "units.h"

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
    void setDiagText(const char* text);

private:
    void applyMainFont(bool showAfr);
    void setText(lv_obj_t* label, char* last, size_t lastSize, const char* text);
    void setColor(lv_obj_t* label, uint32_t* last, uint32_t hex);

    lv_obj_t* m_root      = nullptr;
    lv_obj_t* m_freshness = nullptr;
    lv_obj_t* m_unit      = nullptr;
    BigNumber m_mainNum;
    BigNumber m_egtNum;
    lv_obj_t* m_status = nullptr;
    LambdaRing m_ring;

    bool m_showAfr = true;
    char m_diag[24]{};

    // SWR-24: 表示用の値は 1 次 IIR を通す。UI は 30 Hz で回り、CAN は約 20 Hz で届くので、
    // フィルタが間を補間して、毎フレーム値が動く（SYS-12 の最悪条件）。生値は SignalStore が保持している。
    Lpf1 m_lpfLambda;
    Lpf1 m_lpfEgt;
    bool m_lambdaSeeded     = false;
    bool m_egtSeeded        = false;
    uint32_t m_lastUpdateMs = 0;
    bool m_hasLastUpdate    = false;

    // 前回設定した内容。変わらないものを LVGL に渡さない（無効化 = 再描画を避ける）
    char m_lastFreshness[8]{};
    char m_lastUnit[8]{};
    char m_lastStatus[24]{};
    uint32_t m_lastStatusColor = 0xFFFFFFFF;
};

}  // namespace cm::ui
