// SWA-21 / SWD-12: λ ページの指針式デザイン A（大森風）  DOC-23 §3.5 / SYS-21 / SWR-49
#pragma once

#include "big_number.h"
#include "dial_face.h"
#include "display_filter.h"
#include "ipage.h"
#include "needle.h"

namespace cm::ui {

class PageNeedle : public IPage {
public:
    const char* id() const override { return "PAGE_LAMBDA_NEEDLE_A"; }
    void onCreate(lv_obj_t* parent, const Config& cfg) override;
    void onShow() override;
    void onHide() override;
    void onUpdate(const Snapshot& snap, const Config& cfg) override;
    bool onEvent(InputEvent) override { return false; }
    void setDiagText(const char* text) override;

private:
    void setLabel(lv_obj_t* label, char* last, size_t size, const char* text);

    lv_obj_t* m_root = nullptr;
    DialFace m_face;
    Needle m_needle;
    BigNumber m_mainNum;
    BigNumber m_egtNum;
    lv_obj_t* m_status = nullptr;  // NO SIGNAL
    lv_obj_t* m_diag   = nullptr;  // デモの fps など
    DisplayFilter m_filter;

    char m_lastStatus[12]{};
    char m_lastDiag[24]{};
};

}  // namespace cm::ui
