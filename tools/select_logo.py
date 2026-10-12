"""起動ロゴの差し替え（SWR-67 / SWD-14）。PlatformIO の pre スクリプト。

user/logo.cpp と user/logo.h（tools/make_logo.py --user が作る）があれば、assets/lcd21/logo.cpp の代わりに
ビルドへ入れる。logo.h は platformio.ini で -I user を -I assets/lcd21 より前に置いているので自動で差し替わる。
片方だけあると、宣言と実体の大きさが食い違って画面が崩れるので、ビルドを止める。
"""

import os

Import("env")  # noqa: F821  (PlatformIO が注入する)

root = env.subst("$PROJECT_DIR")  # noqa: F821
user_c = os.path.join(root, "user", "logo.cpp")
user_h = os.path.join(root, "user", "logo.h")
has_c, has_h = os.path.isfile(user_c), os.path.isfile(user_h)

if has_c != has_h:
    raise SystemExit(
        "user/logo.cpp と user/logo.h は 2 つそろっている必要があります。"
        "python tools/make_logo.py --user で作り直すか、両方を削除して既定のロゴに戻してください"
    )

# 既定のロゴと user のロゴを切り替えたら、logo.h を読む page_splash を確実に作り直す。
# logo.h はインクルードパスの順で差し替わるだけなので、依存関係の追跡に頼ると古い大きさのまま残りうる
build_dir = env.subst("$BUILD_DIR")  # noqa: F821
stamp = os.path.join(build_dir, "logo_source.txt")
mode = "user" if has_c else "default"
try:
    with open(stamp, encoding="utf-8") as f:
        previous = f.read().strip()
except OSError:
    previous = None
if previous != mode:
    for dirpath, _, files in os.walk(build_dir):
        for name in files:
            if name.startswith("page_splash") and name.endswith(".o"):
                os.remove(os.path.join(dirpath, name))
    os.makedirs(build_dir, exist_ok=True)
    with open(stamp, "w", encoding="utf-8") as f:
        f.write(mode)

if has_c:
    # build_src_filter は文字列のリストで渡ってくる（1 行に複数のパターンが入る）
    flt = env.get("SRC_FILTER") or env.GetProjectOption("build_src_filter")  # noqa: F821
    if isinstance(flt, str):
        flt = [flt]
    default = "+<../assets/lcd21/logo.cpp>"
    if any(default in f for f in flt):
        env.Replace(SRC_FILTER=[f.replace(default, "+<../user/logo.cpp>") for f in flt])  # noqa: F821
        print("logo: user/logo.cpp (user logo)")
