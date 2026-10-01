#include "lvgl_port.h"

#include "theme.h"

namespace cm::ui {

namespace {
// 内蔵 SRAM（.bss）。LVGL の部分描画バッファ。PSRAM には置かない（DEC-05 / RSK-14）
alignas(4) uint8_t g_drawBuf[DisplayHal::kWidth * LvglPort::kBufLines * 2];

LvglPort* g_self = nullptr;
}  // namespace

bool LvglPort::begin(DisplayHal& hal, TickFn nowMs, TickFn nowUs) {
    m_hal   = &hal;
    m_nowUs = nowUs;
    g_self  = this;

    lv_init();
    lv_tick_set_cb(nowMs);

    m_disp = lv_display_create(DisplayHal::kWidth, DisplayHal::kHeight);
    if (m_disp == nullptr) {
        return false;
    }
    lv_display_set_color_format(m_disp, LV_COLOR_FORMAT_RGB565);
    lv_display_set_buffers(m_disp, g_drawBuf, nullptr, sizeof(g_drawBuf), LV_DISPLAY_RENDER_MODE_PARTIAL);
    lv_display_set_flush_cb(m_disp, &LvglPort::flushCb);

    // 背景は黒（DOC-23 P3）。テーマは使わない（lv_conf.h: LV_USE_THEME_DEFAULT = 0）ので明示する
    m_screen = lv_display_get_screen_active(m_disp);
    lv_obj_set_style_bg_color(m_screen, lv_color_hex(color::kBg), 0);
    lv_obj_set_style_bg_opa(m_screen, LV_OPA_COVER, 0);
    lv_obj_remove_flag(m_screen, LV_OBJ_FLAG_SCROLLABLE);
    return true;
}

void LvglPort::flushCb(lv_display_t* disp, const lv_area_t* area, uint8_t* pxMap) {
    LvglPort* self    = g_self;
    const uint32_t t0 = self->m_nowUs();
    self->m_hal->drawBitmap(area->x1, area->y1, area->x2, area->y2, pxMap);
    const uint32_t dt = self->m_nowUs() - t0;

    self->m_stats.flushes++;
    self->m_stats.flushPixels += static_cast<uint32_t>(lv_area_get_width(area)) * lv_area_get_height(area);
    self->m_stats.flushUs += dt;
    self->m_flushUsInService += dt;
    if (lv_display_flush_is_last(disp)) {
        self->m_stats.frames++;
    }
    lv_display_flush_ready(disp);
}

uint32_t LvglPort::service() {
    m_flushUsInService   = 0;
    const uint32_t t0    = m_nowUs();
    const uint32_t next  = lv_timer_handler();
    const uint32_t total = m_nowUs() - t0;
    // 描画を伴わなかった呼び出し（タイマ確認だけ）は数 µs なので、そのまま足しても統計は歪まない
    m_stats.renderUs += (total > m_flushUsInService) ? (total - m_flushUsInService) : 0;
    return next;
}

RenderStats LvglPort::takeStats() {
    const RenderStats s = m_stats;
    m_stats             = RenderStats{};
    return s;
}

}  // namespace cm::ui
