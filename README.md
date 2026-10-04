# circle_meter

rusEFI と CAN で接続し、**空燃比（λ / AFR）と排気温度**を表示する丸形メータ。
ユーノスロードスター (NA) への搭載を想定。

ハードウェアは **Waveshare ESP32-S3-Touch-LCD-2.1 / 2.1B**（ESP32-S3R8 / 2.1" 丸型 **480×480** / 静電容量タッチ）+ **M5Stack Unit Mini CAN**（トランシーバと 12V→5V 変換を兼ねる）。

> 2026-10-01 に対象ハードを M5Stack Dial v1.1 から変更しました。経緯は [DOC-12 §8](docs/12_SYS3_system_arch.md)。
> **コードはまだ M5Dial 向けのままです**（文書を先行改訂する運用のため。[DOC-40 §5](docs/40_SUP8_cm_dev_environment.md)）。
> 下の `m5dial` 系のコマンドは現行コード（M5Dial）のものです。
> **コードはまだ M5Dial 向けのままです**（文書を先行改訂する運用のため。[DOC-40 §5](docs/40_SUP8_cm_dev_environment.md)）。

| | |
|---|---|
| 主表示 | 外周リングバーグラフ（λ 値に応じて 5 色に変化）+ 内周の大きな数値 |
| 副表示 | 排気温度、回転数・水温・吸気温・MAP、バッテリ電圧・油圧・油温、診断 |
| 操作 | **タッチのみ**。左右スワイプでページ切替、タップで AFR/λ 切替、長押しで設定。走行中の操作は想定しない |
| 更新レート | 30 fps 以上（目標）、CAN 受信から画面反映まで 120 ms 以内 |
| CAN | **受信専用**。フレームは一切送出しない（ACK のみ返す）。詳細は下の「配線」節 |

> ⚠️ 本機は表示専用であり、エンジンの制御には一切介入しません。
> 追加メータとしての使用を前提としており、純正メータの代替ではありません。

---

## 開発状況

| フェーズ | 内容 | 状態 |
|---|---|---|
| P0 | ASPICE テーラリング文書一式 | ✅ 完了 |
| P1 | リポジトリ骨格・CI・ドメイン層・単体テスト | ✅ 完了 |
| P2 | ブリングアップ（M5Dial で CAN 受信を実機確認） | ✅ 完了 |
| P2.5 | **ボード移行**（Waveshare ESP32-S3-Touch-LCD-2.1。文書改訂 → プラットフォーム選定 → 描画性能の実測） | ⏳ 実施中 |
| P3 | ドメイン実装の完成 | — |
| P4 | HMI 実装（LVGL + λ リング） | — |
| P5 | 設定メニュー・診断ページ | — |
| P6 | 実車検証 | — |

詳細は [docs/40_SUP8_cm_dev_environment.md](docs/40_SUP8_cm_dev_environment.md) の「フェーズ計画」を参照。

## ドキュメント

本プロジェクトは Automotive SPICE をテーラリングした文書体系で管理しています。
まず [docs/README.md](docs/README.md) を読んでください。

| 文書 | 内容 |
|---|---|
| [DOC-00](docs/00_ASPICE_tailoring.md) | ASPICE テーラリング定義（どのプロセスを適用し、何を除外したか） |
| [DOC-10](docs/10_SYS1_stakeholder_req.md) | ステークホルダ要求 |
| [DOC-11](docs/11_SYS2_system_req.md) | システム要求 + リスク・ハザード分析 |
| [DOC-12](docs/12_SYS3_system_arch.md) | システムアーキテクチャ（HW/SW 境界、GPIO 割当） |
| [DOC-13](docs/13_ICD_rusefi_can.md) | **rusEFI CAN インタフェース仕様（バイト単位の定義）** |
| [DOC-20](docs/20_SWE1_software_req.md) | ソフトウェア要求 |
| [DOC-21](docs/21_SWE2_software_arch.md) | ソフトウェアアーキテクチャ（描画ライブラリ選定の根拠を含む） |
| [DOC-22](docs/22_SWE3_detailed_design.md) | 詳細設計（API 契約・状態遷移） |
| [DOC-23](docs/23_HMI_design.md) | **HMI 設計（画面レイアウト・配色・ページ構成）** |
| [DOC-30](docs/30_test_strategy.md) | テスト戦略・テスト仕様 |
| [DOC-40](docs/40_SUP8_cm_dev_environment.md) | **構成管理・開発環境・CI/CD** |
| [DOC-41](docs/41_SUP1_qa_plan.md) | 品質保証計画・コーディング規約 |
| [DOC-50](docs/50_traceability.md) | トレーサビリティマトリクス |

## クイックスタート

### 必要なもの

- Python 3.11 以降
- PlatformIO Core: `python -m pip install --user --upgrade platformio`
- native 単体テストを PC で回す場合は C++ コンパイラ（Windows なら `scoop install gcc`）

### ビルドとテスト

```bash
pio test -e native
```

```bash
pio run -e m5dial
```

### 実機へ書き込み

```bash
pio run -e m5dial -t upload
```

CAN を繋がずに UI を確認したい場合はシミュレータ版を書き込みます。

```bash
pio run -e m5dial_sim -t upload
```

PowerShell からは `tools/` のスクリプトが使えます。

```bash
./tools/flash_and_monitor.ps1
```

### LCD-2.1 版（開発中）

対象ボード（Waveshare ESP32-S3-Touch-LCD-2.1）向けの実装は LVGL 9 + pioarduino で、**PowerShell から**ビルドします
（Git Bash では pioarduino の導入に失敗します。詳細は [DOC-40 §2.3](docs/40_SUP8_cm_dev_environment.md)）。
書き込みと確認は CH343P の UART Type-C（ネイティブ USB-C は使いません）から行います。

CAN を繋がずに画面だけを確認するスイープデモ（λ が 8.5 秒で 0.68 ⇄ 1.36 を往復し、EGT は 300 ⇄ 960 °C）:

```powershell
python -m platformio run -e lcd21_sim -t upload --upload-port COM10
```

シリアル（115200）から 1 文字送ると操作できます: `h` 停止/再開、`0`〜`9` その位置で停止、`m` AFR/λ 切替、`v` 表示デザイン切替（指針式 A ⇄ リング）。
1 秒ごとに fps と 1 フレームの描画時間も出ます（`DOC-30` の `QT-03`）。

実 CAN 版は `-e lcd21` です（物理層を繋いでから。CAN の GPIO は実機未確認: `OPN-13`）。
M5Dial 向けの `m5dial` / `m5dial_sim` は廃止予定です。

## 配線

車両 12V と CAN_H / CAN_L を Mini CAN の端子台へ入れ、Grove ケーブルを本体の **12PIN** へ繋ぎます。

| Grove 線色 | CAN Unit | 本体（12PIN） | 備考 |
|---|---|---|---|
| 黒 | GND | GND | |
| 赤 | **5V 出力**（端子台入力から生成、最大 700 mA） | **VBus** | 給電が成立するかは未検証（`OPN-14` / `RSK-12`） |
| 黄 | CAN_TX | **D+ (GPIO20)** | `CM_TWAI_TX_GPIO=20`。**暫定・実機未検証**（`OPN-13`） |
| 白 | CAN_RX | **D− (GPIO19)** | `CM_TWAI_RX_GPIO=19`。同上 |

> **接続前に必ず確認**: 車両 CAN バスの CAN_H–CAN_L 間抵抗が約 60 Ω であること。
> CAN Unit 側にも終端抵抗があると約 40 Ω になり、バス全体の通信品質が劣化します
> （`RSK-02` / `IT-10`）。
>
> **ネイティブ USB-C にケーブルを挿さないでください。** 12PIN の D−/D+ は同じ配線で、
> 挿すと CAN のトランシーバと競合します。書き込み・ログ取得は **UART Type-C**（CH343P）を使います。
>
> **UART0 (GPIO43/44) は CAN に使いません。** ROM ブートローダがリセットのたびに
> U0TXD (GPIO43) へログを出すため、CAN バスにノイズを撒いてしまいます。
> また UART Type-C を挿している間は 12PIN 側が切断されます（`DOC-12 §3.1`）。
>
> **ACK について**: 本機は CAN の ACK を返します（`RSK-10`）。ACK を返さない
> Listen Only 構成では、バス上が ECU と本機の 2 ノードだけのとき ECU が再送を
> 繰り返してバスオフに陥ります。フレームの送出は一切行いません。

## ベンチで動作を確認する

PCAN-USB から rusEFI のフレームを模擬送出し、M5Dial の表示を確認できます。
実車がなくても表示の妥当性を判断できるので、UI を触ったら毎回これを回してください。

### Claude Code から

```
/bench-check
```

「ベンチで流して」「実走模擬を走らせて」「目視チェックしたい」などでも起動します。
前提の確認から実行・採点・目視チェックリストの提示までを一通り行います。
定義は [.claude/skills/bench-check/SKILL.md](.claude/skills/bench-check/SKILL.md)。

### コマンドで直接

```bash
python tools/bench_check.py
```

PowerShell なら `./tools/bench_check.ps1`。

| やりたいこと | コマンド |
|---|---|
| 実走模擬を目視確認（既定・45 秒） | `python tools/bench_check.py` |
| 最新をビルド・書き込んでから | `python tools/bench_check.py --flash` |
| 短く済ませる | `python tools/bench_check.py --seconds 20` |
| 青ゾーン（過濃域）も見る | `python tools/bench_check.py --mode sweep` |
| EGT 警告の確認 (`QT-05`) | `python tools/bench_check.py --mode egt-danger --seconds 40` |
| 途絶検出 (`IT-02`) | `python tools/bench_check.py --mode dropout --seconds 25` |
| 高負荷 (`IT-03`) | `python tools/bench_check.py --mode burst --seconds 20` |
| **送信しないこと** (`IT-04`) | `python tools/bench_check.py --listen --seconds 30` |

受信レート・信号喪失の誤検出・キュー溢れ・TWAI エラー・鮮度・描画レートを自動判定し、
合格なら終了コード 0 を返します。あわせて画面の目視チェックリストを表示します。

**実走模擬パターン**（20 秒周期）はアイドル → 1-2-3 速で 60 km/h まで加速 → 定常走行 →
減速（燃料カット）→ アイドル を繰り返します。踏み始めのリーンスパイク、全開時のリッチ、
燃料カット時の張り付き、復帰時のリッチまで再現しており、λ は 0.836 – 1.361 を通ります。
詳細は [DOC-30 §3.4](docs/30_test_strategy.md)。

> 送信側の λ には一次遅れ（τ = 300 ms）を掛けています。実際のワイドバンドセンサにも
> 応答遅れがあり、階段状の λ がバスに出ることはないためです。

### 前提

- PCAN-USB を PC に接続し、CAN_H / CAN_L / GND を CAN Unit へ配線
- M5Dial に **実 CAN 版**（`m5dial`）のファームウェアが入っていること
  （`m5dial_sim` は CAN を一切読みません）
- ベンチは 2 ノードのみなので、終端は CAN Unit 側 120 Ω + PCAN 側 120 Ω（合計 60 Ω）

## ECU 側の前提設定

TunerStudio で以下を確認してください（[DOC-13 §5](docs/13_ICD_rusefi_can.md)）。

- `enableVerboseCanTx` = true / `canWriteEnabled` = true
- `verboseCanBaseAddress` = 0x200（既定）
- `canBaudRate` = 500 kbps、`rusefiVerbose29b` = false
- `Lambda1` と `EGT1` のセンサが構成済みで、ゲージに値が出ていること

## ライセンス

未定（個人利用）。
