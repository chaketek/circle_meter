#include "big_number.h"

#include "theme.h"

namespace cm::ui {

namespace {

constexpr int kPadPx = 2;  // 字の ofs_x / anti-alias がセルからはみ出す分

/// UTF-8 を 1 コードポイント読み、バイト列を out にコピーして長さを返す。壊れた列は 1 バイト消費して '?'。
int decodeUtf8(const char* p, uint32_t* cp, char out[5]) {
    const uint8_t b0 = static_cast<uint8_t>(p[0]);
    int len          = 1;
    if (b0 >= 0xF0) {
        len = 4;
    } else if (b0 >= 0xE0) {
        len = 3;
    } else if (b0 >= 0xC0) {
        len = 2;
    }
    for (int i = 1; i < len; ++i) {
        if ((static_cast<uint8_t>(p[i]) & 0xC0) != 0x80) {  // 継続バイトが足りない
            out[0] = '?';
            out[1] = '\0';
            *cp    = '?';
            return 1;
        }
    }
    uint32_t v = (len == 1) ? b0 : (len == 2) ? (b0 & 0x1Fu) : (len == 3) ? (b0 & 0x0Fu) : (b0 & 0x07u);
    for (int i = 1; i < len; ++i) {
        v = (v << 6) | (static_cast<uint8_t>(p[i]) & 0x3Fu);
    }
    for (int i = 0; i < len; ++i) {
        out[i] = p[i];
    }
    out[len] = '\0';
    *cp      = v;
    return len;
}

}  // namespace

void BigNumber::create(lv_obj_t* parent, const lv_font_t* font, int width, int centerY) {
    m_obj = lv_obj_create(parent);
    lv_obj_remove_style_all(m_obj);
    lv_obj_remove_flag(m_obj, static_cast<lv_obj_flag_t>(LV_OBJ_FLAG_CLICKABLE | LV_OBJ_FLAG_SCROLLABLE));
    lv_obj_add_event_cb(m_obj, &BigNumber::onDraw, LV_EVENT_DRAW_MAIN, this);
    m_centerY = centerY;
    m_font    = font;
    m_width   = width;
    applyGeometry();
}

void BigNumber::applyGeometry() {
    int w = 0;
    for (uint32_t c = '0'; c <= '9'; ++c) {
        const int g = lv_font_get_glyph_width(m_font, c, 0);
        w           = (g > w) ? g : w;
    }
    m_digitW     = w;
    const int lh = lv_font_get_line_height(m_font);
    lv_obj_set_size(m_obj, m_width, lh);
    lv_obj_set_pos(m_obj, (layout::kWidth - m_width) / 2, m_centerY - lh / 2);
}

void BigNumber::setCellScale(uint32_t codepoint, float scale) {
    for (int k = 0; k < m_scaledCount; ++k) {
        if (m_scaledCp[k] == codepoint) {
            m_scale[k] = scale;
            return;
        }
    }
    if (m_scaledCount < kMaxScaled) {
        m_scaledCp[m_scaledCount] = codepoint;
        m_scale[m_scaledCount]    = scale;
        ++m_scaledCount;
    }
}

void BigNumber::setFont(const lv_font_t* font, int width) {
    m_font  = font;
    m_width = width;
    applyGeometry();
    m_count = 0;  // 次の setText() で全体を描き直させる
    lv_obj_invalidate(m_obj);
}

int BigNumber::layout(const char* text, Cell out[kMaxChars]) const {
    int n     = 0;
    int total = 0;
    while (n < kMaxChars && *text != '\0') {
        Cell& c = out[n];
        text += decodeUtf8(text, &c.cp, c.utf8);
        const bool dig = (c.cp >= '0' && c.cp <= '9');
        int w          = dig ? m_digitW : lv_font_get_glyph_width(m_font, c.cp, 0);
        for (int k = 0; !dig && k < m_scaledCount; ++k) {
            if (m_scaledCp[k] == c.cp) {
                w = static_cast<int>(static_cast<float>(m_digitW) * m_scale[k] + 0.5f);
            }
        }
        c.w = static_cast<int16_t>(w);
        total += c.w;
        ++n;
    }
    int x = (m_width - total) / 2;
    for (int i = 0; i < n; ++i) {
        out[i].x = static_cast<int16_t>(x);
        x += out[i].w;
    }
    return n;
}

void BigNumber::invalidateCell(const Cell& c) {
    lv_area_t coords;
    lv_obj_get_coords(m_obj, &coords);
    lv_area_t a;
    a.x1 = coords.x1 + c.x - kPadPx;
    a.x2 = coords.x1 + c.x + c.w - 1 + kPadPx;
    a.y1 = coords.y1;
    a.y2 = coords.y2;
    lv_obj_invalidate_area(m_obj, &a);
}

void BigNumber::setText(const char* text, uint32_t colorHex) {
    Cell next[kMaxChars];
    const int n = layout(text, next);

    if (colorHex != m_colorHex) {
        // 色が変わると全ての桁が変わる（ゾーン境界を跨いだときだけ）
        m_colorHex = colorHex;
        for (int i = 0; i < n; ++i) {
            m_cells[i] = next[i];
        }
        m_count = n;
        lv_obj_invalidate(m_obj);
        return;
    }

    // 変わった桁（文字・位置・幅のいずれか）だけを、旧位置と新位置の両方で無効化する。
    // 旧位置を消さないと、桁が動いたとき（"9.9" -> "10.0" など）に残像が残る
    const int m = (n > m_count) ? n : m_count;
    for (int i = 0; i < m; ++i) {
        const bool hadOld = i < m_count;
        const bool hasNew = i < n;
        const bool same   = hadOld && hasNew && m_cells[i].cp == next[i].cp && m_cells[i].x == next[i].x &&
                            m_cells[i].w == next[i].w;
        if (same) {
            continue;
        }
        if (hadOld) {
            invalidateCell(m_cells[i]);
        }
        if (hasNew) {
            invalidateCell(next[i]);
        }
    }
    for (int i = 0; i < n; ++i) {
        m_cells[i] = next[i];
    }
    m_count = n;
}

void BigNumber::onDraw(lv_event_t* e) {
    auto* self        = static_cast<BigNumber*>(lv_event_get_user_data(e));
    lv_layer_t* layer = lv_event_get_layer(e);
    lv_area_t coords;
    lv_obj_get_coords(self->m_obj, &coords);
    self->draw(layer, coords.x1, coords.y1);
}

void BigNumber::draw(lv_layer_t* layer, int32_t originX, int32_t originY) const {
    const int lh = lv_font_get_line_height(m_font);
    for (int i = 0; i < m_count; ++i) {
        lv_draw_label_dsc_t d;
        lv_draw_label_dsc_init(&d);
        d.text       = m_cells[i].utf8;  // 描画タスクは後で実行されるので、メンバの配列を指す
        d.text_local = 0;
        d.font       = m_font;
        d.color      = lv_color_hex(m_colorHex);
        d.opa        = LV_OPA_COVER;
        d.align      = LV_TEXT_ALIGN_CENTER;
        lv_area_t cell;
        cell.x1 = originX + m_cells[i].x;
        cell.x2 = cell.x1 + m_cells[i].w - 1;
        cell.y1 = originY;
        cell.y2 = originY + lh - 1;
        lv_draw_label(layer, &d, &cell);
    }
}

}  // namespace cm::ui
