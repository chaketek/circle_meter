---
name: bench-check
description: PCAN ベンチで CAN パターンを本体（Waveshare LCD-2.1。旧 M5Dial も可）に流し、シリアルを採点して表示を UVC カメラで確認する。実走模擬（アイドル→加速→定常→燃料カット→アイドル）を流して外周バーの色変化や数値の動きを見たいとき、また DOC-30 の結合テスト IT-01/02/03/04/15 や適格性確認 QT-02/QT-03/QT-05 を実行したいときに使う。「ベンチで流して」「実走模擬を走らせて」「目視チェックしたい」「IT-03 を実行して」などで起動する。
---

# ベンチ目視チェック

PCAN から rusEFI の CAN フレームを模擬送出し、本体のシリアル出力を採点したうえで、
画面を UVC カメラで撮影して確認する。仕様は `docs/30_test_strategy.md` §3。

## 前提の確認

実行前にユーザーに確認する（すでに繋がっていることが会話から明らかなら省略してよい）。

- PCAN-USB が PC に接続され、CAN_H / CAN_L / GND が CAN Unit（Mini CAN）に配線されている
- CAN Unit の Grove が本体の 12PIN に配線されている: GND-GND、5V-VBus、TX-D+ (GPIO20)、RX-D- (GPIO19)
  （本体側ケーブルの線色。現在の個体限定: 黒 GND / 赤 5V / 黄 CAN TX / 緑 CAN RX。Grove 側の RX は白）
  （`README.md` の配線表。**ベンチでは CAN Unit の端子台に 12V を入れない**: 本体の USB 給電と 5V がぶつかる）
- 本体は **CH343 の UART Type-C** で PC に接続（`COM10`、VID 1A86）。**ネイティブ USB-C は挿さない**（`RSK-13`）
- **実 CAN 版**のファームウェア（`lcd21`）が書き込まれている
  - `lcd21_sim` はスイープデモで CAN を一切読まない。入っていたら PowerShell から書き込む:
    `python -m platformio run -e lcd21 -t upload --upload-port COM10`
- ベンチは 2 ノードのみなので、終端は CAN Unit 側 120Ω + PCAN 側 120Ω（合計 60Ω）

## 実行

```bash
python tools/bench_check.py --seconds 45
```

ポートは VID 1A86（LCD-2.1 の CH343）、なければ 303A（M5Dial）から自動検出する。うまくいかなければ `--port COM10` を足す。

| やりたいこと | コマンド |
|---|---|
| 実走模擬を目視確認（既定） | `python tools/bench_check.py` |
| 最新をビルド・書き込んでから | `python tools/bench_check.py --flash`（既定は `lcd21`。M5Dial は `--board m5dial`） |
| 短く済ませる | `python tools/bench_check.py --seconds 20` |
| 青ゾーンも見たい | `python tools/bench_check.py --mode sweep` |
| EGT 警告の確認 (`QT-05`) | `python tools/bench_check.py --mode egt-danger --seconds 40` |
| 途絶検出 (`IT-02`) | `python tools/bench_check.py --mode dropout --seconds 25` |
| 高負荷 (`IT-03`) | `python tools/bench_check.py --mode burst --seconds 20` |
| 無効値の扱い | `python tools/bench_check.py --mode invalid --seconds 15` |
| λ センサのウォームアップ表示 (`IT-17`) | `python tools/bench_check.py --mode wbo-warmup --seconds 32`（故障は `wbo-fault`、WBO なしは `ecu-warmup`）。シリアルの `sensor=` が OFF -> WARMUP -> CHECK -> OK と推移すること |
| キーオンからの流れを見せる（デモ） | `python tools/pcan_send.py --mode startup`（停止 -> 加熱 -> 確認の 18 秒のあと、実走模擬を繰り返す） |
| **送信しないこと (`IT-04`)** | `python tools/bench_check.py --listen --seconds 30` |

`--listen` は観測を始めてから**本体を RTS でリセット**し、起動直後（ROM ブートローダ・USB PHY が
D+/D- を握っている間）も含めてバスに何も出ないことを見る（`OPN-13` / `RSK-13`）。リセットしたくなければ `--no-reset`。
電源投入そのものも確認したいときは、観測中に本体の USB ケーブルを抜き差しする。

## 結果の扱い

スクリプトは自動判定の表と目視チェックリストを出力する。終了コード 0 が全項目合格。

1. **自動判定の表をそのままユーザーに見せる。** 数値を言い換えたり丸めたりしない。
2. 不合格があれば、どの判定がなぜ落ちたかを `docs/30_test_strategy.md` の該当 `IT-*` と
   結びつけて説明する。推測で原因を断定せず、必要なら計測を足して切り分ける。
3. **画面は UVC カメラで撮って自分で見る**（`python tools/lcd_capture.py --out <scratchpad>/x.jpg`、CLAUDE.md）。
   カメラは画面を 180° 回転して写し、白を水色に寄せる。動いている数字は露光で重なって写るので、
   形の確認はデモを止めた状態で行う。カメラで判断できないもの（実物の色味・眩しさ）だけをユーザーに尋ね、
   「問題なく見えているはずです」とは書かない。
4. リリース前の実行なら、結果を `docs/test_records/<日付>_<用途>.md` に記録する
   （書式は `docs/test_records/README.md`）。

## 判定内容

| 判定項目 | 基準 | 根拠 |
|---|---|---|
| 受信レート | 160 f/s ±10 %（burst は 1300–1900） | ECU 6 + WBO 2 フレーム × 20 Hz（λ は WBO から取る: `SYS-03`） |
| 信号喪失の誤検出 | `(none)` が 0 行 | `RSK-01`（最上位ハザード） |
| 不明 ID / DLC 不正 | 0 | `SWR-04` / `SWR-06` |
| 受信キュー溢れ | `ovf=0` | `SYS-06` |
| TWAI エラーカウンタ | `tec=0` かつ `rec=0` | バス品質 |
| スナップショット取得失敗 | `snapFail=0` | `DOC-21 §5.1` |
| 信号の鮮度 | `ageL < 500 ms` | `SYS-40` |
| 描画レート | 最悪値で 30 fps 以上 | `SYS-12` |

## 注意

- **青ゾーン（`RICH_HEAVY`, λ≤0.75 = AFR≤11.0）は実走模擬では通らない。**
  失火域であり、暖機後の健全なエンジンでは到達しないため。確認には `--mode sweep` を使う。
- **EGT の警告 850 ℃ / 危険 920 ℃ も実走模擬では入らない。** 確認には `--mode egt-danger` を使う。
- 実行後はバックグラウンドの送信プロセスが残らないことを確認する
  （スクリプトは終了時に停止するが、中断した場合は残ることがある）。
