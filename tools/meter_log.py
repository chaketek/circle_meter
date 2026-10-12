"""本体の起動ログを読み、書き込みが成功したか・設定が効いたか・CAN を受信できているかを判定する。

配布先ユーザー向けの Skill（flash-meter / customize-meter）と手順書（docs/user/）から使う（SWR-67 / DOC-40 §2.6）。
CAN へは何も送らない（PC から本体のシリアルを読むだけ）。

LCD-2.1 は本体を再起動して起動直後から読む。M5Dial（ネイティブ USB）はポートを開いても再起動しないので、
シリアルから 'i' を送って起動ログを出し直させる（SWR-68。LCD-2.1 でも起動ログが読めなかったときは同じく 'i' を送る）。

使い方:
    python tools/meter_log.py                 # 本体を探し（LCD-2.1 -> M5Dial の順）、12 秒読む
    python tools/meter_log.py --seconds 20    # 長く読む（車両の受信を見るとき）
    python tools/meter_log.py --board m5dial  # M5Dial を探す
    python tools/meter_log.py --port COM10    # ポートを指定
    python tools/meter_log.py --raw           # 判定せずにログをそのまま出す

終了コード: 0 = 問題なし / 1 = 要確認の項目あり / 2 = 本体が見つからない・ログが読めない
"""

import argparse
import re
import sys
import time

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    sys.exit("pyserial が必要です: python -m pip install --user pyserial")

CH343_VID = 0x1A86   # LCD-2.1 の UART Type-C（CH343P）
ESP_USB_VID = 0x303A  # M5Dial（ESP32-S3 のネイティブ USB）
BOARD_VIDS = {"lcd21": CH343_VID, "m5dial": ESP_USB_VID}

STAT_RE = re.compile(r"rx=(?P<rx>\d+) f/s=(?P<fps>\d+).*?tec=(?P<tec>\d+) rec=(?P<rec>\d+)")
SENSOR_RE = re.compile(r"sensor=(?P<sensor>\S+)")


def find_port(explicit, board):
    """(ポート, 機種) を返す。board が auto なら LCD-2.1 -> M5Dial の順に探す。"""
    if explicit:
        vid = next((p.vid for p in list_ports.comports() if p.device == explicit), None)
        guess = "m5dial" if vid == ESP_USB_VID else ("lcd21" if board == "auto" else board)
        return explicit, guess
    order = ["lcd21", "m5dial"] if board == "auto" else [board]
    for b in order:
        for p in list_ports.comports():
            if p.vid == BOARD_VIDS[b]:
                return p.device, b
    ports = [f"{p.device} ({p.description})" for p in list_ports.comports()]
    print("本体が見つかりません。データ通信できるケーブルで繋いでください"
          "（LCD-2.1 は UART と書かれた USB-C、M5Dial は本体の USB-C）。")
    print(f"  見えているポート: {ports or 'なし'}")
    sys.exit(2)


def read_log(port, seconds, board):
    """LCD-2.1: RTS で EN を引いて再起動し、起動直後から読む（自動書き込み回路）。
    M5Dial: 再起動せず 'i' で起動ログを出し直させる。どちらも起動ログが読めなければ途中で 'i' を送る。"""
    ser = serial.Serial()
    ser.port = port
    ser.baudrate = 115200
    ser.timeout = 0.3
    ser.dtr = False
    ser.rts = False
    try:
        ser.open()
    except serial.SerialException as e:
        print(f"{port} を開けません（{e}）。")
        print("  別のソフト（シリアルモニタ・書き込み中の処理）がポートを使っていないか確認してください。")
        sys.exit(2)
    if board == "lcd21":
        ser.rts = True
        time.sleep(0.15)
        ser.rts = False
        ask_at = 6.0  # 起動ログ（スプラッシュ明けの ui ready まで約 3.5 秒）が来なければ 'i' で頼む
    else:
        ask_at = 0.5
    lines = []
    asked = False
    t0 = time.time()
    while time.time() - t0 < seconds:
        if not asked and time.time() - t0 >= ask_at and not any("circle_meter " in ln for ln in lines):
            ser.write(b"i")
            asked = True
        raw = ser.readline()
        if raw:
            lines.append(raw.decode("utf-8", "replace").rstrip())
    ser.close()
    return lines


def judge(lines):
    ok = True
    find = lambda key: next((ln for ln in lines if key in ln), None)  # noqa: E731

    banner = find("circle_meter ")
    if not banner:
        print("NG  起動メッセージが読めませんでした（書き込みに失敗したか、別のプログラムが入っています）")
        return False
    print(f"OK  ファームウェア: {banner.strip()}")
    print(f"OK  機種: {'M5Dial' if '(M5Dial' in banner else 'LCD-2.1'}")

    source = find("signal source:")
    sim = source and "SIMULATOR" in source
    if source:
        kind = "デモ（CAN を使わない）" if sim else "車両用（CAN を受信する）"
        print(f"OK  種類: {kind}  [{source.split(':', 1)[1].strip()}]")

    user = find("user config:")
    if user:
        print(f"OK  ユーザー設定: {'あり' if 'present' in user else 'なし（既定の表示）'}")
        if "present" in user:
            print("      （色の設定はここには出ません。画面で確かめてください）")
        idx = lines.index(user)
        for ln in lines[idx + 1: idx + 3]:
            if "applied:" in ln:
                print(f"      反映した項目: {ln.split(':', 1)[1].strip()}")
            if "REJECTED" in ln:
                ok = False
                print(f"NG    範囲外のため既定値のままの項目: {ln.split(':', 1)[1].strip()}")
                print("      user/user_config.h の値を見直してください（docs/user/05_customize.md）")

    if not find("ui ready"):
        ok = False
        print("NG  画面の初期化が終わったログがありません（display / lvgl init FAILED が出ていないか確認）")

    if sim or not source:
        return ok

    stats = [m for m in (STAT_RE.search(ln) for ln in lines) if m]
    if not stats:
        print("--  CAN の受信状況のログがまだ出ていません（--seconds を長くしてください）")
        return ok
    last = stats[-1]
    fps, rec = int(last["fps"]), int(last["rec"])
    sensor = next((m["sensor"] for m in (SENSOR_RE.search(ln) for ln in reversed(lines)) if m), "?")
    if fps > 0:
        print(f"OK  CAN 受信: {fps} フレーム/秒、λ センサの状態 {sensor}")
    elif rec >= 96:
        ok = False
        print(f"NG  CAN を受信できていません（受信エラー rec={rec}）。")
        print("    通信速度（500k / 1M）が車両と合っていない可能性が高いです。")
        print("    もう一方の速度で書き込み直すか、user_config.h の USER_CAN_BITRATE_KBPS を見直してください。")
    else:
        ok = False
        print(f"NG  CAN のフレームが届いていません（受信エラーなし rec={rec}）。")
        print("    配線（CAN_H / CAN_L、Grove ケーブル）、車両のキー ON、ECU / WBO の CAN 出力設定を確認してください。")
    return ok


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--port")
    ap.add_argument("--board", choices=["auto", "lcd21", "m5dial"], default="auto",
                    help="探す機種（既定: LCD-2.1 -> M5Dial の順）")
    ap.add_argument("--seconds", type=float, default=12.0)
    ap.add_argument("--raw", action="store_true", help="判定せずにログをそのまま出す")
    args = ap.parse_args()

    port, board = find_port(args.port, args.board)
    how = "再起動して" if board == "lcd21" else "（M5Dial は再起動せず起動ログを出し直させて）"
    print(f"{port} の本体を{how} {args.seconds:.0f} 秒ログを読みます…")
    lines = read_log(port, args.seconds, board)
    if args.raw:
        print("\n".join(lines))
        return
    if not lines:
        print("ログが 1 行も読めませんでした。ケーブルと本体の電源を確認してください。")
        sys.exit(2)
    sys.exit(0 if judge(lines) else 1)


if __name__ == "__main__":
    main()
