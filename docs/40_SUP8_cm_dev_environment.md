# 40. 構成管理・開発環境定義書 (SUP.8)

| 項目 | 内容 |
|---|---|
| 文書ID | `DOC-40` |
| プロセス | SUP.8 構成管理 / SUP.9 問題解決 / SUP.10 変更要求 / MAN.3 プロジェクト管理 / SPL.2 リリース |
| 版 | 0.1 (Draft) |
| 最終更新 | 2026-10-05 |

---

## 1. 構成アイテム一覧

構成管理の対象（= git で版管理し、リリース時に版数が確定するもの）。

| 分類 | 構成アイテム | 場所 | 版管理 |
|---|---|---|---|
| 文書 | ASPICE 文書一式 | `docs/*.md` | git |
| ソース | ファームウェア | `src/`, `lib/` | git |
| テスト | 単体テスト | `test/` | git |
| ビルド定義 | `platformio.ini`, `partitions.csv`, `sdkconfig` 相当 | ルート | git |
| CI 定義 | GitHub Actions ワークフロー | `.github/workflows/` | git |
| ツール | 書き込み・ログ・変換スクリプト | `tools/` | git |
| 資産 | ロゴ・フォント | `assets/` | git（生成元 PNG と生成物 C の両方） |
| ボード定義 | PlatformIO ボード定義・パネル設定 | `boards/`（`waveshare_lcd21.json`, `lcd21_conf/`） | git |
| LVGL 設定 | `lv_conf.h`（必要な項目だけ。残りは LVGL の既定値） | `include/` | git |
| ブリングアップ | 計測専用の使い捨てスケッチ（本番ではない） | `bringup/` | git |
| 外部依存 | PlatformIO platform / ライブラリ | `platformio.ini` に**バージョン固定**して記録 | git（`pio pkg` のロック相当） |
| 成果物 | `firmware.bin`, `firmware.elf` | GitHub Release / Actions artifact | タグに紐付け |

### 1.1 外部依存のバージョン固定方針

再現可能なビルドのため、以下は**必ずバージョンを明示**する（`^` や `latest` を使わない箇所を明記）。

| 依存 | 指定 | 固定の理由 |
|---|---|---|
| `platform` (espressif32) | 完全固定（例 `6.9.0`） | Arduino core / ESP-IDF のバージョンが変わると TWAI API・メモリ配置が変わる |
| `lvgl` | 完全固定（例 `9.2.2`） | v9 系で描画性能の差がある（`DOC-21 §2.4`）。FPS 実測結果とバージョンを対応付ける |
| `M5Dial` / `M5Unified` / `M5GFX` | 完全固定 | パネル初期化パラメータが変わると表示が崩れる |
| `platform`（LCD-2.1 版） | **pioarduino 55.03.312-1**（URL とバージョンを固定）。第三者製 | 公式の espressif32 7.1.3 は Arduino コアが 2.0.17 系で RGB パネルの bounce buffer が使えない（`OPN-15`） |
| `ESP32_Display_Panel` / `ESP32_IO_Expander` / `esp-lib-utils` | タグ固定（v1.0.4 / v1.1.0 / v0.2.0） | ピン・ST7701 初期化列・タイミングの定義を含む |

依存を更新する際は**必ず `QT-03`（FPS 実測）を再実行**し、結果を PR に記載する。

## 2. 開発環境

### 2.1 前提環境（ホスト）

| 項目 | 値 | 確認コマンド |
|---|---|---|
| OS | Windows 11 | — |
| シェル | PowerShell 7 / Git Bash | — |
| Git | 2.52 以降 | `git --version` |
| Python | 3.11 以降（確認済: 3.13） | `python --version` |
| PlatformIO Core | 6.1 以降 | `pio --version` |
| GitHub CLI | 2.x（任意、PR 操作用） | `gh --version` |
| C++ コンパイラ | native 単体テスト用。Windows は `scoop install gcc`（確認済: MinGW GCC 15.2） | `g++ --version` |
| `python-can` | PCAN からの CAN 送出（`DOC-30 §3`） | `python -c "import can"` |
| Pillow | ロゴ変換 `tools/make_logo.py` | `python -c "import PIL"` |
| clang-format | CI の lint ゲートをローカルで再現する | `python -m pip install --user clang-format` |
| PEAK PCAN ドライバ | PCAN-USB を使う結合テスト | PCAN-View が起動すること |
| エディタ | VS Code + PlatformIO IDE 拡張（任意） | — |

### 2.2 初回セットアップ

```bash
python -m pip install --user --upgrade platformio python-can pillow clang-format
```

インストール後、`pio` が PATH に無い場合は `python -m platformio` で代用できる。
本プロジェクトのスクリプトは `pio` が無ければ自動的に `python -m platformio` へフォールバックする。

依存パッケージ（ツールチェーン・ライブラリ）は初回ビルド時に自動取得される。

```bash
pio run -e m5dial          # ファームウェアビルド（初回は 5-10 分）
pio test -e native         # ドメイン層の単体テスト（PC 上で実行）
```

### 2.3 ターゲット環境

| 項目 | 値 |
|---|---|
| ボード | **Waveshare ESP32-S3-Touch-LCD-2.1 / 2.1B**（ESP32-S3R8） |
| PlatformIO board | `boards/waveshare_lcd21.json`（自作）。**環境は 4 つ**: `lcd21`（実 CAN。GPIO20 TX / GPIO19 RX、500 kbps）、`lcd21_1m`（`lcd21` の CAN を 1 Mbps にしたもの。作者の車両用。DOC-13 §1）、`lcd21_sim`（CAN の代わりに λ / EGT をスイープ）、`lcd21_bringup`（計測専用）。**プラットフォームは pioarduino 55.03.312-1**（`OPN-15`: TWAI の実機動作とサプライチェーン確認が残り）。**Git Bash では `idf_tools.py` が `MSys/Mingw is not supported` で失敗するので PowerShell から実行する**。初回に `~/.platformio/penv` が Python 3.13 の venv に作り直される（システムの `python -m platformio` と `m5dial` 環境は影響を受けない）。最適化は **-O2**（Arduino-ESP32 既定の -Os を外す。描画が CPU 律速のため） |
| framework | `arduino` |
| Flash | **16 MB**, QIO, 80 MHz |
| PSRAM | **8 MB Octal**（実機の esptool 応答で確認済み）。`memory_type = qio_opi`（Flash は Quad、PSRAM は Octal） |
| パーティション | **新規作成が必要**（16 MB 用） |
| CPU | 240 MHz |
| 書き込み・ログ | **CH343P の UART Type-C**（実機で `COM10` として認識。自動書き込み回路あり）。**ネイティブ USB-C にはケーブルを挿さない**（12PIN の D−/D+ を CAN に使うため競合する: `RSK-13`） |

### 2.4 書き込み手順

本ボードは **CH343P の UART Type-C** から自動リセットで書き込める
（実機で esptool が接続し、チップ情報を取得できることを確認した）。

```bash
python -m platformio device list
```

`USB-Enhanced-SERIAL CH343` として見えるポートが対象。ポートを明示して書き込む。

> **PlatformIO の esptool をシステムの Python から直接呼ぶと失敗する**（`click` のバージョン不整合）。
> `C:\Users\<ユーザー>\.platformio\penv\Scripts\python.exe` から呼ぶこと。`pio run -t upload` は問題ない。

書き込みモードに入らない場合は **BOOT ボタンを押しながら RESET** を押す。

**ネイティブ USB-C にはケーブルを挿さない。** 12PIN の D−/D+（GPIO19/20）を CAN に使うため、
挿すと CAN のトランシーバと信号が競合する（`RSK-13`）。

### 2.4.1 表示の目視確認（UVC カメラ）

LCD を UVC カメラで撮影し、表示を目で確認できる構成にしてある。
画像はファイルとして取得できるので、Claude Code から直接見て確認できる。

```bash
ffmpeg -f dshow -video_size 1280x720 -i video="5MP USB Camera" -frames:v 1 -update 1 out.jpg
```

> PC 内蔵カメラ（`Integrated Camera`）は使わない。LCD の確認には外付けのカメラのみを使う。

`tools/lcd_capture.py` がこの撮影を包んでいる（PC 内蔵カメラは名前で拒否する。`--burst N` で連写）。
カメラの設置状態により **画面は 180° 回転して写る**。色はカメラの癖で白が水色に寄る（白 = R 162 / G 245 / B 250 程度）ので、
色は絶対値ではなく相対差で判断する。**動いている数字は露光中に重なって写り、残像のように見える**ので、
見た目の確認は停止状態（下の操作キー）で撮る。fps・描画量は撮影ではなくシリアルの計測値で見る。

### 2.5 車両に接続せずに開発する方法（`SYS-61` / `SWR-100`）

| 手段 | 用途 | 使い方 |
|---|---|---|
| **native 単体テスト** | デコード・鮮度管理・単位換算のロジック検証 | `pio test -e native` |
| **CAN シミュレータ（内蔵）** | 実機で UI の見た目と FPS を確認 | `pio run -e m5dial_sim -t upload`（λ・EGT をスイープ） |
| **画面スイープデモ（LCD-2.1）** | 物理層なしで λ ゾーン・EGT 警告・fps を確認 | PowerShell で `pio run -e lcd21_sim -t upload`。λ が 0.68 <-> 1.36 を 8.5 秒で往復し、EGT は 5 °C 刻みで 300 <-> 960 °C。UART0 から 1 文字送って操作: `h` 停止/再開、`0`-`9` その位置で停止、`m` AFR/λ 切替、`v` 表示デザイン切替（指針式 A <-> リング。`SYS-21`）。1 秒ごとにシリアルへ fps・描画時間・画素数を出す |
| **PCAN からの送出** | ベンチで実バスを模擬（`IT-*` / `QT-*`） | `python tools/pcan_send.py --mode sweep`（要 PEAK ドライバ + `python-can`）。詳細は `DOC-30 §3` |

`tools/replay/` の CSV 形式:

```
# time_ms, can_id(hex), dlc, d0, d1, d2, d3, d4, d5, d6, d7
0,207,8,10,27,00,00,00,00,00,00
50,209,8,A9,00,00,00,00,00,00,00
```

## 3. ブランチ戦略・コミット規約

### 3.1 ブランチ

| ブランチ | 用途 | 保護 |
|---|---|---|
| `main` | 常にビルド可能・実機で動作する状態を保つ | **直接 push 禁止**。PR 経由のみ |
| `feat/<topic>` | 機能追加 | — |
| `fix/<topic>` | 不具合修正 | — |
| `docs/<topic>` | 文書のみの変更 | — |
| `chore/<topic>` | ビルド・CI・依存更新 | — |

### 3.2 コミットメッセージ

Conventional Commits に準拠する。

```
<type>(<scope>): <要約>

<本文（任意）>

Refs: SWR-07, UT-02
```

- `type`: `feat` / `fix` / `docs` / `test` / `chore` / `refactor` / `perf`
- `scope`: `can` / `signal` / `ui` / `hal` / `build` / `docs`
- **要求 ID を `Refs:` 行に書く**。これが `DOC-50` トレーサビリティの実データになる。

### 3.3 バージョニング

セマンティックバージョニング `vMAJOR.MINOR.PATCH`。git タグで付与。

| 増分 | 条件 |
|---|---|
| MAJOR | CAN インタフェース仕様（`DOC-13`）の非互換変更、設定構造体の非互換変更 |
| MINOR | ページ追加・機能追加 |
| PATCH | 不具合修正・文書修正 |

ファームウェアには `git describe --tags --always --dirty` の結果を埋め込む（`SWR-102`）。

## 4. CI/CD

### 4.1 パイプライン構成

```mermaid
flowchart LR
    A["PR 作成 / push"] --> B["lint: clang-format 差分チェック"]
    A --> C["guard: 禁止 API 検査\n(twai_transmit 等)"]
    A --> D["test: pio test -e native\nUT-* 全件"]
    A --> E["build: pio run -e m5dial\n+ サイズレポート"]
    B & C & D & E --> F{"全て green?"}
    F -->|No| G["マージ不可"]
    F -->|Yes| H["firmware.bin を artifact 保存"]
    H --> I["main へマージ"]
    I --> J["タグ push 時: GitHub Release 作成\n+ firmware.bin 添付"]
    J --> K["★ 実機書き込みはローカル\ntools/flash.ps1"]
```

### 4.2 ジョブ定義

| ジョブ | 実行環境 | 内容 | 失敗時 |
|---|---|---|---|
| `lint` | ubuntu-latest | `clang-format --dry-run -Werror` | マージ不可 |
| `guard` | ubuntu-latest | `bash tools/guard.sh`（`twai_transmit` の混入 `RSK-06`、`TWAI_MODE_NO_ACK` `RSK-10`、ドメイン層の HW 依存 `DEC-06`、スケーリング定数の直書き `SWD-01`） | マージ不可 |
| `test` | ubuntu-latest | `pio test -e native` | マージ不可 |
| `build` | ubuntu-latest | `pio run -e m5dial -e m5dial_sim` + Flash/RAM 使用量をジョブサマリに出力 | マージ不可 |
| `build-lcd21` | ubuntu-latest | `pio run -e lcd21 -e lcd21_sim`（pioarduino。LVGL / ESP32_Display_Panel を含む） | マージ不可（初回の実績を見て判断） |
| `release` | ubuntu-latest | タグ push 時のみ。`firmware.bin` `firmware.elf` を Release に添付 | — |

### 4.3 実機への書き込みが CI に含まれない理由

GitHub Actions のランナーは M5Dial に物理接続できない。
**書き込みはローカル実行**とし、Claude Code から以下を呼べるようにする。

| スクリプト | 内容 |
|---|---|
| `tools/build.ps1` | `pio run -e m5dial` |
| `tools/test.ps1` | `pio test -e native` |
| `bash tools/guard.sh` | CI の `guard` ジョブと同じ静的チェックをローカルで実行 |
| `tools/bench_check.ps1` | PCAN ベンチで CAN パターンを流し、表示を目視確認する（`DOC-30 §3`）。Claude Code からは Skill `bench-check` |
| `tools/can_send.ps1` | PCAN からの送出のみ（採点なし） |
| `tools/flash.ps1 [-Port COM5] [-Sim]` | ビルド + 書き込み。`-Sim` で CAN シミュレータ版 |
| `tools/monitor.ps1 [-Port COM5]` | シリアルモニタ（115200 bps） |
| `tools/flash_and_monitor.ps1` | 書き込み後そのままモニタ |

これらは `.claude/settings.json` の `permissions.allow` に登録済みであり、
Claude Code から都度の承認なしに実行できる（`SUP.8` としての「自動ビルド・書き込み」の実体）。

将来、自宅に常設のセルフホストランナー（USB で M5Dial を接続した PC）を置けば
`build` の後段に `flash` ジョブを追加して完全自動化できる。本フェーズでは対象外。

### 4.4 Claude Code から実行する典型フロー

```bash
tools/test.ps1          # ロジック変更後: まず単体テスト
tools/build.ps1         # ビルドが通るか
tools/flash.ps1         # 実機へ書き込み
tools/monitor.ps1       # 起動ログと診断出力を確認
```

## 5. 問題解決管理 (SUP.9) / 変更要求管理 (SUP.10)

GitHub Issues で一元管理する。文書は作らず、ラベルで区別する。

| ラベル | 意味 | 必須記載事項 |
|---|---|---|
| `bug` | 不具合 | 再現手順、期待動作、実際の動作、FW 版数、違反している要求 ID |
| `change-request` | 仕様変更要求 | 変更理由、影響を受ける要求 ID、影響範囲の見積 |
| `question` | 未解決事項（`OPN-*`） | — |
| `risk` | リスク（`RSK-*`） | 影響度・発生度・対策 |

**変更要求の処理規則**:
1. 要求（`STK-*` / `SYS-*` / `SWR-*`）を変更する場合、**先に文書を更新する PR** を出す。
2. 文書 PR がマージされてから実装 PR を出す。
3. 実装 PR のコミットに `Refs: <変更後の要求 ID>` を記載する。

これにより「実装が先に進んで文書が置き去りになる」ことを構造的に防ぐ。

## 6. リリース手順 (SPL.2)

1. `main` の CI が全て green であることを確認する。
2. 実機ゲート `QT-01` – `QT-09`（`DOC-30`）を実行し、結果を `docs/test_records/` に記録する。
3. `CHANGELOG.md` を更新する。
4. `git tag -a v0.1.0 -m "..."` → `git push --tags`。
5. Actions の `release` ジョブが `firmware.bin` を添付した Release を自動作成する。
6. Release ノートに「このバージョンで実行した実機ゲートの結果」へのリンクを記載する。

## 7. フェーズ計画 (MAN.3)

| フェーズ | 内容 | 完了条件 | 状態 |
|---|---|---|---|
| **P0. 文書化** | `DOC-00` – `DOC-50` 作成 | 本文書一式のレビュー完了 | ✅ 完了 (2026-09-21) |
| **P1. 骨格** | リポジトリ構成、`platformio.ini`、CI、ドメイン層、単体テスト、ブリングアップ FW | `pio test -e native` (45 件) と `pio run -e m5dial` が green、実機で起動確認 | ✅ 完了 (2026-09-21) |
| **P2. ブリングアップ（M5Dial）** | `OPN-01` – `OPN-03` の実測解決。CAN 受信が動くこと | ベンチで `0x207` / `0x209` を受信し、シリアルに値が出る | ✅ 完了 (2026-09-21)。`IT-01/02/03/04/15` 合格 |
| **P2.5 ボード移行** | 対象ハードを Waveshare ESP32-S3-Touch-LCD-2.1 へ変更（`DOC-12 §8`） | ① 文書改訂 ② `OPN-15`（プラットフォーム・表示ライブラリ）決定 ③ `OPN-12`（480×480 RGB の実描画性能）実測 ④ `OPN-13`（CAN を GPIO19/20 に割り当てて PCAN と通信）確認 ⑤ `OPN-14`（`VBus` 給電・消費電流）確認 | ⏳ 実施中（① 完了、② 暫定決定（pioarduino + ESP32_Display_Panel + LVGL 9.2.2。TWAI とサプライチェーン確認が残る）、③ λ ページは達成（LVGL で平均 39 fps）、④⑤ 未着手） |
| **P3. ドメイン実装** | デコーダ・信号ストア・単位換算の完成 + `UT-*` 全件 | `UT-01` – `UT-13` 全て pass | 未着手 |
| **P4. HMI 実装** | λ リング・数値・EGT・ページ管理・スプラッシュ | `QT-02` / `QT-03` 合格 | ⏳ 着手（2026-10-02）。**λ ページ・スプラッシュ・輝度・EGT 警告・シミュレータデモが LCD-2.1 実機で動作**。未実装: 他ページ、`PageManager`、タッチ入力（`SWD-06`）、設定メニュー、診断ページ |
| **P5. 設定・診断** | 設定メニュー・NVS・診断ページ | `IT-05` / `IT-13` 合格 | 未着手 |
| **P6. 車両検証** | 実車搭載、`QT-*` 全件、1 時間連続走行 | `DOC-10 §6` 受入基準を全て満たす | 未着手 |
| **P7. v1.0 リリース** | タグ付け、Release 作成 | — | 未着手 |

**P2 が最大の不確実性**である（`OPN-01` – `OPN-03`）。
P1 完了後、P3/P4 の本実装に入る前に P2 を先に片付けることで、
「作ったが CAN が繋がらない」という手戻りを避ける。

## 8. バックアップ・可用性

| 対象 | 方法 |
|---|---|
| ソース・文書 | GitHub リモートリポジトリ（`origin/main`） |
| リリース成果物 | GitHub Releases |
| 車載 FW の書き戻し | Release の `firmware.bin` を `esptool` で直接書き込み可能（PlatformIO 環境が無くても復旧できる） |
