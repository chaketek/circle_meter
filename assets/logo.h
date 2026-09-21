// tools/make_logo.py が生成。手で編集しないこと。
// 元画像: assets/logo_src.png  (DOC-23 §7 / SWR-48)
#pragma once

#include <cstdint>

constexpr int      cm_logo_width  = 204;
constexpr int      cm_logo_height = 87;
constexpr uint16_t cm_logo_bg     = 0x0000;
extern const uint16_t cm_logo_data[17748];
