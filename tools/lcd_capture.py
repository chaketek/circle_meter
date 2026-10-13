"""UVC カメラで LCD を撮影する（表示の目視確認用）。

LCD を外付けの UVC カメラで撮影し、画像ファイルとして保存する。保存した画像は
Claude Code から直接開いて確認できる（docs/40_SUP8 §2.4.1）。

    python tools/lcd_capture.py                       1 枚撮って captures/lcd.jpg に保存
    python tools/lcd_capture.py --out captures/a.jpg
    python tools/lcd_capture.py --burst 12            連続 12 枚（captures/lcd_001.jpg ...）
    python tools/lcd_capture.py --list                使えるカメラの一覧
    python tools/lcd_capture.py --wb auto             ホワイトバランスを自動に戻して撮る（既定は 5800 K に固定）
    python tools/lcd_capture.py --wb keep             カメラの設定を触らずに撮る

--burst はカメラのフレームレートで連続撮影する（30 fps なら約 33 ms 間隔）。
NVS 書き込みのような短時間の表示の乱れ（RSK-14）を捉えるために使う。

前提: ffmpeg が PATH にあること（Windows の DirectShow を使う）。

【ホワイトバランス】既定で撮影前に 5800 K の固定に切り替える（OpenCV の DirectShow 経由。2026-10-13）。
自動のままだと、黒地に発光色が散る画面では白が水色に寄り、色の判断を誤る。LCD-2.1 の目盛り数字（白）で
2800〜6500 K を比べ、5800 K が最も白に近く、λ ゾーンの色も自然だった（6500 K では緑に転ぶ）。
設定はカメラに残るので、ffmpeg の撮影にもそのまま効く。露出には触らない（人が目視に合わせて調整しているため）。
OpenCV（cv2）が無ければ警告だけ出して、設定を触らずに撮る。

【プライバシー】PC 内蔵カメラ（"Integrated Camera" など）は使わせない。
LCD の確認に必要なのは外付けカメラだけで、内蔵カメラは人が写るため。
"""

import argparse
import os
import re
import shutil
import subprocess
import sys

DEFAULT_CAMERA = "5MP USB Camera"
# 名前にこれらを含むカメラは拒否する（PC 内蔵・仮想カメラ）
FORBIDDEN_WORDS = ("integrated", "built-in", "builtin", "内蔵", "virtual")

for _stream in (sys.stdout, sys.stderr):
    try:
        _stream.reconfigure(errors="replace")
    except Exception:
        pass


def need_ffmpeg() -> str:
    exe = shutil.which("ffmpeg")
    if not exe:
        sys.exit("ffmpeg が PATH に見つかりません。")
    return exe


def list_cameras(ffmpeg: str) -> list:
    r = subprocess.run([ffmpeg, "-hide_banner", "-list_devices", "true", "-f", "dshow", "-i", "dummy"],
                       capture_output=True, text=True, encoding="utf-8", errors="replace")
    names = []
    for line in (r.stderr or "").splitlines():
        m = re.search(r'"([^"]+)"\s+\(video\)', line)
        if m:
            names.append(m.group(1))
    return names


DEFAULT_WB_K = 5800


def set_white_balance(ffmpeg: str, camera: str, wb: str) -> None:
    """撮影前にホワイトバランスを設定する。wb は "keep" / "auto" / 色温度（K）。

    OpenCV はカメラを名前ではなく番号で開くので、ffmpeg（DirectShow）の一覧の順番から番号を決める。
    OpenCV の DirectShow も同じ列挙を使う。番号を取り違えて PC 内蔵カメラを開かないよう、その位置の名前を確かめる。
    """
    if wb == "keep":
        return
    try:
        import cv2
    except ImportError:
        print("（OpenCV が無いのでホワイトバランスは変えずに撮ります: python -m pip install --user opencv-python）")
        return
    names = list_cameras(ffmpeg)
    if camera not in names:
        return  # 撮影側でエラーにする
    idx = names.index(camera)
    if any(w in names[idx].lower() for w in FORBIDDEN_WORDS):
        sys.exit(f"'{names[idx]}' は PC 内蔵/仮想カメラなので使えません。")
    cap = cv2.VideoCapture(idx, cv2.CAP_DSHOW)
    try:
        if not cap.isOpened():
            print("（カメラを開けなかったのでホワイトバランスは変えずに撮ります）")
            return
        if wb == "auto":
            cap.set(cv2.CAP_PROP_AUTO_WB, 1)
        else:
            cap.set(cv2.CAP_PROP_AUTO_WB, 0)
            cap.set(cv2.CAP_PROP_WB_TEMPERATURE, int(wb))
    finally:
        cap.release()


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--camera", default=DEFAULT_CAMERA, help=f"カメラ名（既定: {DEFAULT_CAMERA}）")
    ap.add_argument("--out", default=os.path.join("captures", "lcd.jpg"))
    ap.add_argument("--burst", type=int, default=1, help="連続撮影の枚数")
    ap.add_argument("--size", default="1280x720")
    ap.add_argument("--list", action="store_true", help="カメラ一覧を表示して終了")
    ap.add_argument("--wb", default=str(DEFAULT_WB_K),
                    help=f"ホワイトバランス: 色温度 K（既定 {DEFAULT_WB_K}）/ auto / keep（触らない）")
    args = ap.parse_args()

    ffmpeg = need_ffmpeg()

    if args.list:
        for n in list_cameras(ffmpeg):
            banned = any(w in n.lower() for w in FORBIDDEN_WORDS)
            print(f"  {'[使用不可: 内蔵/仮想]' if banned else '[OK]':<18} {n}")
        return

    if any(w in args.camera.lower() for w in FORBIDDEN_WORDS):
        sys.exit(f"'{args.camera}' は PC 内蔵/仮想カメラなので使えません。外付けカメラを指定してください。")

    if args.wb not in ("auto", "keep") and not args.wb.isdigit():
        sys.exit("--wb は色温度（例 5800）/ auto / keep で指定してください")
    set_white_balance(ffmpeg, args.camera, args.wb)

    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)

    if args.burst <= 1:
        # 先頭のフレームは露出が合っていないことがあるので数枚捨てて最後の 1 枚を保存する
        cmd = [ffmpeg, "-hide_banner", "-loglevel", "error", "-y", "-f", "dshow", "-video_size", args.size,
               "-i", f"video={args.camera}", "-frames:v", "6", "-update", "1", args.out]
        out_desc = args.out
    else:
        base, ext = os.path.splitext(args.out)
        out_desc = f"{base}_001{ext} ... {base}_{args.burst:03d}{ext}"
        cmd = [ffmpeg, "-hide_banner", "-loglevel", "error", "-y", "-f", "dshow", "-video_size", args.size,
               "-i", f"video={args.camera}", "-frames:v", str(args.burst), f"{base}_%03d{ext}"]

    r = subprocess.run(cmd, capture_output=True, text=True, encoding="utf-8", errors="replace")
    if r.returncode != 0:
        print(r.stderr.strip())
        sys.exit(f"撮影に失敗しました（カメラ名を --list で確認してください）: {args.camera}")
    print(f"saved: {out_desc}")


if __name__ == "__main__":
    main()
