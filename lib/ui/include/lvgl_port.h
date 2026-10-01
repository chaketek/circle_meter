// SWA-20: LVGL の初期化・flush・計測
//
// 描画バッファは内蔵 SRAM の静的配列 1 面（DEC-05）。LVGL は部分描画モードで、無効領域を
// バッファの高さごとに区切って描き、flush で DisplayHal（= PSRAM のフレームバッファへの memcpy）へ送る。
// flush は同期なので 2 面にしても重なる処理が無く、SRAM を無駄にするだけ。1 面にしている。
//
// LVGL の API を呼んでよいのは UI タスクだけ（lv_conf.h: LV_USE_OS = NONE）。
#pragma once

#include <cstdint>

#include "display_hal.h"
#include "lvgl.h"

namespace cm::ui {

/// 1 秒ごとなどに取り出して fps を計算するための累積値。
struct RenderStats {
    uint32_t frames      = 0;  ///< 最後の領域まで flush した回数（= 実際に画面を更新したフレーム数）
    uint32_t flushes     = 0;  ///< flush の呼び出し回数（1 フレーム = 無効領域をバッファ高さで区切った数）
    uint32_t flushPixels = 0;
    uint32_t flushUs     = 0;  ///< flush（フレームバッファへの転送）に使った時間
    uint32_t renderUs    = 0;  ///< lv_timer_handler 全体から flush を引いた時間（= LVGL の描画）
};

class LvglPort {
public:
    using TickFn = uint32_t (*)();

    static constexpr int kBufLines = 60;  // 480 x 60 x 2 = 57.6 KB

    /// @param nowMs LVGL のタイマ・アニメーション用の単調増加ミリ秒
    /// @param nowUs 計測用の単調増加マイクロ秒
    bool begin(DisplayHal& hal, TickFn nowMs, TickFn nowUs);

    /// UI タスクのループから呼ぶ。LVGL の描画と flush はここで行われる。
    /// @return 次に呼ぶまでに待ってよい時間 [ms]
    uint32_t service();

    lv_obj_t* screen() const { return m_screen; }

    /// 前回の呼び出しからの累積を返し、0 に戻す。
    RenderStats takeStats();

private:
    static void flushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* pxMap);

    DisplayHal* m_hal    = nullptr;
    TickFn m_nowUs       = nullptr;
    lv_display_t* m_disp = nullptr;
    lv_obj_t* m_screen   = nullptr;
    RenderStats m_stats{};
    uint32_t m_flushUsInService = 0;
};

}  // namespace cm::ui
