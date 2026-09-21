"""SWR-102: ビルド時にファームウェア版数を埋め込む。

git describe の結果を CM_FW_VERSION マクロとして渡す。
git が使えない環境（tarball 展開など）では "unknown" になるが、ビルドは失敗させない。
"""

import subprocess

Import("env")  # noqa: F821  (PlatformIO が注入する)


def git_describe() -> str:
    try:
        out = subprocess.check_output(
            ["git", "describe", "--tags", "--always", "--dirty"],
            stderr=subprocess.DEVNULL,
        )
        return out.decode("utf-8", "replace").strip()
    except Exception:
        return "unknown"


version = git_describe()
print(f"circle_meter firmware version: {version}")
env.Append(CPPDEFINES=[("CM_FW_VERSION", env.StringifyMacro(version))])  # noqa: F821
