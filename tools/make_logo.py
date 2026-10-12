"""assets/logo_src.png を M5GFX で描ける RGB565 の C++ 配列に変換する。

DOC-23 §7 / SWR-48。生成物は assets/logo.cpp / assets/logo.h。

使い方:
    python tools/make_logo.py --user               # 配布先ユーザー: user/logo.png -> user/generated/<機種>/user_logo.*（SWR-67 / SWR-68）
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


# 配布先ユーザーのロゴ（SWR-67 / SWR-68 / SWD-14）。機種ごとに枠とスプラッシュでの位置が違う
USER_BOARDS = {
    # 機種: (枠の幅, 枠の高さ, 画面の一辺, スプラッシュでの中央からの上ずれ px)
    "lcd21": (408, 174, 480, 16),  # page_splash.cpp（下に版数を出すため 16 px 上）
    "m5dial": (204, 140, 240, 8),  # src/main.cpp の showSplash
}


def load_source(path: str, invert: bool, bg_hex: str) -> "Image.Image":
    src = Image.open(path)
    if src.mode in ("RGBA", "LA") or (src.mode == "P" and "transparency" in src.info):
        # 透過 PNG は背景色の上に重ねる（透明部分を黒にする。--bg で変えられる）
        under = Image.new("RGBA", src.size, "#" + bg_hex)
        src = Image.alpha_composite(under, src.convert("RGBA"))
    im = src.convert("RGB")
    return unmix_invert(im) if invert else im


def fit(im: "Image.Image", bg, max_w: int, max_h: int) -> "Image.Image":
    """余白を落とし、縦横比を保って枠に収め、偶数幅にそろえる。"""
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

    scale = min(max_w / im.width, max_h / im.height, 1.0)
    w = max(1, int(round(im.width * scale)))
    h = max(1, int(round(im.height * scale)))
    im = im.resize((w, h), Image.LANCZOS)

    # 偶数幅に揃える（転送単位を合わせるため）
    if w % 2:
        canvas = Image.new("RGB", (w + 1, h), bg)
        canvas.paste(im, (0, 0))
        im = canvas
    return im


def write_c(im: "Image.Image", bg, src_path: str, out_c: str, out_h: str) -> None:
    w, h = im.size
    data = []
    spx = im.load()
    for y in range(h):
        for x in range(w):
            r, g, b = spx[x, y]
            data.append(to_rgb565(r, g, b))

    name = "cm_logo"
    src_rel = os.path.relpath(src_path, ROOT).replace(os.sep, "/")
    header = os.path.basename(out_h)
    with open(out_h, "w", encoding="utf-8", newline="\n") as f:
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
    with open(out_c, "w", encoding="utf-8", newline="\n") as f:
        f.write(
            "// tools/make_logo.py が生成。手で編集しないこと。\n"
            f"// 元画像: {src_rel}  (DOC-23 §7 / SWR-48)\n"
            f'#include "{header}"\n\n'
            f"const uint16_t {name}_data[{w * h}] = {{\n"
        )
        for i in range(0, len(data), 12):
            f.write("    " + " ".join(f"0x{v:04X}," for v in data[i : i + 12]) + "\n")
        f.write("};\n")
    print(f"{w}x{h} -> {os.path.relpath(out_c, ROOT)} ({w * h * 2} bytes / {w * h * 2 / 1024:.1f} KB)")


def write_preview(im: "Image.Image", bg, screen: int, lift: int, path: str) -> None:
    """変換結果を丸い画面に置いた見え方。Claude Code とユーザーが書き込む前に見て確かめる（docs/user/06_logo.md）"""
    from PIL import ImageDraw

    w, h = im.size
    preview = Image.new("RGB", (screen, screen), bg)
    preview.paste(im, ((screen - w) // 2, (screen - h) // 2 - lift))
    # 丸い画面の外側を灰色にして、円の中に収まっているかを見やすくする
    mask = Image.new("L", (screen, screen), 255)
    ImageDraw.Draw(mask).ellipse((0, 0, screen - 1, screen - 1), fill=0)
    preview.paste((64, 64, 64), (0, 0, screen, screen), mask)
    preview.save(path)
    print(f"preview  -> {os.path.relpath(path, ROOT)}（{screen}x{screen} の画面に置いた見え方）")


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("--user", action="store_true",
                    help="配布先ユーザー用: user/logo.png を LCD-2.1 と M5Dial の両方の大きさで "
                         "user/generated/<機種>/user_logo.cpp / .h へ。既定で反転せず黒地に置く（--invert で反転）")
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
        user_dir = os.path.join(ROOT, "user")
        src_path = args.src or os.path.join(user_dir, "logo.png")
        if not os.path.isfile(src_path):
            sys.exit(f"画像が見つかりません: {src_path}\nuser フォルダに logo.png という名前で置いてください")
        invert = args.invert
        bg_hex = args.bg or "000000"  # 画面の背景と同じ黒
        bg = tuple(int(bg_hex[i : i + 2], 16) for i in (0, 2, 4))
        base = load_source(src_path, invert, bg_hex)
        for board, (max_w, max_h, screen, lift) in USER_BOARDS.items():
            out_dir = os.path.join(user_dir, "generated", board)
            os.makedirs(out_dir, exist_ok=True)
            im = fit(base, bg, max_w, max_h)
            write_c(im, bg, src_path, os.path.join(out_dir, "user_logo.cpp"), os.path.join(out_dir, "user_logo.h"))
            write_preview(im, bg, screen, lift, os.path.join(user_dir, f"logo_preview_{board}.png"))
        print(f"background = #{bg_hex}, invert = {invert}")
        return

    src_path = args.src or os.path.join(ROOT, "assets", "logo_src.png")
    out_c = args.out_c or os.path.join(ROOT, "assets", "logo.cpp")
    out_h = args.out_h or os.path.join(ROOT, "assets", "logo.h")
    invert = not args.no_invert
    bg_hex = args.bg or ("000000" if invert else "FFFFFF")
    bg = tuple(int(bg_hex[i : i + 2], 16) for i in (0, 2, 4))
    im = fit(load_source(src_path, invert, bg_hex), bg, args.max_width or 204, args.max_height or 140)
    write_c(im, bg, src_path, out_c, out_h)
    print(f"background = #{bg_hex}, invert = {invert}")


if __name__ == "__main__":
    main()
