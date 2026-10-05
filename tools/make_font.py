"""数字・記号専用の LVGL ビットマップフォント (.c) を TTF から生成する。

DOC-23 §9。LVGL 内蔵の Montserrat は最大 48 px で、480x480 の主数値（約 130 px）に足りない。
公式の lv_font_conv は Node.js が要るので、Pillow だけで同じ形式（4 bpp・非圧縮）を出力する。

    python tools/make_font.py             assets/lcd21/fonts/ に全フォントと cm_fonts.h を生成
    python tools/make_font.py --probe     生成せず、サンプル文字列の寸法を表示（サイズ決定用）

出力形式は lv_font_conv の `--no-compress --bpp 4 --format lvgl` と同じで、ビットは行をまたいで連続して
詰める（lv_font_fmt_txt.c の bpp==4 の展開コードと一致させてある）。

元の TTF は assets/fonts/ に置く（DOC-40 §1: 元データと生成物の両方を git で管理する）。
Montserrat は SIL Open Font License 1.1（assets/fonts/README.md）。
"""

import argparse
import os
import sys

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError:
    sys.exit("Pillow が必要です: python -m pip install --user pillow")

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
TTF = os.path.join(ROOT, "assets", "fonts", "Montserrat-Bold.ttf")
B612_BOLD = os.path.join(ROOT, "assets", "fonts", "B612Mono-Bold.ttf")
B612_REG = os.path.join(ROOT, "assets", "fonts", "B612Mono-Regular.ttf")
UPPER = " -./0123456789:ABCDEFGHIJKLMNOPQRSTUVWXYZ\u00b0"  # ":" と "°" は λ センサの状態表示（SWR-51）
ASCII = "".join(chr(c) for c in range(0x20, 0x7F))
OUT_DIR = os.path.join(ROOT, "assets", "lcd21", "fonts")

DIGITS = "0123456789.-"

# (シンボル名, サイズ px, 収録文字, 用途)
# サイズは DOC-23 §3.3 / §4 の円に収まる最大値（--probe の結果から決めた）
# (シンボル名, サイズ px, 収録文字, 用途, TTF)
# サイズは DOC-23 §3.3 / §3.5 / §4 の円に収まる最大値（--probe の結果から決めた）
FONTS = [
    ("cm_font_afr", 136, DIGITS, "主数値 AFR（例 14.7）", TTF),
    ("cm_font_lambda", 108, DIGITS, "主数値 λ（例 1.000）", TTF),
    ("cm_font_egt", 76, "0123456789-\u00b0C", "排気温度（例 845°C）", TTF),
    # 表示デザイン A（指針式・大森風。DOC-23 §3.5）。B612 Mono は等幅（SWR-50）
    ("cm_font_b612_main", 70, DIGITS, "A: 主数値（例 14.2 / 1.000）", B612_BOLD),
    ("cm_font_b612_egt", 34, "0123456789-\u00b0C", "A: 排気温度（例 712°C）", B612_BOLD),
    ("cm_font_b612_scale", 28, "0123456789", "A: 目盛り数字", B612_REG),
    ("cm_font_b612_label", 22, UPPER, "A: 銘板・NO SIGNAL", B612_BOLD),
    ("cm_font_b612_small", 14, ASCII, "A: 小さい銘板・診断", B612_REG),
]


def glyph_of(font, ch):
    """1 文字をレンダリングし、(box_w, box_h, ofs_x, ofs_y, adv_w16, 4bpp 値の配列) を返す。

    getbbox() の横方向は送り幅を含むので、インクの外接矩形は描画結果から取る。
    ofs_y は「ベースラインからグリフ下端までの高さ」（下端がベースラインより下なら負）。
    """
    size = font.size
    ox0, oy0 = size, size * 2  # 大きなキャンバス上のベースライン原点
    canvas = Image.new("L", (size * 4, size * 3), 0)
    ImageDraw.Draw(canvas).text((ox0, oy0), ch, font=font, fill=255, anchor="ls")
    bb = canvas.getbbox()
    adv = int(round(font.getlength(ch) * 16))
    if bb is None:  # 空白など
        return 0, 0, 0, 0, adv, []
    im = canvas.crop(bb)
    w, h = im.size
    px = im.load()
    vals = [min(15, (px[x, y] + 8) // 17) for y in range(h) for x in range(w)]
    return w, h, bb[0] - ox0, oy0 - bb[3], adv, vals


def pack4(vals):
    """4 bpp を連続したビット列に詰める（上位ニブルが先。行の区切りは無い）。"""
    out = bytearray()
    for i in range(0, len(vals), 2):
        hi = vals[i]
        lo = vals[i + 1] if i + 1 < len(vals) else 0
        out.append((hi << 4) | lo)
    return out


def build(name, size, chars, note, ttf=TTF):
    font = ImageFont.truetype(ttf, size, layout_engine=ImageFont.Layout.BASIC)
    cps = sorted(set(ord(c) for c in chars))
    glyphs = [glyph_of(font, chr(cp)) for cp in cps]

    # 収録文字のインクだけで行の高さを決める。数字専用なので行間は不要で、
    # ラベルの箱が見た目の字の高さとほぼ一致し、垂直中央揃えがそのまま効く。
    asc = max([g[3] + g[1] for g in glyphs if g[1]] + [0])    # ベースラインより上の最大高さ
    desc = max([-g[3] for g in glyphs if g[1]] + [0])         # ベースラインより下の最大深さ
    desc = max(desc, 0)
    line_h = asc + desc

    bitmap = bytearray()
    dsc_rows = ["    {.bitmap_index = 0, .adv_w = 0, .box_w = 0, .box_h = 0, .ofs_x = 0, .ofs_y = 0}, /* id 0 = 未収録 */"]
    for cp, (w, h, ox, oy, adv, vals) in zip(cps, glyphs):
        idx = len(bitmap)
        bitmap += pack4(vals)
        dsc_rows.append(
            f"    {{.bitmap_index = {idx}, .adv_w = {adv}, .box_w = {w}, .box_h = {h}, .ofs_x = {ox}, .ofs_y = {oy}}},"
            f" /* U+{cp:04X} '{chr(cp)}' */"
        )
    if len(bitmap) >= (1 << 20):
        sys.exit(f"{name}: ビットマップが 1 MB を超えた（bitmap_index は 20 bit）")

    lo, hi = cps[0], cps[-1]
    unicode_list = ", ".join(str(cp - lo) for cp in cps)

    lines = []
    lines.append(f"// tools/make_font.py が生成。手で編集しないこと。")
    lines.append(f"// {name}: {os.path.basename(ttf)} {size} px / 4 bpp / 非圧縮 / {note} (DOC-23 §9)")
    lines.append('#include "lvgl.h"')
    lines.append("")
    lines.append(f"static const uint8_t glyph_bitmap[{len(bitmap)}] = {{")
    for i in range(0, len(bitmap), 24):
        lines.append("    " + ", ".join(f"0x{b:02x}" for b in bitmap[i:i + 24]) + ",")
    lines.append("};")
    lines.append("")
    lines.append("static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {")
    lines += dsc_rows
    lines.append("};")
    lines.append("")
    lines.append(f"static const uint16_t unicode_list_0[] = {{{unicode_list}}};")
    lines.append("")
    lines.append("static const lv_font_fmt_txt_cmap_t cmaps[] = {")
    lines.append(
        f"    {{.range_start = {lo}, .range_length = {hi - lo + 1}, .glyph_id_start = 1, .unicode_list = unicode_list_0,"
        f" .glyph_id_ofs_list = NULL, .list_length = {len(cps)}, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY}},"
    )
    lines.append("};")
    lines.append("")
    lines.append("static const lv_font_fmt_txt_dsc_t font_dsc = {")
    lines.append("    .glyph_bitmap = glyph_bitmap,")
    lines.append("    .glyph_dsc = glyph_dsc,")
    lines.append("    .cmaps = cmaps,")
    lines.append("    .kern_dsc = NULL,")
    lines.append("    .kern_scale = 0,")
    lines.append("    .cmap_num = 1,")
    lines.append("    .bpp = 4,")
    lines.append("    .kern_classes = 0,")
    lines.append("    .bitmap_format = 0,")
    lines.append("};")
    lines.append("")
    lines.append(f"const lv_font_t {name} = {{")
    lines.append("    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,")
    lines.append("    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,")
    lines.append(f"    .line_height = {line_h},")
    lines.append(f"    .base_line = {desc},")
    lines.append("    .subpx = LV_FONT_SUBPX_NONE,")
    lines.append("    .underline_position = -1,")
    lines.append("    .underline_thickness = 1,")
    lines.append("    .dsc = &font_dsc,")
    lines.append("    .fallback = NULL,")
    lines.append("    .user_data = NULL,")
    lines.append("};")
    return "\n".join(lines) + "\n", len(bitmap), line_h


def ink_extent(font, text):
    """文字列全体のインク実寸 (幅, 高さ)。"""
    size = font.size
    canvas = Image.new("L", (int(font.getlength(text)) + size * 2, size * 3), 0)
    ImageDraw.Draw(canvas).text((size, size * 2), text, font=font, fill=255, anchor="ls")
    bb = canvas.getbbox()
    return (bb[2] - bb[0], bb[3] - bb[1]) if bb else (0, 0)


def probe():
    """サンプル文字列の寸法（インク実寸）と、円に収まるかを表示する。"""
    cx = cy = 240
    r_in = 176  # DOC-23 §2: 内周コンテンツの半径
    samples = [
        ("AFR", "14.7", 208), ("AFR", "20.0", 208), ("AFR", "10.0", 208),
        ("LAM", "1.000", 208), ("LAM", "0.680", 208), ("LAM", "1.360", 208),
        ("EGT", "845°C", 336), ("EGT", "960°C", 336), ("EGT", "300°C", 336),
    ]
    for size in (160, 152, 144, 136, 128, 120, 112, 104, 96, 88, 80, 72, 64):
        font = ImageFont.truetype(TTF, size, layout_engine=ImageFont.Layout.BASIC)
        for tag, text, yc in samples:
            w, h = ink_extent(font, text)
            dy = max(abs(yc - h / 2 - cy), abs(yc + h / 2 - cy))
            avail = 2 * (max(r_in ** 2 - dy ** 2, 0) ** 0.5)
            ok = "OK " if w <= avail else "NG "
            print(f"{tag} {size:3d}px '{text}' w={w:4d} h={h:3d} avail={avail:5.0f} {ok}")
        print()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--probe", action="store_true")
    args = ap.parse_args()
    if args.probe:
        probe()
        return
    os.makedirs(OUT_DIR, exist_ok=True)
    decl = ["// tools/make_font.py が生成。手で編集しないこと。", "#pragma once", '#include "lvgl.h"', ""]
    for name, size, chars, note, ttf in FONTS:
        src, nbytes, line_h = build(name, size, chars, note, ttf)
        with open(os.path.join(OUT_DIR, name + ".c"), "w", encoding="utf-8", newline="\n") as f:
            f.write(src)
        decl.append(f"extern const lv_font_t {name};  // {size} px / {note}")
        print(f"{name}: {size}px bitmap {nbytes} B line_height {line_h}")
    with open(os.path.join(OUT_DIR, "cm_fonts.h"), "w", encoding="utf-8", newline="\n") as f:
        f.write("\n".join(decl) + "\n")


if __name__ == "__main__":
    main()
