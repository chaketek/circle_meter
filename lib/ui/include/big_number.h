// SWA-22: BigNumber（主数値・EGT 用の大きい数字）
//
// 【なぜ lv_label を使わないか】
//   1. lv_label は描画範囲を「フォント高さの 1/4」ずつ広げる（lv_label.c の LV_EVENT_REFR_EXT_DRAW_SIZE）。
//      字が行の外へはみ出す欧文の下降部のための余白だが、数字専用フォントでは不要で、
//      主数値では無効領域が 320x78 -> 359x117（約 1.7 倍）に膨らんだ（OPN-12 の実機計測）。
//   2. 文字列が変わると label は全体を再描画する。値が毎フレーム動くとき、変わるのは
//      通常は末尾の 1 桁だけなので、変わった桁のセルだけを無効化すれば描画量を 1/4 以下にできる（DEC-05）。
//
// 数字は等幅（最も広い数字の幅）のセルに置く。桁ごとに幅が違う比例幅だと、1 桁変わるたびに
// 文字列全体の幅が変わって全部が動き、桁単位の更新ができない。セルの中では中央揃えにする。
//
// 文字列は UTF-8。1 セル = 1 コードポイント（EGT の "°" は 2 バイト）。
#pragma once

#include <cstdint>

#include "lvgl.h"

namespace cm::ui {

class BigNumber {
public:
    static constexpr int kMaxChars = 8;

    /// @param width    文字列を中央揃えで置く領域の幅（最も広い文字列が収まること）
    /// @param centerY  垂直中心（画面座標）
    void create(lv_obj_t* parent, const lv_font_t* font, int width, int centerY);

    /// 書体と領域幅を変える（AFR <-> λ の切替）。全体を再描画する。
    void setFont(const lv_font_t* font, int width);

    /// 表示する文字列（UTF-8）と色。前回と同じ桁・同じ色は再描画しない。
    void setText(const char* text, uint32_t colorHex);

    lv_obj_t* obj() const { return m_obj; }

private:
    struct Cell {
        int16_t x    = 0;  // オブジェクト左端からの位置
        int16_t w    = 0;
        uint32_t cp  = 0;  // コードポイント
        char utf8[5] = {0, 0, 0, 0, 0};
    };

    static void onDraw(lv_event_t* e);
    void draw(lv_layer_t* layer, int32_t originX, int32_t originY) const;
    int layout(const char* text, Cell out[kMaxChars]) const;
    void invalidateCell(const Cell& c);
    void applyGeometry();

    lv_obj_t* m_obj         = nullptr;
    const lv_font_t* m_font = nullptr;
    int m_width             = 0;
    int m_centerY           = 0;
    int m_digitW            = 0;
    uint32_t m_colorHex     = 0;
    int m_count             = 0;
    Cell m_cells[kMaxChars]{};  // 描画は後で行われるので、文字列（utf8）もセルが保持する
};

}  // namespace cm::ui
