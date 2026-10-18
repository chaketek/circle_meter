# circle_meter

rusEFI と CAN で接続し、**空燃比（λ / AFR）と排気温度**を表示する丸形メータ。
ユーノスロードスター (NA) への搭載を想定。

ハードウェアは **Waveshare ESP32-S3-Touch-LCD-2.1 / 2.1B**（ESP32-S3R8 / 2.1" 丸型 **480×480** / 静電容量タッチ）または **M5Stack Dial v1.1**（1.28" 丸型 240×240。機能は一部）+ **M5Stack Unit Mini CAN**（トランシーバと 12V→5V 変換を兼ねる）。

## できること（v0.1.0）

| | LCD-2.1 | M5Dial |
|---|---|---|
| 主表示 | λ / AFR の大きな数値と、**指針式**（既定）または**外周リング**。λ に応じて 5 色に変わる | 外周リングと数値（リングのみ） |
| 排気温度 | 数値。警告で黄、危険で赤（外周も点滅） | 数値。警告で黄、危険で赤 |
| λ センサの状態 | `SENSOR OFF` / `WARMING UP 512°C` / `SENSOR CHECK` / 故障 / `NO WBO DATA` / `NO SIGNAL` | 同じ（温度の単位は `C`） |
| 本体での操作 | なし | ボタンで AFR / λ 切替 |
| 表示の変更 | PC から設定ファイル（`user/user_config.h`）を書き換えて書き込む: 色・警告温度・AFR / λ・デザイン・明るさ・CAN の通信速度・起動ロゴ | 同じ（デザインはリングのみ） |
| 描画 | 約 40 fps | 約 55 fps |
| CAN | **受信専用**。λ は rusEFI WBO（`0x190`）、排気温度は ECU の verbose broadcast（`0x209`）から。フレームは一切送出しない（ACK のみ返す） | 同じ |

## まだできないこと（今後の予定）

- **タッチ操作**（スワイプでのページ切替、長押しの設定メニュー）。今は設定を PC から変える
- **λ・排気温度以外のページ**（回転数・水温・吸気温・電圧など）。CAN のデコードは済んでいて、表示が未実装
- **設定を本体に保存すること**（NVS）。設定は書き込んだときに決まる
- ブザーでの警告

> ⚠️ 本機は表示専用であり、エンジンの制御には一切介入しません。
> 追加メータとしての使用を前提としており、純正メータの代替ではありません。

## このメータを使う方へ

**[利用者ガイド（docs/user/）](docs/user/README.md) から始めてください。**
Git や Claude Code を使ったことがなくても、Windows の PC で書き込み、色・警告温度・表示の種類・起動ロゴを
好みに変えられるように書いています。Claude Code に「環境を作って」「針の色を青にして」のように頼むだけで進められます。

あなたの設定は `user/` フォルダに置きます（[user/README.md](user/README.md)）。作者の更新を取り込んでも上書きされません。

---

以下は開発者向けの情報です。

> 2026-10-01 に対象ハードを M5Stack Dial v1.1 から変更しました。経緯は [DOC-12 §8](docs/12_SYS3_system_arch.md)。
> LCD-2.1 向けの本番コードは `src/main_lcd21.cpp` / `lib/ui/` です。**M5Stack Dial v1.1 も配布対象**です（2026-10-12〜。`src/main.cpp`、`m5dial` / `m5dial_sim` 環境。WBO の λ・センサ状態・ユーザー設定・ロゴに対応し、表示はリングのみ。[DOC-12 §8.5](docs/12_SYS3_system_arch.md)）。

---

## 開発状況

| フェーズ | 内容 | 状態 |
|---|---|---|
| P0 | ASPICE テーラリング文書一式 | ✅ 完了 |
| P1 | リポジトリ骨格・CI・ドメイン層・単体テスト | ✅ 完了 |
| P2 | ブリングアップ（M5Dial で CAN 受信を実機確認） | ✅ 完了 |
| P2.5 | **ボード移行**（Waveshare ESP32-S3-Touch-LCD-2.1。文書改訂 → プラットフォーム選定 → 描画性能の実測） | ⏳ 実施中 |
| P3 | ドメイン実装の完成 | — |
| P4 | HMI 実装（LVGL + λ リング / 指針式） | ⏳ 着手 |
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

## クイックスタート（開発者）

### 必要なもの

- Windows 11、Python 3.11 以降、Git
- PlatformIO Core と部品: `python -m pip install --user --upgrade platformio pyserial pillow numpy`
- native 単体テストを PC で回す場合は C++ コンパイラ（Windows なら `scoop install gcc`）

詳しくは [DOC-40 §2](docs/40_SUP8_cm_dev_environment.md)。

### ビルドとテスト

LCD-2.1 版は LVGL 9 + pioarduino で、**PowerShell から**ビルドします
（Git Bash では pioarduino の導入に失敗します。[DOC-40 §2.3](docs/40_SUP8_cm_dev_environment.md)）。

```powershell
python -m platformio test -e native
```

```powershell
./tools/build.ps1
```

### 実機へ書き込み

書き込みと確認は CH343P の UART Type-C から行います（ネイティブ USB-C は使いません）。ポートは自動で探します。

| 環境 | 内容 | コマンド |
|---|---|---|
| `lcd21_sim` | CAN を繋がずに画面だけを動かすスイープデモ（λ 0.68 ⇄ 1.36 を 8.5 秒、EGT 300 ⇄ 960 °C） | `./tools/flash.ps1 -Sim` |
| `lcd21` | 実 CAN 版（500 kbps。rusEFI の既定） | `./tools/flash.ps1` |
| `lcd21_1m` | 実 CAN 版（1 Mbps。作者の車両用） | `./tools/flash.ps1 -Can1M` |
| `m5dial_sim` | M5Dial のスイープデモ | `./tools/flash.ps1 -Board m5dial -Sim` |
| `m5dial` | M5Dial の実 CAN 版（通信速度は `user/user_config.h` の `USER_CAN_BITRATE_KBPS`） | `./tools/flash.ps1 -Board m5dial` |

書き込み後は `python tools/meter_log.py` で起動ログを読み、書き込み・ユーザー設定・CAN 受信を判定できます。
デモはシリアル（115200）から 1 文字送ると操作できます: `h` 停止/再開、`0`〜`9` その位置で停止、`m` AFR/λ 切替、`v` 表示デザイン切替。

### 配布先ユーザー向けの仕組み

| 置き場所 | 内容 |
|---|---|
| [docs/user/](docs/user/README.md) | 利用者ガイド（準備 → 導入 → 書き込み → 表示の変更 → 更新） |
| [user/](user/README.md) | 利用者の設定 `user_config.h` とロゴ。git の管理対象外（雛形 `user_config.example.h` だけ管理） |
| `.claude/skills/` | `setup-meter` / `flash-meter` / `customize-meter` / `update-meter`（利用者向け）、`bench-check`（開発者向け） |

仕様は `SWR-67` / `SWD-14`、運用は [DOC-40 §2.6](docs/40_SUP8_cm_dev_environment.md)。

## 配線

車両 12V と CAN_H / CAN_L を Mini CAN の端子台へ入れ、Grove ケーブルを本体の **12PIN** へ繋ぎます。

| Grove 線色 | CAN Unit | 本体（12PIN） | **本体側ケーブルの線色** | 備考 |
|---|---|---|---|---|
| 黒 | GND | GND | **黒** | |
| 赤 | **5V 出力**（端子台入力から生成、最大 700 mA） | **VBus** | **赤**（VCC 5V） | 車両の 12V から Mini CAN 経由で給電して動作を確認（2026-10-18。`OPN-14`）。消費電流は未測定 |
| 黄 | CAN_TX | **D+ (GPIO20)** | **黄**（CAN TX） | `CM_TWAI_TX_GPIO=20`。ベンチで通信を確認（`OPN-13`） |
| 白 | CAN_RX | **D− (GPIO19)** | **緑**（CAN RX） | `CM_TWAI_RX_GPIO=19`。同上 |

> 本体側ケーブルの線色は、**現在使用中の LCD-2.1 の個体に付属のケーブル**で確認したもの（2026-10-05）。
> Waveshare の資料に線色の記載は無く、別の個体・2.1B では違う可能性があるので、色ではなく 12PIN の信号名で確かめること。
> RX は Grove 側が白、本体側が緑で、両端の色が違う。

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

PCAN-USB から rusEFI のフレームを模擬送出し、本体の表示を確認できます。
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
- 本体に **実 CAN 版**（`lcd21`）のファームウェアが入っていること
  （`lcd21_sim` は CAN を一切読みません）
- ベンチは 2 ノードのみなので、終端は CAN Unit 側 120 Ω + PCAN 側 120 Ω（合計 60 Ω）

## ECU 側の前提設定

TunerStudio で以下を確認してください（[DOC-13 §5](docs/13_ICD_rusefi_can.md)）。

- `enableVerboseCanTx` = true / `canWriteEnabled` = true
- `verboseCanBaseAddress` = 0x200（既定）
- `canBaudRate` = 500 kbps（違う場合は `user/user_config.h` の `USER_CAN_BITRATE_KBPS` か `lcd21_1m`）、`rusefiVerbose29b` = false
- `Lambda1` と `EGT1` のセンサが構成済みで、ゲージに値が出ていること

## ライセンス

[MIT ライセンス](LICENSE)（Copyright (c) 2026 chaketek）。コード・ツール・文書・起動ロゴが対象です。

同梱のフォント（B612 Mono・Montserrat。SIL Open Font License 1.1）と、ビルドのときに取得するライブラリ（LVGL・Arduino-ESP32〔LGPL 2.1〕・ESP32_Display_Panel〔Apache 2.0〕など）は、それぞれのライセンスに従います。一覧とファームウェアを配布するときの注意は [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md) を見てください。
