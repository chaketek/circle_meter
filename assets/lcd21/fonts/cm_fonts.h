// tools/make_font.py が生成。手で編集しないこと。
#pragma once
#include "lvgl.h"

extern const lv_font_t cm_font_afr;  // 136 px / 主数値 AFR（例 14.7）
extern const lv_font_t cm_font_lambda;  // 108 px / 主数値 λ（例 1.000）
extern const lv_font_t cm_font_egt;  // 76 px / 排気温度（例 845°C）
extern const lv_font_t cm_font_b612_main;  // 70 px / A: 主数値（例 14.2 / 1.000）
extern const lv_font_t cm_font_b612_egt;  // 34 px / A: 排気温度（例 712°C）
extern const lv_font_t cm_font_b612_scale;  // 28 px / A: 目盛り数字
extern const lv_font_t cm_font_b612_label;  // 22 px / A: 銘板・NO SIGNAL
extern const lv_font_t cm_font_b612_small;  // 14 px / A: 小さい銘板・診断
