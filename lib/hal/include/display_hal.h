// SWA-08 / SWD-06: 表示とタッチの HAL（Waveshare ESP32-S3-Touch-LCD-2.1）
//
// ESP32_Display_Panel（Espressif）を包み、上位（lib/ui）に ESP32 / ライブラリの型を見せない。
// 実装は CM_BOARD_LCD21 のときだけ有効（M5Dial 環境でもこのライブラリは一緒にコンパイルされるため）。
//
// 【起動時のちらつき対策】Board::begin() は既定でバックライトを点灯するが、その時点のフレームバッファ
// （PSRAM）は未初期化でノイズが出る。begin() 前に idle_off を立て、点灯は呼び出し側が最初の描画を終えて
// から setBrightnessPercent() で行う（DOC-23 §7）。
#pragma once

#include <cstdint>

namespace cm {

class DisplayHal {
public:
    static constexpr int kWidth  = 480;
    static constexpr int kHeight = 480;

    /// LCD / タッチ / IO エキスパンダ / バックライトを初期化する。バックライトは消灯のまま。
    bool begin();

    /// 0-100。バックライトは GPIO6 の LEDC PWM（SWR-47）。
    void setBrightnessPercent(uint8_t percent);

    /// RGB565（リトルエンディアン）の矩形を転送する。x2 / y2 は端点を含む（LVGL の lv_area_t と同じ）。
    /// 実体は PSRAM のフレームバッファへの memcpy で、完了まで戻らない。
    void drawBitmap(int x1, int y1, int x2, int y2, const uint8_t* rgb565);

    /// 現在の接触点。押されていれば true。非ブロッキング（I2C 読み出しが 1 回走る）。
    bool readTouch(int& x, int& y);

    bool ready() const { return m_ready; }

private:
    bool m_ready = false;
};

}  // namespace cm
