#include "page_splash.h"

#include "logo.h"
#include "theme.h"

namespace cm::ui {

void PageSplash::onCreate(lv_obj_t* parent, const Config&) {
    m_root = lv_obj_create(parent);
    lv_obj_remove_style_all(m_root);
    lv_obj_set_pos(m_root, 0, 0);
    lv_obj_set_size(m_root, layout::kWidth, layout::kHeight);
    lv_obj_remove_flag(m_root, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));

    // assets/lcd21/logo.cpp は RGB565 の生データ（tools/make_logo.py が生成。LVGL の既定バイト順と同じ）。
    // Flash 上の const 配列を直接参照させ、RAM へコピーしない（DOC-21 §2.4）
    m_logo.header.magic  = LV_IMAGE_HEADER_MAGIC;
    m_logo.header.cf     = LV_COLOR_FORMAT_RGB565;
    m_logo.header.w      = cm_logo_width;
    m_logo.header.h      = cm_logo_height;
    m_logo.header.stride = cm_logo_width * 2;
    m_logo.data_size     = static_cast<uint32_t>(cm_logo_width) * cm_logo_height * 2;
    m_logo.data          = reinterpret_cast<const uint8_t*>(cm_logo_data);

    lv_obj_t* img = lv_image_create(m_root);
    lv_image_set_src(img, &m_logo);
    lv_obj_set_pos(img, (layout::kWidth - cm_logo_width) / 2, (layout::kHeight - cm_logo_height) / 2 - 16);

    auto makeLabel = [&](const char* text, int centerY) {
        lv_obj_t* l = lv_label_create(m_root);
        lv_obj_set_style_text_font(l, &lv_font_montserrat_24, 0);
        lv_obj_set_style_text_color(l, lv_color_hex(color::kDisabled), 0);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_set_width(l, 320);
        lv_label_set_text_static(l, text);
        lv_obj_set_pos(l, layout::kCx - 160, centerY - lv_font_get_line_height(&lv_font_montserrat_24) / 2);
    };
    makeLabel("circle_meter", 360);
    makeLabel(m_version, 392);
}

void PageSplash::onShow() {
    lv_obj_remove_flag(m_root, LV_OBJ_FLAG_HIDDEN);
}

void PageSplash::onHide() {
    lv_obj_add_flag(m_root, LV_OBJ_FLAG_HIDDEN);
}

}  // namespace cm::ui
