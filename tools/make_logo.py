"""assets/logo_src.png を M5GFX で描ける RGB565 の C++ 配列に変換する。

DOC-23 §7 / SWR-48。生成物は assets/logo.cpp / assets/logo.h。

使い方:
    python tools/make_logo.py --user               # 配布先ユーザー: user/logo.png -> user/logo.cpp / logo.h（SWR-67）
    python tools/make_logo.py --user --invert      # 同上。白地に黒（+1 色）のロゴを黒地へ反転する
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
    ap.add_argument("--user", action="store_true",
                    help="配布先ユーザー用: user/logo.png を LCD-2.1 の大きさで user/logo.cpp / logo.h へ。"
                         "既定で反転せず黒地に置く（--invert で反転）")
    ap.add_argument("--invert", action="store_true", help="--user のとき輝度反転する（白地に黒・1 色のロゴ向け）")
    ap.add_argument("--src", default=None)
    ap.add_argument("--out-c", default=None)
    ap.add_argument("--out-h", default=None)
    ap.add_argument("--max-width", type=int, default=None)
    ap.add_argument("--max-height", type=int, default=None)
    ap.add_argument("--no-invert", action="store_true", help="輝度反転をしない（白背景のまま）")
    ap.add_argument("--bg", default=None, help="背景色 RRGGBB。既定は反転時 000000 / 非反転時 FFFFFF")
    args = ap.parse_args()

    if args.user:
        # LCD-2.1 のスプラッシュの枠（DOC-23 §7）。背景は画面と同じ黒
        user_dir = os.path.join(ROOT, "user")
        args.src = args.src or os.path.join(user_dir, "logo.png")
        args.out_c = args.out_c or os.path.join(user_dir, "logo.cpp")
        args.out_h = args.out_h or os.path.join(user_dir, "logo.h")
        args.max_width = args.max_width or 408
        args.max_height = args.max_height or 174
        args.no_invert = not args.invert
        args.bg = args.bg or "000000"
        if not os.path.isfile(args.src):
            sys.exit(f"画像が見つかりません: {args.src}\nuser フォルダに logo.png という名前で置いてください")
    else:
        args.src = args.src or os.path.join(ROOT, "assets", "logo_src.png")
        args.out_c = args.out_c or os.path.join(ROOT, "assets", "logo.cpp")
        args.out_h = args.out_h or os.path.join(ROOT, "assets", "logo.h")
        args.max_width = args.max_width or 204
        args.max_height = args.max_height or 140

    src = Image.open(args.src)
    if src.mode in ("RGBA", "LA") or (src.mode == "P" and "transparency" in src.info):
        # 透過 PNG は背景色の上に重ねる（透明部分を黒にする。--bg で変えられる）
        bg_for_alpha = args.bg or ("FFFFFF" if args.no_invert else "000000")
        under = Image.new("RGBA", src.size, "#" + bg_for_alpha)
        src = Image.alpha_composite(under, src.convert("RGBA"))
    im = src.convert("RGB")

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

    if args.user:
        # 変換結果を画像でも残す。Claude Code とユーザーが書き込む前に見て確かめるため（docs/user/06_logo.md）
        preview = Image.new("RGB", (480, 480), bg)
        # page_splash.cpp と同じ位置（中央から 16 px 上。下に版数を出すため）
        preview.paste(im, ((480 - w) // 2, (480 - h) // 2 - 16))
        # 丸い画面の外側を灰色にして、円の中に収まっているかを見やすくする
        from PIL import ImageDraw
        mask = Image.new("L", (480, 480), 255)
        ImageDraw.Draw(mask).ellipse((0, 0, 479, 479), fill=0)
        preview.paste((64, 64, 64), (0, 0, 480, 480), mask)
        preview.save(os.path.join(os.path.dirname(args.out_c), "logo_preview.png"))

    data = []
    spx = im.load()
    for y in range(h):
        for x in range(w):
            r, g, b = spx[x, y]
            data.append(to_rgb565(r, g, b))

    name = "cm_logo"
    src_rel = os.path.relpath(args.src, ROOT).replace(os.sep, "/")
    with open(args.out_h, "w", encoding="utf-8", newline="\n") as f:
        f.write(
            "// tools/make_logo.py が生成。手で編集しないこと。\n"
            f"// 元画像: {src_rel}  (DOC-23 §7 / SWR-48)\n"
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
            f"// 元画像: {src_rel}  (DOC-23 §7 / SWR-48)\n"
            '#include "logo.h"\n\n'
            f"const uint16_t {name}_data[{w * h}] = {{\n"
        )
        for i in range(0, len(data), 12):
            f.write("    " + " ".join(f"0x{v:04X}," for v in data[i : i + 12]) + "\n")
        f.write("};\n")

    print(f"{w}x{h} -> {args.out_c} ({w * h * 2} bytes / {w * h * 2 / 1024:.1f} KB)")
    print(f"background = #{bg_hex}, invert = {not args.no_invert}")
    if args.user:
        print("preview  -> user/logo_preview.png（480x480 の画面に置いた見え方）")


if __name__ == "__main__":
    main()
