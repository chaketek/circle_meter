"""assets/logo_src.png を M5GFX で描ける RGB565 の C++ 配列に変換する。

DOC-23 §7 / SWR-48。生成物は assets/logo.cpp / assets/logo.h。

使い方:
    python tools/make_logo.py                      # 既定（黒背景・輝度反転）
    python tools/make_logo.py --no-invert          # 元画像のまま（白背景）
    python tools/make_logo.py --max-width 200 --max-height 140

既定で輝度を反転するのは、HMI が黒基調（DOC-23 P3）であり、
白背景のロゴをそのまま出すと丸型画面に白い四角が浮いてしまうため。
反転しても赤（スクリプト部）は色相を保つように処理している。
"""

import argparse
import os
import sys

try:
    from PIL import Image
except ImportError:
    sys.exit("Pillow が必要です: python -m pip install --user pillow")

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)


def is_chromatic(r: int, g: int, b: int, threshold: int = 40) -> bool:
    """有彩色（この画像では赤のスクリプト部分）かどうか。"""
    return (max(r, g, b) - min(r, g, b)) > threshold


def invert_luma(px: tuple) -> tuple:
    """無彩色のみ明暗を反転する。有彩色はそのまま残す。"""
    r, g, b = px[:3]
    if is_chromatic(r, g, b):
        return (r, g, b)
    return (255 - r, 255 - g, 255 - b)


def to_rgb565(r: int, g: int, b: int) -> int:
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", default=os.path.join(ROOT, "assets", "logo_src.png"))
    ap.add_argument("--out-c", default=os.path.join(ROOT, "assets", "logo.cpp"))
    ap.add_argument("--out-h", default=os.path.join(ROOT, "assets", "logo.h"))
    ap.add_argument("--max-width", type=int, default=204)
    ap.add_argument("--max-height", type=int, default=140)
    ap.add_argument("--no-invert", action="store_true", help="輝度反転をしない（白背景のまま）")
    ap.add_argument("--bg", default=None, help="背景色 RRGGBB。既定は反転時 000000 / 非反転時 FFFFFF")
    args = ap.parse_args()

    im = Image.open(args.src).convert("RGB")

    if not args.no_invert:
        px = im.load()
        for y in range(im.height):
            for x in range(im.width):
                px[x, y] = invert_luma(px[x, y])

    bg_hex = args.bg or ("FFFFFF" if args.no_invert else "000000")
    bg = tuple(int(bg_hex[i : i + 2], 16) for i in (0, 2, 4))

    # 余白を落とす: 背景色と異なるピクセルの外接矩形
    diff = Image.new("L", im.size, 0)
    dpx = diff.load()
    spx = im.load()
    for y in range(im.height):
        for x in range(im.width):
            r, g, b = spx[x, y]
            if abs(r - bg[0]) + abs(g - bg[1]) + abs(b - bg[2]) > 24:
                dpx[x, y] = 255
    bbox = diff.getbbox()
    if bbox:
        im = im.crop(bbox)

    # 縦横比を保って収める
    scale = min(args.max_width / im.width, args.max_height / im.height, 1.0)
    w = max(1, int(round(im.width * scale)))
    h = max(1, int(round(im.height * scale)))
    im = im.resize((w, h), Image.LANCZOS)

    # 偶数幅に揃える（転送単位を合わせるため）
    if w % 2:
        canvas = Image.new("RGB", (w + 1, h), bg)
        canvas.paste(im, (0, 0))
        im = canvas
        w += 1

    data = []
    spx = im.load()
    for y in range(h):
        for x in range(w):
            r, g, b = spx[x, y]
            data.append(to_rgb565(r, g, b))

    name = "cm_logo"
    with open(args.out_h, "w", encoding="utf-8", newline="\n") as f:
        f.write(
            "// tools/make_logo.py が生成。手で編集しないこと。\n"
            "// 元画像: assets/logo_src.png  (DOC-23 §7 / SWR-48)\n"
            "#pragma once\n\n"
            "#include <cstdint>\n\n"
            f"constexpr int      {name}_width  = {w};\n"
            f"constexpr int      {name}_height = {h};\n"
            f"constexpr uint16_t {name}_bg     = 0x{to_rgb565(*bg):04X};\n"
            f"extern const uint16_t {name}_data[{w * h}];\n"
        )

    with open(args.out_c, "w", encoding="utf-8", newline="\n") as f:
        f.write(
            "// tools/make_logo.py が生成。手で編集しないこと。\n"
            "// 元画像: assets/logo_src.png  (DOC-23 §7 / SWR-48)\n"
            '#include "logo.h"\n\n'
            f"const uint16_t {name}_data[{w * h}] = {{\n"
        )
        for i in range(0, len(data), 12):
            f.write("    " + " ".join(f"0x{v:04X}," for v in data[i : i + 12]) + "\n")
        f.write("};\n")

    print(f"{w}x{h} -> {args.out_c} ({w * h * 2} bytes / {w * h * 2 / 1024:.1f} KB)")
    print(f"background = #{bg_hex}, invert = {not args.no_invert}")


if __name__ == "__main__":
    main()
