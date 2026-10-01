// SWD-07: ページの共通インタフェース
//
// 契約（DOC-22 §7）:
//   - onCreate() は起動時に 1 回だけ。以降 LVGL オブジェクトの生成・破棄をしない（CLAUDE.md 規則 5）
//   - 非表示ページには onUpdate() を呼ばない
//   - LVGL のオブジェクトを外へ露出しない（30 fps を満たせない場合に 1 ページだけ描画方式を差し替えるため）
#pragma once

#include "config.h"
#include "input_event.h"
#include "lvgl.h"
#include "signal_store.h"

namespace cm::ui {

class IPage {
public:
    virtual ~IPage() = default;

    virtual const char* id() const                                 = 0;
    virtual void onCreate(lv_obj_t* parent, const Config& cfg)     = 0;
    virtual void onShow()                                          = 0;
    virtual void onHide()                                          = 0;
    virtual void onUpdate(const Snapshot& snap, const Config& cfg) = 0;
    /// true = 消費した（PageManager はページ切替として扱わない）
    virtual bool onEvent(InputEvent ev) = 0;
};

}  // namespace cm::ui
