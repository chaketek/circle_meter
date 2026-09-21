---
name: bench-check
description: PCAN ベンチで CAN パターンを M5Dial に流し、表示を目視確認する。実走模擬（アイドル→加速→定常→燃料カット→アイドル）を流して外周バーの色変化や数値の動きを見たいとき、また DOC-30 の結合テスト IT-01/02/03/04/15 や適格性確認 QT-02/QT-03/QT-05 を実行したいときに使う。「ベンチで流して」「実走模擬を走らせて」「目視チェックしたい」「IT-03 を実行して」などで起動する。
---

# ベンチ目視チェック

PCAN から rusEFI の CAN フレームを模擬送出し、M5Dial のシリアル出力を採点したうえで、
画面の目視確認項目を提示する。仕様は `docs/30_test_strategy.md` §3。

## 前提の確認

実行前にユーザーに確認する（すでに繋がっていることが会話から明らかなら省略してよい）。

- PCAN-USB が PC に接続され、CAN_H / CAN_L / GND が M5Stack CAN Unit に配線されている
- M5Dial が USB 接続されている
- **実 CAN 版**のファームウェア（`m5dial`）が書き込まれている
  - `m5dial_sim` はシミュレータ版で CAN を一切読まない。入っていたら `--flash` を付ける
- ベンチは 2 ノードのみなので、終端は CAN Unit 側 120Ω + PCAN 側 120Ω（合計 60Ω）

## 実行

```bash
python tools/bench_check.py --seconds 45
```

ポートは VID 303A から自動検出する。うまくいかなければ `--port COM7` を足す。

| やりたいこと | コマンド |
|---|---|
| 実走模擬を目視確認（既定） | `python tools/bench_check.py` |
| 最新をビルド・書き込んでから | `python tools/bench_check.py --flash` |
| 短く済ませる | `python tools/bench_check.py --seconds 20` |
| 青ゾーンも見たい | `python tools/bench_check.py --mode sweep` |
| EGT 警告の確認 (`QT-05`) | `python tools/bench_check.py --mode egt-danger --seconds 40` |
| 途絶検出 (`IT-02`) | `python tools/bench_check.py --mode dropout --seconds 25` |
| 高負荷 (`IT-03`) | `python tools/bench_check.py --mode burst --seconds 20` |
| 無効値の扱い | `python tools/bench_check.py --mode invalid --seconds 15` |
| **送信しないこと (`IT-04`)** | `python tools/bench_check.py --listen --seconds 30` |

`--listen` は**本機の電源だけを入れた状態**で実行すること（送信は行わない）。

## 結果の扱い

スクリプトは自動判定の表と目視チェックリストを出力する。終了コード 0 が全項目合格。

1. **自動判定の表をそのままユーザーに見せる。** 数値を言い換えたり丸めたりしない。
2. 不合格があれば、どの判定がなぜ落ちたかを `docs/30_test_strategy.md` の該当 `IT-*` と
   結びつけて説明する。推測で原因を断定せず、必要なら計測を足して切り分ける。
3. **目視チェックリストはユーザーにしか判定できない。** 画面がどう見えたかを尋ねる。
   こちらから「問題なく見えているはずです」と書かない。
4. リリース前の実行なら、結果を `docs/test_records/<日付>_<用途>.md` に記録する
   （書式は `docs/test_records/README.md`）。

## 判定内容

| 判定項目 | 基準 | 根拠 |
|---|---|---|
| 受信レート | 120 f/s ±10 %（burst は 900–1400） | 6 フレーム × 20 Hz |
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
