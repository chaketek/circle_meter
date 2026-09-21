"""PCAN インタフェースから rusEFI verbose broadcast を模擬送出する結合テスト用ツール。

DOC-30 の `IT-*` を実機で実行するための PC 側ハーネス（SWR-101）。
フレームのバイト配置は docs/13_ICD_rusefi_can.md に従う。

前提:
    PEAK の PCAN ドライバ（PCANBasic.dll）がインストールされていること
    python -m pip install --user python-can

使い方:
    python tools/pcan_send.py --mode idle
    python tools/pcan_send.py --mode sweep
    python tools/pcan_send.py --mode egt-danger
    python tools/pcan_send.py --mode dropout
    python tools/pcan_send.py --mode burst
    python tools/pcan_send.py --mode invalid
    python tools/pcan_send.py --mode replay --csv tools/replay/idle.csv
    python tools/pcan_send.py --listen 10        # IT-04: バス上の全フレームを観測

    --channel PCAN_USBBUS1  --bitrate 500000  --base 0x200  --period-ms 50

終端抵抗の注意 (RSK-02 / IT-10):
    ベンチでは PCAN と M5Stack CAN Unit の 2 ノードだけになる。
    多くの PCAN-USB は終端抵抗を内蔵しないため、CAN Unit 側の 120 ohm を
    有効にしたうえで、PCAN 側にも 120 ohm を追加すること（合計 60 ohm）。
    逆に車両へ接続するときは CAN Unit 側の終端を外す。
"""

import argparse
import math
import os
import struct
import sys
import time

try:
    import can
except ImportError:
    sys.exit("python-can が必要です: python -m pip install --user python-can")

# ---------------------------------------------------------------- スケーリング
# docs/13_ICD_rusefi_can.md §3 と一致させること。
LAMBDA_SCALE = 0.0001
EGT_SCALE_C = 5.0
TIMING_SCALE = 0.02
DUTY_SCALE = 0.5
MAP_SCALE = 1.0 / 30.0
BATT_SCALE = 0.001
TEMP_OFFSET = -40.0

OFF_STATUS = 0
OFF_SPEEDS = 1
OFF_SENSORS1 = 3
OFF_SENSORS2 = 4
OFF_FUELING3 = 7
OFF_EGTS = 9


def u16(v: float, scale: float) -> int:
    return max(0, min(0xFFFF, int(round(v / scale))))


def s16(v: float, scale: float) -> int:
    return max(-32768, min(32767, int(round(v / scale))))


def temp8(deg_c: float) -> int:
    return max(0, min(255, int(round(deg_c - TEMP_OFFSET))))


class Frames:
    """1 サイクル分のフレーム束を組み立てる。"""

    def __init__(self, base: int):
        self.base = base

    def status(self, cel: bool = False, lambda_protect: bool = False, gear: int = 0) -> tuple:
        flags = (0x08 if cel else 0) | (0x20 if lambda_protect else 0) | 0x02  # main relay on
        d = struct.pack("<HHBBH", 0, 0, flags, gear, 0)
        return (self.base + OFF_STATUS, d)

    def speeds(self, rpm: float, timing: float = 15.0, inj: float = 8.0, vss: float = 0.0) -> tuple:
        d = struct.pack(
            "<HhBBBB",
            u16(rpm, 1.0),
            s16(timing, TIMING_SCALE),
            int(inj / DUTY_SCALE),
            int(inj / DUTY_SCALE),
            int(vss),
            0,
        )
        return (self.base + OFF_SPEEDS, d)

    def sensors1(self, map_kpa: float, clt: float, iat: float, fuel_pct: float = 60.0) -> tuple:
        d = struct.pack(
            "<HBBBBBB",
            u16(map_kpa, MAP_SCALE),
            temp8(clt),
            temp8(iat),
            temp8(0),
            temp8(0),
            temp8(45),
            int(fuel_pct / 0.5),
        )
        return (self.base + OFF_SENSORS1, d)

    def sensors2(self, oil_kpa: float, oil_temp: float, batt_v: float) -> tuple:
        d = struct.pack(
            "<HHBBH",
            0,
            u16(oil_kpa, MAP_SCALE),
            temp8(oil_temp),
            temp8(30),
            u16(batt_v, BATT_SCALE),
        )
        return (self.base + OFF_SENSORS2, d)

    def fueling3(self, lam1: float, lam2: float = 0.0) -> tuple:
        d = struct.pack("<HHHH", u16(lam1, LAMBDA_SCALE), u16(lam2, LAMBDA_SCALE), 0, 0)
        return (self.base + OFF_FUELING3, d)

    def egts(self, egt1: float, egt2: float = 0.0) -> tuple:
        e1 = max(0, min(255, int(round(egt1 / EGT_SCALE_C))))
        e2 = max(0, min(255, int(round(egt2 / EGT_SCALE_C))))
        d = bytes([e1, e2, 0, 0, 0, 0, 0, 0])
        return (self.base + OFF_EGTS, d)


# ---------------------------------------------------------------- シナリオ
def scenario_value(mode: str, t: float):
    """経過秒 t における (lambda, egt_c, rpm) を返す。"""
    if mode == "idle":
        return 1.00, 500.0, 850.0
    if mode == "sweep":
        # 8 秒周期で λ 0.68 <-> 1.36 を往復
        phase = (math.sin(2 * math.pi * t / 8.0) + 1.0) / 2.0
        lam = 0.68 + phase * (1.36 - 0.68)
        return lam, 300.0 + phase * 700.0, 1000.0 + phase * 6000.0
    if mode == "egt-danger":
        # 600 -> 960 degC を 20 秒かけて上げ、そのあと戻す
        phase = (math.sin(2 * math.pi * t / 40.0 - math.pi / 2) + 1.0) / 2.0
        return 0.80, 600.0 + phase * 360.0, 4500.0
    return 1.00, 500.0, 850.0


def send_cycle(bus, f: Frames, lam: float, egt: float, rpm: float, extended: bool) -> int:
    msgs = [
        f.status(),
        f.speeds(rpm),
        f.sensors1(map_kpa=45.0 + rpm / 200.0, clt=87.0, iat=32.0),
        f.sensors2(oil_kpa=350.0, oil_temp=95.0, batt_v=13.9),
        f.fueling3(lam),
        f.egts(egt),
    ]
    for can_id, data in msgs:
        bus.send(can.Message(arbitration_id=can_id, data=data, is_extended_id=extended))
    return len(msgs)


def run_replay(bus, path: str, extended: bool, loop: bool) -> None:
    rows = []
    with open(path, encoding="utf-8") as fp:
        for line in fp:
            line = line.strip()
            if not line or line.startswith("#"):
                continue
            parts = [p.strip() for p in line.split(",")]
            t_ms = int(parts[0])
            can_id = int(parts[1], 16)
            dlc = int(parts[2])
            data = bytes(int(p, 16) for p in parts[3 : 3 + dlc])
            rows.append((t_ms, can_id, data))
    if not rows:
        sys.exit(f"{path} に有効な行がありません")

    print(f"replay {path}: {len(rows)} frames, loop={loop}")
    while True:
        t0 = time.perf_counter()
        for t_ms, can_id, data in rows:
            target = t0 + t_ms / 1000.0
            while time.perf_counter() < target:
                time.sleep(0.0005)
            bus.send(can.Message(arbitration_id=can_id, data=data, is_extended_id=extended))
        if not loop:
            return


def run_listen(bus, seconds: float) -> None:
    """IT-04: 本機が 1 フレームも送信しないことを確認する。

    メータの電源だけを入れた状態でこれを回し、観測が 0 件であること。
    Listen Only モードでは ACK も返さないため、こちらの送信も ACK エラーになる点に注意。
    """
    print(f"listening for {seconds:.0f}s ...  (IT-04: 本機由来のフレームが 0 件であること)")
    t0 = time.perf_counter()
    seen = {}
    while time.perf_counter() - t0 < seconds:
        msg = bus.recv(timeout=0.5)
        if msg is None:
            continue
        key = (msg.arbitration_id, msg.is_extended_id)
        seen[key] = seen.get(key, 0) + 1
        print(f"  {msg.arbitration_id:#05x}  dlc={msg.dlc}  {msg.data.hex(' ')}")
    print("---")
    if not seen:
        print("観測フレーム 0 件")
    for (cid, ext), n in sorted(seen.items()):
        print(f"  id={cid:#05x} ext={ext} count={n}")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument(
        "--mode",
        default="sweep",
        choices=["idle", "sweep", "egt-danger", "dropout", "burst", "invalid", "replay"],
    )
    ap.add_argument("--channel", default="PCAN_USBBUS1")
    ap.add_argument("--bitrate", type=int, default=500000)
    ap.add_argument("--base", default="0x200")
    ap.add_argument("--period-ms", type=int, default=50, help="送信周期。rusEFI の canSleepPeriodMs 相当")
    ap.add_argument("--extended", action="store_true", help="29bit 拡張 ID で送る")
    ap.add_argument("--csv", default=None, help="--mode replay のときの CSV")
    ap.add_argument("--no-loop", action="store_true", help="replay を 1 回だけ再生する")
    ap.add_argument("--listen", type=float, default=None, help="送信せず N 秒バスを観測する (IT-04)")
    args = ap.parse_args()

    base = int(args.base, 0)

    try:
        bus = can.Bus(interface="pcan", channel=args.channel, bitrate=args.bitrate)
    except Exception as exc:  # noqa: BLE001
        sys.exit(
            f"PCAN を開けませんでした: {exc}\n"
            "  - PEAK のドライバ（PCANBasic.dll）がインストールされているか\n"
            "  - チャンネル名が正しいか（既定 PCAN_USBBUS1）\n"
            "  - 他のアプリ（PCAN-View 等）がチャンネルを掴んでいないか"
        )

    try:
        if args.listen is not None:
            run_listen(bus, args.listen)
            return

        if args.mode == "replay":
            if not args.csv:
                sys.exit("--mode replay には --csv が必要です")
            run_replay(bus, args.csv, args.extended, not args.no_loop)
            return

        f = Frames(base)
        period = args.period_ms / 1000.0
        if args.mode == "burst":
            period = 0.005  # IT-03: 200 Hz で全フレームを投げ、取りこぼしを確認する

        print(
            f"mode={args.mode} base={base:#05x} bitrate={args.bitrate} "
            f"period={period * 1000:.0f}ms ext={args.extended}\nCtrl+C で停止"
        )

        t0 = time.perf_counter()
        sent = 0
        next_at = t0
        last_report = t0
        while True:
            now = time.perf_counter()
            t = now - t0

            if args.mode == "dropout":
                # IT-02: 5 秒送って 5 秒止める。止めている間に鮮度切れ -> NO SIGNAL を確認する
                if int(t / 5.0) % 2 == 1:
                    if now >= next_at:
                        next_at += period
                    time.sleep(0.002)
                    if now - last_report >= 1.0:
                        last_report = now
                        print(f"  t={t:6.1f}s  [停止中]")
                    continue

            if now >= next_at:
                next_at += period
                if args.mode == "invalid":
                    # UT-09 / IT-02 の実機確認: λ=0（センサ未構成）、EGT=0、DLC 不足
                    bus.send(
                        can.Message(
                            arbitration_id=base + OFF_FUELING3,
                            data=struct.pack("<HHHH", 0, 0, 0, 0),
                            is_extended_id=args.extended,
                        )
                    )
                    bus.send(
                        can.Message(
                            arbitration_id=base + OFF_EGTS,
                            data=bytes(8),
                            is_extended_id=args.extended,
                        )
                    )
                    # DLC 不足フレーム（破棄され badDlc が増えること）
                    bus.send(
                        can.Message(
                            arbitration_id=base + OFF_SPEEDS,
                            data=bytes(4),
                            is_extended_id=args.extended,
                        )
                    )
                    sent += 3
                else:
                    lam, egt, rpm = scenario_value(args.mode, t)
                    sent += send_cycle(bus, f, lam, egt, rpm, args.extended)
            else:
                time.sleep(0.0005)

            if now - last_report >= 1.0:
                last_report = now
                lam, egt, rpm = scenario_value(args.mode, t)
                print(f"  t={t:6.1f}s  sent={sent:7d}  lambda={lam:.3f}  egt={egt:4.0f}C  rpm={rpm:5.0f}")

    except KeyboardInterrupt:
        print("\n停止しました")
    finally:
        bus.shutdown()


if __name__ == "__main__":
    main()
