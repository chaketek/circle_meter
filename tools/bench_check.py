"""PCAN ベンチで CAN パターンを流し、本体（LCD-2.1 / M5Dial）のシリアル出力を採点する。

DOC-30 の `IT-*` / `QT-02` をいつも同じ手順で実行するためのオーケストレータ。
送信（tools/pcan_send.py）と受信ログの採点を 1 コマンドにまとめてある。

    python tools/bench_check.py                       実走模擬を 45 秒
    python tools/bench_check.py --seconds 20
    python tools/bench_check.py --mode egt-danger     QT-05 の警告確認
    python tools/bench_check.py --mode dropout        IT-02 途絶検出
    python tools/bench_check.py --mode burst          IT-03 高負荷
    python tools/bench_check.py --listen              IT-04 本機が送信しないこと（観測開始後に本体をリセット）
    python tools/bench_check.py --flash               先に最新をビルドして書き込む

前提:
  - PCAN-USB を PC に接続し、CAN_H / CAN_L / GND を M5Stack CAN Unit へ配線
  - 本体を USB 接続（実 CAN 版のファームウェアが入っていること。LCD-2.1 は lcd21、M5Dial は m5dial）
    LCD-2.1 は CH343 の UART Type-C（VID 1A86）、M5Dial はネイティブ USB（VID 303A）で自動検出する
  - ベンチは 2 ノードのみ。終端は CAN Unit 側 120 ohm + PCAN 側 120 ohm（合計 60 ohm）

終了コード 0 = 全判定合格。
"""

import argparse
import os
import re
import subprocess
import sys
import tempfile
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

# PlatformIO の仮想環境に pyserial が入っている場合があるので後ろに足しておく
for extra in (
    os.path.expanduser(r"~\.platformio\penv\Lib\site-packages"),
    os.path.expanduser("~/.platformio/penv/lib/python3/site-packages"),
):
    if os.path.isdir(extra) and extra not in sys.path:
        sys.path.append(extra)

# Windows のコンソールは cp932 のことがあり、範囲外の文字で例外になる。
# 判定結果の表示が落ちるほうが困るので、表示できない文字は置き換える。
for _stream in (sys.stdout, sys.stderr):
    try:
        _stream.reconfigure(errors="replace")
    except Exception:
        pass

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    sys.exit("pyserial が必要です: python -m pip install --user pyserial")


# ---------------------------------------------------------------- 受信ログの解析
LINE_RE = re.compile(
    r"rx=(?P<rx>\d+)\s+f/s=(?P<fps>\d+)\s+unk=(?P<unk>\d+)\s+dlc=(?P<dlc>\d+)\s+"
    r"ovf=(?P<ovf>\d+)\s+tec=(?P<tec>\d+)\s+rec=(?P<rec>\d+)\s*\|\s*"
    r"lam=(?P<lamnone>\(none\))?(?P<lam>[\d.]+)\s+egt=(?P<egtnone>\(none\))?(?P<egt>[\d.]+)"
)
AGE_RE = re.compile(r"ageL=(?P<ageL>\d+)\s+ageE=(?P<ageE>\d+)\s+snapFail=(?P<snapFail>\d+)")
DRAW_RE = re.compile(r"draw=(?P<draw>\d+)us \(max (?P<fps>\d+) fps\)")

ZONES = [(0.75, "RICH_HEAVY (青)"), (0.85, "RICH (シアン)"), (1.03, "OPTIMAL (緑)"),
         (1.10, "LEAN (黄)"), (99.0, "LEAN_HEAVY (赤)")]


def zone_of(lam: float) -> str:
    for hi, name in ZONES:
        if lam <= hi:
            return name
    return "?"


def find_port(explicit: str = None) -> str:
    if explicit:
        return explicit
    # LCD-2.1 は CH343P（VID 0x1A86）、M5Dial は ESP32-S3 のネイティブ USB（VID 0x303A）
    for vid in (0x1A86, 0x303A):
        for p in list_ports.comports():
            if p.vid == vid:
                return p.device
    ports = [p.device for p in list_ports.comports()]
    sys.exit(f"本体が見つかりません（VID 1A86 / 303A）。検出されたポート: {ports or 'なし'}\n"
             "  --port COM10 のように明示してください。")


def open_without_reset(port: str) -> "serial.Serial":
    """DTR / RTS を下げたままポートを開く。

    pyserial は既定で開いた瞬間に DTR / RTS を有効にする。LCD-2.1 は CH343 の RTS が EN（リセット）に
    つながる自動書き込み回路なので、普通に開くと本体がリセットされ、起動直後の数秒（オープニング中の
    受信の溜まり・最初の全面描画）が採点に混ざっていた（2026-10-05）。
    """
    ser = serial.Serial()
    ser.port = port
    ser.baudrate = 115200
    ser.timeout = 0.5
    ser.dtr = False
    ser.rts = False
    ser.open()
    return ser


def reset_board(port: str) -> None:
    """CH343 の RTS で EN を引いて本体をリセットする（LCD-2.1 の自動書き込み回路を使う）。"""
    try:
        ser = serial.Serial(port, 115200, timeout=0.2)
        ser.setDTR(False)
        ser.setRTS(True)
        time.sleep(0.15)
        ser.setRTS(False)
        ser.close()
        print(f"   {port} をリセットしました（観測中に起動させる）")
    except Exception as e:  # noqa: BLE001
        print(f"   リセットできませんでした（{e}）。本体の電源を手で入れ直してください")


def run_flash(board: str, sim: bool, port: str) -> None:
    env = board + ("_sim" if sim else "")
    print(f"== ビルドして書き込み ({env}) ==")
    # pioarduino（lcd21）は Git Bash（MSYS）の環境変数があると idf_tools.py が失敗する（DOC-40 §2.3）
    child_env = {k: v for k, v in os.environ.items() if not k.startswith(("MSYSTEM", "MINGW"))}
    cmd = [sys.executable, "-m", "platformio", "run", "-e", env, "-t", "upload"]
    if port:
        cmd += ["--upload-port", port]
    r = subprocess.run(cmd, cwd=ROOT, env=child_env)
    if r.returncode != 0:
        sys.exit("書き込みに失敗しました")
    time.sleep(2.0)


# ---------------------------------------------------------------- 判定
class Check:
    def __init__(self):
        self.items = []

    def add(self, name: str, ok: bool, detail: str):
        self.items.append((name, ok, detail))

    @property
    def passed(self) -> bool:
        return all(ok for _, ok, _ in self.items)

    def report(self) -> None:
        print("\n| 判定項目 | 結果 | 実測 |")
        print("|---|---|---|")
        for name, ok, detail in self.items:
            print(f"| {name} | {'PASS' if ok else '**FAIL**'} | {detail} |")


def main() -> None:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--mode", default="drive",
                    choices=["drive", "idle", "sweep", "egt-danger", "dropout", "burst", "invalid",
                             "wbo-warmup", "wbo-fault", "ecu-warmup", "startup"])
    ap.add_argument("--seconds", type=float, default=45.0, help="観測時間（既定 45 秒 = 実走模擬 2 周期強）")
    ap.add_argument("--port", default=None, help="本体のシリアルポート（既定は自動検出）")
    ap.add_argument("--board", default="lcd21", choices=["lcd21", "m5dial"],
                    help="--flash で書き込む対象（既定 lcd21）")
    ap.add_argument("--no-reset", action="store_true", help="--listen のとき本体をリセットしない")
    ap.add_argument("--channel", default="PCAN_USBBUS1")
    ap.add_argument("--flash", action="store_true", help="先にビルドして書き込む")
    ap.add_argument("--sim", action="store_true", help="--flash のとき CAN シミュレータ版を書き込む")
    ap.add_argument("--listen", action="store_true", help="IT-04: 送信せずバスを観測するだけ")
    ap.add_argument("--expect-fps", type=float, default=160.0,
                    help="期待する受信フレームレート（ECU 6 + WBO 2 フレーム x 20 Hz）")
    args = ap.parse_args()

    if args.flash:
        run_flash(args.board, args.sim, args.port)

    # ---- IT-04: 本機が送信しないことの確認
    if args.listen:
        print(f"== IT-04: {args.seconds:.0f} 秒間バスを観測（本機の電源だけ入れた状態で実行すること） ==")
        listener = subprocess.Popen([sys.executable, os.path.join(HERE, "pcan_send.py"),
                                     "--channel", args.channel, "--listen", str(args.seconds)], cwd=ROOT)
        # OPN-13 / RSK-13: CAN を USB の D+/D- に割り当てているので、起動直後（ROM ブートローダ・USB PHY が
        # パッドを握っている間）にバスへ何か出ないかを見る。観測を始めてから本体をリセットする
        if not args.no_reset:
            time.sleep(2.0)
            reset_board(find_port(args.port))
        sys.exit(listener.wait())

    port = find_port(args.port)
    print(f"== ベンチ確認 mode={args.mode} port={port} {args.seconds:.0f}秒 ==")

    # 送信側の出力はファイルへ逃がす。
    # subprocess.PIPE にして読まずに放置すると、Windows のパイプバッファ（4KB）が
    # 20 秒ほどで埋まり、送信側が書き込みでブロックして CAN 送出が止まる。
    # そうなると「受信が途中で止まった」ように見えて、原因を本体側と誤認する。
    sender_log = os.path.join(tempfile.gettempdir(), "circle_meter_pcan_send.log")
    log_fp = open(sender_log, "w", encoding="utf-8", errors="replace")
    sender = subprocess.Popen(
        [sys.executable, "-u", os.path.join(HERE, "pcan_send.py"),
         "--mode", args.mode, "--channel", args.channel],
        cwd=ROOT, stdout=log_fp, stderr=subprocess.STDOUT)

    def sender_output() -> str:
        try:
            log_fp.flush()
        except Exception:
            pass
        try:
            with open(sender_log, encoding="utf-8", errors="replace") as fp:
                return fp.read()
        except Exception:
            return ""

    time.sleep(2.0)
    if sender.poll() is not None:
        print(sender_output())
        sys.exit("PCAN 送信が開始できませんでした")

    rows, none_rows, lines = [], 0, 0
    try:
        ser = None
        for _ in range(10):
            try:
                ser = open_without_reset(port)
                break
            except Exception:
                time.sleep(1.2)
        if ser is None:
            raise RuntimeError(f"{port} を開けません")
        ser.setDTR(False)
        ser.setRTS(False)

        t0 = time.time()
        while time.time() - t0 < args.seconds:
            raw = ser.readline()
            if not raw:
                continue
            text = raw.decode("utf-8", "replace").rstrip()
            if not text.startswith("rx="):
                continue
            lines += 1
            m = LINE_RE.search(text)
            if not m:
                continue
            d = {k: v for k, v in m.groupdict().items()}
            a = AGE_RE.search(text)
            w = DRAW_RE.search(text)
            row = {
                "fps": int(d["fps"]), "unk": int(d["unk"]), "dlc": int(d["dlc"]),
                "ovf": int(d["ovf"]), "tec": int(d["tec"]), "rec": int(d["rec"]),
                "lam": None if d["lamnone"] else float(d["lam"]),
                "egt": None if d["egtnone"] else float(d["egt"]),
                "ageL": int(a.group("ageL")) if a else None,
                "snapFail": int(a.group("snapFail")) if a else None,
                "draw": int(w.group("draw")) if w else None,
            }
            if row["lam"] is None or row["egt"] is None:
                none_rows += 1
                print(f"  !! {text}")
            rows.append(row)
        ser.close()
    finally:
        sender_died = sender.poll() is not None
        sender.terminate()
        try:
            sender.wait(timeout=3)
        except Exception:
            sender.kill()
        log_fp.close()

    # 送信側が観測中に落ちていたら、受信が途切れたのは本体のせいではない
    if sender_died:
        print("\n**PCAN 送信プロセスが観測中に終了しました。** 判定は本体の評価になりません。")
        print("--- 送信側の出力 ---")
        print(sender_output())
        sys.exit(2)

    if not rows:
        sys.exit("本体から 1 行も受信できませんでした。配線・ポート・ファームウェア（実 CAN 版か）を確認してください。")

    # ---- 採点（起動直後の 1 行目は計測窓が短いので除外する）
    body = rows[1:] if len(rows) > 1 else rows
    fps = [r["fps"] for r in body]
    lams = [r["lam"] for r in rows if r["lam"] is not None]
    egts = [r["egt"] for r in rows if r["egt"] is not None]
    ages = [r["ageL"] for r in rows if r["ageL"] is not None]
    snaps = [r["snapFail"] for r in rows if r["snapFail"] is not None]
    draws = [r["draw"] for r in rows if r["draw"] is not None]

    c = Check()
    if args.mode not in ("dropout", "invalid"):
        lo, hi = args.expect_fps * 0.9, args.expect_fps * 1.1
        if args.mode == "burst":
            lo, hi = 1300, 1900  # 8 フレーム x 200 Hz = 1600
        c.add("受信レート", all(lo <= f <= hi for f in fps), f"{min(fps)} - {max(fps)} f/s")
        c.add("信号喪失の誤検出 (RSK-01)", none_rows == 0, f"{none_rows} 行 / {len(rows)} 行")
    # 本体のカウンタは起動からの累積。本体をリセットせずに続けて試験すると前の試験の分が残るので、
    # この試験の間の増分で判定する（2026-10-05: 無効値試験の直後の途絶試験で誤判定した）
    def delta(key):
        return rows[-1][key] - rows[0][key]

    c.add("不明 ID / DLC 不正", (delta("unk") == 0 and delta("dlc") == 0) if args.mode != "invalid" else True,
          f"この試験の増分 unk={delta('unk')} dlc={delta('dlc')}")
    c.add("受信キュー溢れ (SYS-06)", delta("ovf") == 0, f"この試験の増分 ovf={delta('ovf')}")
    c.add("TWAI エラーカウンタ", all(r["tec"] == 0 and r["rec"] == 0 for r in rows),
          f"tec={max(r['tec'] for r in rows)} rec={max(r['rec'] for r in rows)}")
    if snaps:
        c.add("スナップショット取得失敗", max(snaps) == 0, f"snapFail={max(snaps)}")
    if args.mode == "dropout":
        # IT-02: 送信を止めている間に必ず Lost（"--"）になり、古い値を出さないこと。再開で復帰すること
        lost_ages = [r["ageL"] for r in rows if r["lam"] is None and r["ageL"] is not None]
        c.add("途絶で信号喪失になる (IT-02 / RSK-01)", none_rows > 0 and all(a >= 2000 for a in lost_ages),
              f"喪失 {none_rows} 行、喪失時の ageL 最小 {min(lost_ages) if lost_ages else '-'} ms（2000 以上で正）")
        c.add("再開で復帰する", any(r["lam"] is not None for r in rows[-8:]), "末尾 8 行に受信値あり")
    elif args.mode == "invalid":
        # UT-09 の実機確認: λ=0 / EGT=0 は採用しない（ずっと Lost）。DLC 不足は破棄して数える
        c.add("無効値を採用しない (SYS-42 / RSK-09)", none_rows == len(rows), f"喪失 {none_rows} 行 / {len(rows)} 行")
        c.add("DLC 不足を破棄して数える (SWR-06)", delta("dlc") > 0, f"この試験の増分 dlc={delta('dlc')}")
    elif ages:
        c.add("信号の鮮度", max(ages) < 500, f"ageL 最大 {max(ages)} ms")
    if draws:
        worst_fps = 1000000 // max(draws)
        c.add("描画レート (SYS-12: 30 fps 以上)", worst_fps >= 30,
              f"最悪 {max(draws)} us = {worst_fps} fps")

    print(f"\n=== 計測サマリ（{args.mode} / {args.seconds:.0f} 秒 / {len(rows)} ログ行）===")
    if lams:
        print(f"  λ      : {min(lams):.3f} - {max(lams):.3f}  "
              f"(AFR {min(lams) * 14.7:.2f} - {max(lams) * 14.7:.2f})")
    if egts:
        print(f"  EGT    : {min(egts):.0f} - {max(egts):.0f} degC")

    if lams:
        seen = {}
        for l in lams:
            seen[zone_of(l)] = seen.get(zone_of(l), 0) + 1
        print("\n  通過した λ ゾーン（1 秒ごとのログでの出現回数。画面上はもっと細かく動く）")
        for _, name in ZONES:
            n = seen.get(name, 0)
            print(f"    {name:<18} {n:3d}")

    c.report()

    # 受信が途中で止まっていないか（送信側の停止と本体の不具合を取り違えないため）
    stalled = [i for i in range(1, len(rows)) if rows[i]["fps"] == 0]
    if stalled:
        print(f"\n**受信が観測中に {len(stalled)} 回 f/s=0 になりました。**")
        print("  ageL が単調に増えていれば、止まったのは送信側でバス上にフレームが無い状態です。")
        print(f"  送信側の出力: {sender_log}")

    if args.mode == "drive":
        print("""
=== 目視で確認すること（QT-02 / QT-03）===
  [ ] 外周バーが青以外の 4 ゾーンを通り、色が滑らかに変化する
  [ ] 加速の踏み始めに一瞬だけ黄（LEAN）を横切る
  [ ] 減速中はバーが赤（LEAN_HEAVY）に張り付く
  [ ] 燃料カット復帰でシアン側へ振れる（約 0.3 秒）
  [ ] 排気温度が加速で上がり、減速で遅れて下がる
  [ ] 数値がちらつかない / 桁あふれしない
  [ ] 回転数・車速・ギアが破綻なく動く（副ページ）

  ※ 青ゾーン (λ<=0.75 = AFR<=11.0) は失火域のため実走模擬では通らない。
     確認するときは --mode sweep を使う。
  ※ EGT の警告 (850) / 危険 (920) も実走模擬では入らない。
     確認するときは --mode egt-danger を使う。""")

    print("\n" + ("すべての自動判定に合格しました。" if c.passed else "**不合格の項目があります。**"))
    sys.exit(0 if c.passed else 1)


if __name__ == "__main__":
    main()
