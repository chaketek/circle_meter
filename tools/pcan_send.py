"""PCAN インタフェースから rusEFI verbose broadcast を模擬送出する結合テスト用ツール。

DOC-30 の `IT-*` を実機で実行するための PC 側ハーネス（SWR-101）。
フレームのバイト配置は docs/13_ICD_rusefi_can.md に従う。

前提:
    PEAK の PCAN ドライバ（PCANBasic.dll）がインストールされていること
    python -m pip install --user python-can

使い方:
    python tools/pcan_send.py --mode drive        実走模擬（既定。20 秒周期で繰り返す）
    python tools/pcan_send.py --mode drive --lambda-tau-ms 0    λ の応答遅れを無効化
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

STOICH = 14.7  # ガソリン。DOC-23 §3.2 と合わせる

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


def lerp(a: float, b: float, u: float) -> float:
    return a + (b - a) * u


class Sample:
    """1 サイクル分の車両状態。"""

    def __init__(self, phase="", afr=STOICH, egt=500.0, rpm=850.0, speed=0.0, gear=0,
                 map_kpa=35.0, inj_duty=5.0, timing=15.0, clt=87.0, iat=32.0, fuel_cut=False):
        self.phase = phase
        self.afr = afr
        self.egt = egt
        self.rpm = rpm
        self.speed = speed
        self.gear = gear
        self.map_kpa = map_kpa
        self.inj_duty = inj_duty
        self.timing = timing
        self.clt = clt
        self.iat = iat
        self.fuel_cut = fuel_cut

    @property
    def lam(self) -> float:
        return self.afr / STOICH


class Frames:
    """1 サイクル分のフレーム束を組み立てる。"""

    def __init__(self, base: int):
        self.base = base

    def status(self, gear: int = 0, cel: bool = False, lambda_protect: bool = False) -> tuple:
        flags = (0x08 if cel else 0) | (0x20 if lambda_protect else 0) | 0x02  # main relay on
        d = struct.pack("<HHBBH", 0, 0, flags, gear, 0)
        return (self.base + OFF_STATUS, d)

    def speeds(self, rpm: float, timing: float = 15.0, inj: float = 8.0, vss: float = 0.0) -> tuple:
        d = struct.pack(
            "<HhBBBB",
            u16(rpm, 1.0),
            s16(timing, TIMING_SCALE),
            max(0, min(200, int(inj / DUTY_SCALE))),
            max(0, min(200, int(inj / DUTY_SCALE))),
            max(0, min(255, int(vss))),
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
        return (self.base + OFF_EGTS, bytes([e1, e2, 0, 0, 0, 0, 0, 0]))


# ---------------------------------------------------------------- 実走模擬
class DriveCycle:
    """アイドル → 1-2-3 速加速 → 60 km/h 定常 → 減速（燃料カット）→ アイドル、を 20 秒で繰り返す。

    空燃比の作り方は実際の挙動に合わせている。

    | 場面 | AFR | 理由 |
    |---|---|---|
    | アイドル / 定常 | 14.7 付近を小さく振動 | 閉ループ制御 |
    | 加速の踏み始め | 一瞬 16 付近までリーン | 加速増量が間に合わない過渡リーンスパイク |
    | 全開加速 | 13 前後 | 出力空燃比。燃料で冷やす |
    | シフト中（スロットル閉） | 17 台 | オーバーラン |
    | 減速 | 20 に張り付き | 燃料カット（噴射停止） |
    | 燃料カット復帰 | 12.5 前後 | 復帰時の一時的なリッチ |

    λ には一次遅れ（既定 300 ms）を掛ける。実際のワイドバンドセンサと ECU のフィルタには
    応答遅れがあり、バス上に階段状の λ が出ることはないため。
    リーンスパイクと復帰リッチは、この遅れで鈍ったあとに上表の値へ届くよう
    持続時間を取ってある（実車でも過渡増量は一瞬では終わらない）。

    排気温度は空燃比と負荷から目標値を作り、一次遅れを掛ける（熱容量の模擬）。
    目標値の基準は DOC-23 §4 の想定に合わせた。
      アイドル 400 / 定常 14.7 で 600 / リッチ化で約 -100 / 燃料カットで 400
    """

    # 回転数は「ギアごとの rpm/(km/h) 係数 × 車速」で決めており、同じギア・同じ車速なら
    # 必ず同じ回転数になるようにしてある（そうしないと 3 速 60 km/h で回転数だけ変わる、
    # といった物理的にありえない表示になる）。係数はロードスター NA 相当の概算値。
    #   1速 200 / 2速 115 / 3速 72 / 4速 49  [rpm per km/h]
    # 60 km/h までの加速は 1-2-3 速、巡航は 4 速へシフトアップする。
    #
    # (終了時刻, 名前, ギア, rpm0, rpm1, v0, v1, afr0, afr1, MAP, 噴射duty, EGT目標, 燃料カット)
    PHASES = [
        (3.00, "IDLE",       0,  850,  850,  0,  0, 14.7, 14.7, 35,  3, 400, False),
        # リーンスパイクと復帰リッチは、λ の一次遅れ (LAMBDA_TAU_MS) で鈍ったあとに
        # 指定値へ届くよう持続時間を取ってある。実車でも過渡増量は一瞬では終わらない。
        (3.30, "TIP-IN",     1, 1100, 1400,  0,  5, 14.7, 16.4, 70, 12, 520, False),
        (3.60, "LEAN SPIKE", 1, 1400, 2000,  5, 10, 16.4, 16.4, 88, 22, 620, False),
        (3.95, "SPIKE DECAY", 1, 2000, 2600, 10, 13, 16.4, 13.0, 95, 30, 700, False),
        (5.60, "1ST WOT",    1, 2600, 5200, 13, 26, 13.0, 13.0, 98, 42, 800, False),
        (5.90, "SHIFT 1-2",  0, 5200, 3000, 26, 26, 17.5, 17.5, 30,  4, 640, False),
        (8.10, "2ND WOT",    2, 3000, 5175, 26, 45, 13.0, 13.0, 98, 45, 820, False),
        (8.40, "SHIFT 2-3",  0, 5175, 3240, 45, 45, 17.5, 17.5, 30,  4, 660, False),
        (10.20, "3RD WOT",   3, 3240, 4320, 45, 60, 13.2, 13.2, 96, 40, 800, False),
        (10.50, "SHIFT 3-4", 0, 4320, 2940, 60, 60, 17.0, 17.0, 30,  4, 660, False),
        (14.00, "CRUISE 60", 4, 2940, 2940, 60, 60, 14.7, 14.7, 50, 14, 600, False),
        # 減速は燃料カットのまま 4 -> 3 -> 2 とシフトダウンする。
        # ダウンシフトの瞬間に回転が上がるのは実車どおり。
        (15.60, "CUT 4TH",   4, 2940, 1715, 60, 35, 20.0, 20.0, 25,  0, 400, True),
        (16.50, "CUT 3RD",   3, 2520, 1440, 35, 20, 20.0, 20.0, 25,  0, 400, True),
        (17.20, "CUT 2ND",   2, 2300, 1035, 20,  9, 20.0, 20.0, 25,  0, 400, True),
        (18.40, "RECOVER",   0, 1300, 1050,  9,  2, 12.1, 12.1, 45, 16, 480, False),
        (20.00, "IDLE RET",  0, 1050,  850,  2,  0, 13.5, 14.7, 35,  4, 420, False),
    ]
    PERIOD_S = 20.0

    # 排気温度の一次遅れ。熱電対は温まるより冷めるほうが遅い。
    TAU_UP_MS = 1200.0
    TAU_DOWN_MS = 1800.0

    # 空燃比の一次遅れ。実際のワイドバンドセンサと ECU のフィルタには応答遅れがあり、
    # バス上に階段状の λ が出ることはない。表示側の見え方を実物に近づけるために入れる。
    # 排気温度の目標値は「実際の燃焼」で決まるのでフィルタ前の空燃比から計算する。
    LAMBDA_TAU_MS = 300.0

    def __init__(self, lambda_tau_ms: float = LAMBDA_TAU_MS):
        self.egt = 400.0
        self.afr_tau_ms = lambda_tau_ms
        self.afr_filt = None

    def sample(self, t: float, dt_ms: float) -> Sample:
        tc = t % self.PERIOD_S
        t0 = 0.0
        for (t1, name, gear, r0, r1, v0, v1, a0, a1, mapk, inj, egt_base, cut) in self.PHASES:
            if tc < t1:
                u = (tc - t0) / (t1 - t0) if t1 > t0 else 0.0
                afr = lerp(a0, a1, u)
                # 閉ループ中は目標付近で小さく振動する
                if name in ("IDLE", "CRUISE 60", "IDLE RET"):
                    afr += 0.15 * math.sin(2 * math.pi * 0.8 * t)

                # EGT 目標: 場面ごとの基準に空燃比ぶんの補正を足す。
                # 14.7 で 0、リッチ側で下がり、リーン側で上がる（約 -66.7 degC / AFR）。
                if cut:
                    target = egt_base  # 燃焼していないので空燃比によらない
                else:
                    target = egt_base + (afr - STOICH) * 66.7
                    target = max(300.0, min(1000.0, target))

                tau = self.TAU_UP_MS if target > self.egt else self.TAU_DOWN_MS
                alpha = dt_ms / (tau + dt_ms) if dt_ms > 0 else 0.0
                self.egt += alpha * (target - self.egt)

                # λ センサの応答遅れ。バスに出るのはこちらの値。
                if self.afr_filt is None or self.afr_tau_ms <= 0.0:
                    self.afr_filt = afr
                else:
                    a = dt_ms / (self.afr_tau_ms + dt_ms) if dt_ms > 0 else 0.0
                    self.afr_filt += a * (afr - self.afr_filt)

                return Sample(
                    phase=name,
                    afr=self.afr_filt,
                    egt=self.egt,
                    rpm=lerp(r0, r1, u),
                    speed=lerp(v0, v1, u),
                    gear=gear,
                    map_kpa=mapk,
                    inj_duty=0.0 if cut else inj,
                    timing=8.0 if cut else lerp(30.0, 16.0, min(1.0, inj / 45.0)),
                    fuel_cut=cut,
                )
            t0 = t1
        return Sample()


def simple_sample(mode: str, t: float) -> Sample:
    """drive 以外のモード。"""
    if mode == "idle":
        return Sample("IDLE", afr=STOICH, egt=500.0, rpm=850.0)
    if mode == "sweep":
        phase = (math.sin(2 * math.pi * t / 8.0) + 1.0) / 2.0
        lam = 0.68 + phase * (1.36 - 0.68)
        return Sample("SWEEP", afr=lam * STOICH, egt=300.0 + phase * 700.0,
                      rpm=1000.0 + phase * 6000.0, speed=phase * 100.0, map_kpa=30 + phase * 60)
    if mode == "egt-danger":
        phase = (math.sin(2 * math.pi * t / 40.0 - math.pi / 2) + 1.0) / 2.0
        return Sample("EGT", afr=0.80 * STOICH, egt=600.0 + phase * 360.0, rpm=4500.0,
                      speed=80.0, gear=3, map_kpa=95, inj_duty=40)
    return Sample()


def send_cycle(bus, f: Frames, s: Sample, extended: bool) -> int:
    msgs = [
        f.status(gear=s.gear),
        f.speeds(s.rpm, timing=s.timing, inj=s.inj_duty, vss=s.speed),
        f.sensors1(map_kpa=s.map_kpa, clt=s.clt, iat=s.iat),
        f.sensors2(oil_kpa=200.0 + s.rpm * 0.05, oil_temp=95.0, batt_v=13.9),
        f.fueling3(s.lam),
        f.egts(s.egt),
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
        default="drive",
        choices=["drive", "idle", "sweep", "egt-danger", "dropout", "burst", "invalid", "replay"],
    )
    ap.add_argument("--channel", default="PCAN_USBBUS1")
    ap.add_argument("--bitrate", type=int, default=500000)
    ap.add_argument("--base", default="0x200")
    ap.add_argument("--period-ms", type=int, default=50, help="送信周期。rusEFI の canSleepPeriodMs 相当")
    ap.add_argument("--extended", action="store_true", help="29bit 拡張 ID で送る")
    ap.add_argument("--lambda-tau-ms", type=float, default=DriveCycle.LAMBDA_TAU_MS,
                    help="--mode drive の λ 一次遅れ時定数 [ms]。0 でフィルタ無効")
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

        drive = DriveCycle(args.lambda_tau_ms)
        print(
            f"mode={args.mode} base={base:#05x} bitrate={args.bitrate} "
            f"period={period * 1000:.0f}ms ext={args.extended}\nCtrl+C で停止"
        )

        t0 = time.perf_counter()
        sent = 0
        next_at = t0
        last_report = t0
        last_sample = Sample()
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
                    bus.send(can.Message(arbitration_id=base + OFF_FUELING3,
                                         data=struct.pack("<HHHH", 0, 0, 0, 0),
                                         is_extended_id=args.extended))
                    bus.send(can.Message(arbitration_id=base + OFF_EGTS, data=bytes(8),
                                         is_extended_id=args.extended))
                    # DLC 不足フレーム（破棄され badDlc が増えること）
                    bus.send(can.Message(arbitration_id=base + OFF_SPEEDS, data=bytes(4),
                                         is_extended_id=args.extended))
                    sent += 3
                else:
                    if args.mode == "drive":
                        last_sample = drive.sample(t, period * 1000.0)
                    else:
                        last_sample = simple_sample(args.mode, t)
                    sent += send_cycle(bus, f, last_sample, args.extended)
            else:
                time.sleep(0.0005)

            if now - last_report >= 0.5:
                last_report = now
                s = last_sample
                cut = " FUEL-CUT" if s.fuel_cut else ""
                print(
                    f"  t={t % DriveCycle.PERIOD_S:5.1f}s {s.phase:<10s} G{s.gear} "
                    f"{s.rpm:5.0f}rpm {s.speed:3.0f}km/h  AFR={s.afr:5.2f} "
                    f"(lam {s.lam:.3f})  EGT={s.egt:4.0f}C{cut}"
                )

    except KeyboardInterrupt:
        print("\n停止しました")
    finally:
        bus.shutdown()


if __name__ == "__main__":
    main()
