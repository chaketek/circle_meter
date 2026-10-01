#include "display_hal.h"

#ifdef CM_BOARD_LCD21

#include <board/esp_panel_board_default_config.hpp>
#include <esp_display_panel.hpp>

using namespace esp_panel::board;
using namespace esp_panel::drivers;

namespace cm {

namespace {
Board* g_board = nullptr;
}

bool DisplayHal::begin() {
    BoardConfig cfg = ESP_PANEL_BOARD_DEFAULT_CONFIG;
    if (cfg.backlight.has_value()) {
        cfg.backlight->pre_process.idle_off = 1;  // begin() で点灯させない（上のヘッダコメント参照）
    }
    g_board = new Board(cfg);  // 起動時に 1 回だけ。解放しない
    if (!g_board->init() || !g_board->begin()) {
        return false;
    }
    m_ready = (g_board->getLCD() != nullptr);
    return m_ready;
}

void DisplayHal::setBrightnessPercent(uint8_t percent) {
    if (g_board == nullptr || g_board->getBacklight() == nullptr) {
        return;
    }
    g_board->getBacklight()->setBrightness(percent > 100 ? 100 : percent);
}

void DisplayHal::drawBitmap(int x1, int y1, int x2, int y2, const uint8_t* rgb565) {
    if (g_board == nullptr || g_board->getLCD() == nullptr) {
        return;
    }
    g_board->getLCD()->drawBitmap(x1, y1, x2 - x1 + 1, y2 - y1 + 1, rgb565);
}

bool DisplayHal::readTouch(int& x, int& y) {
    if (g_board == nullptr || g_board->getTouch() == nullptr) {
        return false;
    }
    TouchPoint p;
    if (g_board->getTouch()->readPoints(&p, 1, 0) < 1) {
        return false;
    }
    x = p.x;
    y = p.y;
    return true;
}

}  // namespace cm

#endif  // CM_BOARD_LCD21
