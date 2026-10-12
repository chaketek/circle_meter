"""assets/logo_src.png を M5GFX で描ける RGB565 の C++ 配列に変換する。

DOC-23 §7 / SWR-48。生成物は assets/logo.cpp / assets/logo.h。

使い方:
    python tools/make_logo.py                      # 既定（黒背景・輝度反転）
    python tools/make_logo.py --no-invert          # 元画像のまま（白背景）
    python tools/make_logo.py --max-width 200 --max-height 140

既定で輝度を反転するのは、HMI が黒基調（DOC-23 P3）であり、
白背景のロゴをそのまま出すと丸型画面に白い四角が浮いてしまうため。
反転しても赤（スクリプト部）は色相を保つように処理している。

反転は「画素ごとに白地へ黒インクと色インク（赤）が何割ずつ乗っているか」を求め、黒地の上で
黒インク -> 白、色インク -> そのままの色、として塗り直す方式（unmix_invert）。
画素単位で「有彩色なら残す / 無彩色なら反転」と切り替えると、赤字の縁の薄いピンク（赤と白の中間色）が
反転されずに残り、黒地で明るい縁取りが浮いてジャギーに見える（2026-10-04 に修正）。
"""

import argparse
import os
import sys

try:
    import numpy as np
    from PIL import Image
except ImportError:
    sys.exit("Pillow と numpy が必要です: python -m pip install --user pillow numpy")

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)


def estimate_ink(arr) -> "np.ndarray":
    """色インク（スクリプトの赤）の色を推定する。彩度の高い画素の中央値。"""
    chroma = arr.max(axis=2) - arr.min(axis=2)
    strong = arr[chroma > 120 / 255.0]  # arr は 0.0-1.0
    if len(strong) == 0:
        return np.array([1.0, 0.0, 0.0])
    return np.median(strong, axis=0)


def unmix_invert(im: "Image.Image") -> "Image.Image":
    """白地の画像を、黒インクと色インクの 2 色の混合とみなして分解し、黒地に塗り直す。

    画素 p = 白 * (1 - k - c) + 黒 * k + 色 * c  を (k, c) について最小二乗で解く。
    塗り直しは  out = 白 * k + 色 * c（黒地なので残りは 0）。
    中間色（アンチエイリアスの縁）も k / c の割合として保たれるので、縁がなめらかに黒地へ溶ける。
    """
    arr = np.asarray(im.convert("RGB")).astype(np.float64) / 255.0
    ink = estimate_ink(arr)
    d = 1.0 - arr                      # 白からの差 = k * (1,1,1) + c * (1 - ink)
    u = np.ones(3)
    v = 1.0 - ink
    # 2x2 の正規方程式
    uu, uv, vv = u @ u, u @ v, v @ v
    du = d @ u
    dv = d @ v
    det = uu * vv - uv * uv
    k = (du * vv - dv * uv) / det
    c = (dv * uu - du * uv) / det
    k = np.clip(k, 0.0, 1.0)
    c = np.clip(c, 0.0, 1.0)
    total = k + c
    over = total > 1.0
    k[over] /= total[over]
    c[over] /= total[over]
    out = k[..., None] * u + c[..., None] * ink
    return Image.fromarray(np.clip(out * 255.0 + 0.5, 0, 255).astype(np.uint8), "RGB")


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
        im = unmix_invert(im)

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
