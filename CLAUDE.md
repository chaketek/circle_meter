# circle_meter — Claude Code 向けプロジェクト指示

rusEFI と CAN 接続する車載メータ（M5Stack Dial v1.1 / ESP32-S3）のファームウェア。
ASPICE をテーラリングした文書体系で管理している。**文書が仕様であり、コードは文書に従う。**

## 最初に読むもの

- `docs/README.md` — 文書体系の入口
- `docs/13_ICD_rusefi_can.md` — CAN のバイト配置。**CAN 関連を触る前に必ず読む**
- `docs/23_HMI_design.md` — 画面の配色・レイアウト。**UI を触る前に必ず読む**
- `docs/40_SUP8_cm_dev_environment.md` §7 — 今どのフェーズか

## 絶対に守ること

1. **CAN バスへ送信しない。** `twai_transmit()` を書かない。TWAI は `TWAI_MODE_LISTEN_ONLY`
   以外で初期化しない。CI の `guard` ジョブが混入を検出する（`RSK-06` / `SYS-07`）。
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

PowerShell からは `tools/test.ps1` / `tools/build.ps1` / `tools/flash.ps1` /
`tools/monitor.ps1` / `tools/flash_and_monitor.ps1`。
`pio` が PATH に無い場合は `python -m platformio` を使う。

**書き込みは実機が USB 接続されているときだけ実行する。** CI では行わない。

## コードの置き場所

| ディレクトリ | 内容 | 制約 |
|---|---|---|
| `lib/rusefi_can/` | CAN フレームのデコード（純粋関数） | HW 非依存・状態を持たない |
| `lib/signal_model/` | 信号ストア・単位換算・設定・ボタン FSM | HW 非依存 |
| `lib/hal/` | TWAI・入力・表示・NVS・診断 | ESP32 依存 |
| `lib/ui/` | LVGL ポート・ページ・ウィジェット・テーマ | LVGL 依存（P4 で実装） |
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

- `OPN-01` 所有している CAN Unit の型番と終端抵抗の有無
- `OPN-02` M5Dial PORT.B の GPIO 割当（G1/G2）と Grove 線色の対応、TX/RX の向き
- `OPN-03` rusEFI 側で `Lambda1` / `EGT1` が構成済みか、`canSleepPeriodMs` の実設定値

これらが未確定のまま「たぶんこうだろう」で実装を進めない。
先に `src/main.cpp` のブリングアップ版を書き込み、シリアルログで確認する。

## 未実装（フェーズ P4 以降）

- `include/lv_conf.h` — LVGL の設定ヘッダ。LVGL を使い始めるときに作成する
- `lib/ui/` 一式（`LvglPort` / `PageManager` / `IPage` 実装 / `LambdaRing`）
- `lib/hal/` の `InputDriver` / `DisplayHal` / `NvsStore` / `Diagnostics`
- `assets/` のロゴ・数字フォント（`OPN-05`）
