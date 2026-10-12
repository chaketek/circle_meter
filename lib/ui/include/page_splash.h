// SWA-21: PAGE_SPLASH（オープニング画面）  DOC-23 §7 / SYS-18 / SWR-48
//
// ロゴと版数を出すだけのページ。フェードイン/アウトは画素ではなくバックライトの PWM で行う
// （16 bit の階調ではアルファ合成が縞になるうえ CPU を食う。DOC-23 §7）ので、このクラスは表示を持たない。
#pragma once

#include "ipage.h"

namespace cm::ui {

class PageSplash : public IPage {
public:
    const char* id() const override { return "PAGE_SPLASH"; }
    void onCreate(lv_obj_t* parent, const Config& cfg) override;
    void onShow() override;
    void onHide() override;
    void onUpdate(const Snapshot&, const Config&) override {}
    bool onEvent(InputEvent) override { return true; }  // 自動遷移のみ。入力は無視する

    /// 版数文字列（"v0.1.0-3-gabc1234"）。onCreate より前に呼ぶ。参照を保持するので静的領域の文字列を渡す
    void setVersion(const char* version) { m_version = version; }

private:
    lv_obj_t* m_root      = nullptr;
    const char* m_version = "";
    lv_image_dsc_t m_logo{};
};

}  // namespace cm::ui
