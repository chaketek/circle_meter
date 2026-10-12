"""起動ロゴの差し替え（SWR-67 / SWR-68 / SWD-14）。PlatformIO の pre スクリプト。

tools/make_logo.py --user が機種ごとに作る user/generated/<機種>/user_logo.cpp / user_logo.h があれば、
既定のロゴ（assets/lcd21/logo.cpp / assets/logo.cpp）の代わりにビルドへ入れ、インクルードパスと CM_USER_LOGO を足す。
スプラッシュは CM_USER_LOGO のとき user_logo.h を読む。ヘッダの名前を既定の logo.h と分けているので、
インクルードパスの順に頼らない。CM_USER_LOGO の有無でコンパイルの引数が変わるので、切り替えると作り直される。
"""

import os

Import("env")  # noqa: F821  (PlatformIO が注入する)

# 環境名の先頭で機種を決める（lcd21 / lcd21_sim / lcd21_1m -> lcd21、m5dial / m5dial_sim -> m5dial）
pioenv = env.subst("$PIOENV")  # noqa: F821
board = "lcd21" if pioenv.startswith("lcd21") else ("m5dial" if pioenv.startswith("m5dial") else None)
DEFAULT_LOGO = {"lcd21": "+<../assets/lcd21/logo.cpp>", "m5dial": "+<../assets/logo.cpp>"}

root = env.subst("$PROJECT_DIR")  # noqa: F821
if os.path.isfile(os.path.join(root, "user", "logo.cpp")):
    # 2026-10-12 より前の --user は user/logo.cpp（LCD-2.1 専用）を作っていた。今は使わない
    print("logo: user/logo.cpp は古い形式です。python tools/make_logo.py --user で作り直してください（今回は既定のロゴ）")

if board:
    gen_dir = os.path.join(root, "user", "generated", board)
    has_c = os.path.isfile(os.path.join(gen_dir, "user_logo.cpp"))
    has_h = os.path.isfile(os.path.join(gen_dir, "user_logo.h"))
    if has_c != has_h:
        raise SystemExit(
            f"user/generated/{board}/ の user_logo.cpp と user_logo.h は 2 つそろっている必要があります。"
            "python tools/make_logo.py --user で作り直すか、user/generated フォルダを削除して既定のロゴに戻してください"
        )
    if has_c:
        # build_src_filter は文字列のリストで渡ってくる（1 行に複数のパターンが入る）
        flt = env.get("SRC_FILTER") or env.GetProjectOption("build_src_filter")  # noqa: F821
        if isinstance(flt, str):
            flt = [flt]
        default = DEFAULT_LOGO[board]
        if any(default in f for f in flt):
            user_src = f"+<../user/generated/{board}/user_logo.cpp>"
            env.Replace(SRC_FILTER=[f.replace(default, user_src) for f in flt])  # noqa: F821
            env.Append(CPPPATH=[gen_dir], CPPDEFINES=["CM_USER_LOGO"])  # noqa: F821
            print(f"logo: user/generated/{board}/user_logo.cpp (user logo)")
