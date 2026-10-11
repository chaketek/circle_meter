"""本体を再起動して起動ログを読み、書き込みが成功したか・設定が効いたか・CAN を受信できているかを判定する。

配布先ユーザー向けの Skill（flash-meter / customize-meter）と手順書（docs/user/）から使う（SWR-67 / DOC-40 §2.6）。
CAN へは何も送らない（PC から本体のシリアルを読むだけ）。

使い方:
    python tools/meter_log.py                 # 本体を探し、再起動して 12 秒読む
    python tools/meter_log.py --seconds 20    # 長く読む（車両の受信を見るとき）
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

CH343_VID = 0x1A86  # LCD-2.1 の UART Type-C（CH343P）

STAT_RE = re.compile(r"rx=(?P<rx>\d+) f/s=(?P<fps>\d+).*?tec=(?P<tec>\d+) rec=(?P<rec>\d+)")
SENSOR_RE = re.compile(r"sensor=(?P<sensor>\S+)")


def find_port(explicit):
    if explicit:
        return explicit
    for p in list_ports.comports():
        if p.vid == CH343_VID:
            return p.device
    ports = [f"{p.device} ({p.description})" for p in list_ports.comports()]
    print("本体が見つかりません。UART と書かれた USB-C に、データ通信できるケーブルで繋いでください。")
    print(f"  見えているポート: {ports or 'なし'}")
    sys.exit(2)


def read_log(port, seconds):
    """RTS で EN を引いて再起動し、起動直後から読む（LCD-2.1 の自動書き込み回路）。"""
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
    ser.rts = True
    time.sleep(0.15)
    ser.rts = False
    lines = []
    t0 = time.time()
    while time.time() - t0 < seconds:
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

    source = find("signal source:")
    sim = source and "SIMULATOR" in source
    if source:
        kind = "デモ（CAN を使わない）" if sim else "車両用（CAN を受信する）"
        print(f"OK  種類: {kind}  [{source.split(':', 1)[1].strip()}]")

    user = find("user config:")
    if user:
        print(f"OK  ユーザー設定: {'あり' if 'present' in user else 'なし（既定の表示）'}")
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
    ap.add_argument("--seconds", type=float, default=12.0)
    ap.add_argument("--raw", action="store_true", help="判定せずにログをそのまま出す")
    args = ap.parse_args()

    port = find_port(args.port)
    print(f"{port} の本体を再起動して {args.seconds:.0f} 秒ログを読みます…")
    lines = read_log(port, args.seconds)
    if args.raw:
        print("\n".join(lines))
        return
    if not lines:
        print("ログが 1 行も読めませんでした。ケーブルと本体の電源を確認してください。")
        sys.exit(2)
    sys.exit(0 if judge(lines) else 1)


if __name__ == "__main__":
    main()
