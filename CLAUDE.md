# circle_meter — Claude Code 向けプロジェクト指示

rusEFI と CAN 接続する車載メータ（**Waveshare ESP32-S3-Touch-LCD-2.1 / 2.1B** / ESP32-S3R8 / 480×480 丸型 RGB 並列）のファームウェア。

> **2026-10-01: 対象ハードを M5Stack Dial v1.1 から変更した。** 文書は改訂済み。**コードは移行中**:
> LCD-2.1 向けの本番コード（`src/main_lcd21.cpp`、`lib/ui/`、`lib/hal/display_hal*`）を実装済みで、
> M5Dial 向けの `src/main.cpp` は廃止予定のまま残してある（触らない）。
> 移行の経緯と影響は `docs/12_SYS3_system_arch.md` §8 を読むこと。
ASPICE をテーラリングした文書体系で管理している。**文書が仕様であり、コードは文書に従う。**

## 最初に読むもの

- `docs/README.md` — 文書体系の入口
- `docs/13_ICD_rusefi_can.md` — CAN のバイト配置。**CAN 関連を触る前に必ず読む**
- `docs/23_HMI_design.md` — 画面の配色・レイアウト。**UI を触る前に必ず読む**
- `docs/40_SUP8_cm_dev_environment.md` §7 — 今どのフェーズか

## 絶対に守ること

1. **CAN バスへフレームを送出しない。** `twai_transmit()` を書かない。TWAI は `TWAI_MODE_NORMAL`
   で初期化し ACK は返す（`RSK-10`）。`TWAI_MODE_NO_ACK` は禁止。
   CI の `guard` ジョブが混入を検出する（`RSK-06` / `SYS-07` / `DEC-04`）。
2. **信号喪失時に古い値を返さない。** `Snapshot::get()` は `Lost` のとき `false` を返し
   出力を変更しない。この契約を壊す API（値と鮮度を別々に返すもの）を追加しない（`RSK-01`）。
3. **ドメイン層を HW に依存させない。** `lib/rusefi_can/` と `lib/signal_model/` に
   `Arduino.h` / `lvgl.h` / `driver/*` / `esp_*` / `freertos/*` を include しない。
   PC 上の単体テストが動かなくなる（`DEC-06`）。
4. **CAN のスケーリング値を直書きしない。** `lib/rusefi_can/include/rusefi_can_spec.h`
   の定数のみを使う（`SWD-01`）。
5. **実行時に動的メモリを確保しない**（LVGL 内部を除く）。UI オブジェクトは起動時に
   全て作り、表示切替は `LV_OBJ_FLAG_HIDDEN` で行う（`DOC-21 §6`）。

## 変更の進め方

- 要求（`STK-*` / `SYS-*` / `SWR-*`）を変えるなら、**実装より先に `docs/` を更新する PR** を出す。
- コミットメッセージは Conventional Commits + `Refs: <要求 ID>` 行。
  例: `feat(can): decode EGT1 from 0x209` / `Refs: SWR-08, UT-03`
- ドメインロジックを追加・変更したら `test/` に `UT-*` を追加する。
- `docs/50_traceability.md` を同じ PR で更新する。
- 変更した文書の「最終更新」日付を更新する。

## コマンド

```bash
pio test -e native
```

```bash
pio run -e m5dial
```

```bash
pio run -e m5dial -t upload
```

**LCD-2.1 版は PowerShell から実行する**（pioarduino は Git Bash では `idf_tools.py` が失敗する）。
画面だけを動かすスイープデモ（CAN 不要）:

```powershell
python -m platformio run -e lcd21_sim -t upload --upload-port COM10
```

実 CAN 版（物理層を繋いでから。`OPN-13` は実機未確認）:

```powershell
python -m platformio run -e lcd21 -t upload --upload-port COM10
```

デモは UART0（115200）から 1 文字送って操作できる: `h` 停止/再開、`0`-`9` その位置で停止、`m` AFR/λ 切替。
1 秒ごとにシリアルへ fps・1 フレームの描画/flush 時間・画素数が出る。`m5dial` / `m5dial_sim` は M5Dial 用なので、LCD-2.1 には書き込まない。

PowerShell からは `tools/test.ps1` / `tools/build.ps1` / `tools/flash.ps1` /
`tools/monitor.ps1` / `tools/flash_and_monitor.ps1`。
`pio` が PATH に無い場合は `python -m platformio` を使う。

`lcd21_bringup`（LCD-2.1 の計測用スケッチ）は pioarduino を使うので **PowerShell から** `pio run -e lcd21_bringup` を実行する
（Git Bash では `idf_tools.py` が失敗する）。詳細は `docs/40_SUP8` §2.3。

**書き込みは実機が USB 接続されているときだけ実行する。** CI では行わない。

## コードの置き場所

| ディレクトリ | 内容 | 制約 |
|---|---|---|
| `lib/rusefi_can/` | CAN フレームのデコード（純粋関数） | HW 非依存・状態を持たない |
| `lib/signal_model/` | 信号ストア・単位換算・設定・ボタン FSM・リング幾何・スイープ生成・表示ポリシー | HW 非依存 |
| `lib/hal/` | TWAI・CAN 受信タスク・表示/タッチ（`display_hal`）・入力・NVS・診断 | ESP32 依存。`display_hal_lcd21.cpp` は `CM_BOARD_LCD21` のときだけ有効 |
| `lib/ui/` | LVGL ポート・ページ・ウィジェット（`LambdaRing` / `BigNumber`）・テーマ | LVGL 依存。**表示を変えるときは差分無効化を保つ**（`DEC-08`。全面再描画は 78 ms） |
| `src/main.cpp` | 起動シーケンスとタスク生成 | |
| `test/` | native 単体テスト | |

## コーディング規約（要点）

C++17 / `.clang-format`（Google ベース・インデント 4・列幅 110）/ 例外と RTTI は不使用。
命名は 型 `PascalCase`、関数・変数 `camelCase`、定数 `kPascalCase`、メンバ `m_` 接頭辞。
**変数名に単位を含める**（`egtC`, `timeoutMs`, `lambdaRaw`）。
コメントは「何を」ではなく「なぜ」を書き、該当する要求 ID・設計判断 ID を参照する。

詳細は `docs/41_SUP1_qa_plan.md` §4。

## 現在の未解決事項

`docs/11_SYS2_system_req.md` §4 の `OPN-*` を参照。特に以下は実装を進める前に実測で確定する。

- `OPN-15` ビルドプラットフォーム。**pioarduino 55.03.312-1 + ESP32_Display_Panel + LVGL 9.2.2 で暫定決定**（`DEC-01` / `DEC-09`）。
  残り: TWAI の実機動作、第三者プラットフォームのサプライチェーン確認、CI でのビルド
- `OPN-12` 480×480 RGB の実描画性能。**λ ページは LVGL で平均 39 fps**（`SYS-12` 達成）。残り: CAN 受信中・NVS 書き込み中・他ページ
- `OPN-13` CAN を GPIO20(TX) / GPIO19(RX)（12PIN の D+/D−）に割り当てる案の実機検証。起動直後にバスへ何も出ないこと（`IT-04`）
- `OPN-14` 12PIN の `VBus` へ Mini CAN の 5V を入れて成立するか、本体の消費電流
- `OPN-01` Mini CAN Unit の終端抵抗の有無
- `OPN-03` rusEFI 側で `Lambda1` / `EGT1` が構成済みか、`canSleepPeriodMs` の実設定値

これらが未確定のまま「たぶんこうだろう」で実装を進めない。
先に `src/main.cpp` のブリングアップ版を書き込み、シリアルログで確認する。

## 未実装（フェーズ P4 以降）

実装済み: `include/lv_conf.h`、`LvglPort`、`LambdaRing`、`BigNumber`、`PageLambda`、`PageSplash`、`DisplayHal`、
`CanRxTask`、数字フォント（`tools/make_font.py`）、480×480 のロゴ。

- `PageManager` と λ ページ以外のページ（`PAGE_DUAL` / `ENGINE` / `ELEC` / `DIAG` / `SETTINGS`）
- タッチ入力（`InputDriver`。`DisplayHal::readTouch()` はある）
- `lib/hal/` の `NvsStore` / `Diagnostics`、ブザー（ピン未確認: `OPN-13`）
- `bench_check.py` / Skill の CH343（VID 1A86）対応と、`lcd_capture.py` を使った画面採点

## 表示の目視確認（UVC カメラ）

LCD は外付けの UVC カメラ（`5MP USB Camera`）で撮影できる構成になっている。
**表示の確認は撮影した画像を Read で見て行うこと**（人に「見えましたか」と聞く前に自分で見る）。
手順は `docs/40_SUP8_cm_dev_environment.md` §2.4.1。**PC 内蔵カメラ（`Integrated Camera`）は使わない。**

## 実機の接続（LCD-2.1）

- 書き込み・ログは **CH343P の UART Type-C**（`COM10`）。ネイティブ USB-C にはケーブルを挿さない（CAN と競合: `RSK-13`）
- PlatformIO の esptool を直接呼ぶときは `~/.platformio/penv/Scripts/python.exe` から（システムの Python だと `click` で落ちる）
- スクラッチファイルは指定のスクラッチパッドに置く（Git Bash の `/tmp` と Python の `/tmp` は別の場所を指す）
