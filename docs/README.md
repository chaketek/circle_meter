# circle_meter ドキュメント体系

本プロジェクトは **Automotive SPICE (ASPICE) をテーラリングした文書体系**で管理しています。
個人開発 1 名という体制に合わせて、組織プロセス・調達プロセスは除外し、
**要求からコードまでのトレーサビリティ**だけを実質的に維持しています。

テーラリングの範囲と、何をなぜ除外したかは [DOC-00](00_ASPICE_tailoring.md) に記載しています。

## 読む順番

初めて読む場合は上から順に読んでください。

| # | 文書 | プロセス | 何が書いてあるか |
|---|---|---|---|
| 1 | [DOC-00 ASPICE テーラリング](00_ASPICE_tailoring.md) | — | 文書体系のルール、ID 体系、レビュー運用 |
| 2 | [DOC-10 ステークホルダ要求](10_SYS1_stakeholder_req.md) | SYS.1 | 「何が欲しいか」。運用コンセプトと受入基準 |
| 3 | [DOC-11 システム要求](11_SYS2_system_req.md) | SYS.2 | 「何を作るか」を数値で。リスク・ハザード分析、未解決事項 |
| 4 | [DOC-12 システムアーキテクチャ](12_SYS3_system_arch.md) | SYS.3 | HW 構成、GPIO 割当、HW/SW 機能配分、主要な設計判断 |
| 5 | [DOC-13 rusEFI CAN ICD](13_ICD_rusefi_can.md) | SYS.3 | **CAN フレームのバイト単位の定義**。実装の一次資料 |
| 6 | [DOC-20 ソフトウェア要求](20_SWE1_software_req.md) | SWE.1 | ソフトウェアが満たすべきこと |
| 7 | [DOC-21 ソフトウェアアーキテクチャ](21_SWE2_software_arch.md) | SWE.2 | レイヤ構成、描画ライブラリ選定、タスク設計、遅延バジェット |
| 8 | [DOC-22 詳細設計](22_SWE3_detailed_design.md) | SWE.3 | 公開 API の契約と状態遷移 |
| 9 | [DOC-23 HMI 設計](23_HMI_design.md) | SWE.2 | **画面レイアウト、配色、ページ構成、フォント** |
| 10 | [DOC-30 テスト戦略](30_test_strategy.md) | SWE.4/5/6 | UT / IT / QT の全テストケース。リリース絶対ゲート |
| 11 | [DOC-40 構成管理・開発環境](40_SUP8_cm_dev_environment.md) | SUP.8 | **開発環境、ブランチ運用、CI/CD、リリース手順、フェーズ計画** |
| 12 | [DOC-41 品質保証計画](41_SUP1_qa_plan.md) | SUP.1 | 品質ゲート、静的チェック、コーディング規約 |
| 13 | [DOC-50 トレーサビリティ](50_traceability.md) | — | 要求 ↔ 設計 ↔ 検証の対応表 |

## 目的別の入口

| やりたいこと | 読む文書 |
|---|---|
| CAN のバイト配置を知りたい | [DOC-13](13_ICD_rusefi_can.md) |
| 画面の色・レイアウトを変えたい | [DOC-23](23_HMI_design.md) |
| 開発環境を作りたい | [DOC-40 §2](40_SUP8_cm_dev_environment.md) |
| ビルド・書き込みのコマンドを知りたい | [DOC-40 §4.3](40_SUP8_cm_dev_environment.md) |
| なぜ LVGL を選んだのか知りたい | [DOC-21 §2](21_SWE2_software_arch.md) |
| 実車に載せる前に何を確認すべきか | [DOC-30 §5](30_test_strategy.md) |
| 次に何をやるのか知りたい | [DOC-40 §7](40_SUP8_cm_dev_environment.md) |

## ID 体系（早見表）

```
STK-nn  ステークホルダ要求        SYS-nn  システム要求
SA-nn   システムアーキ要素        SWR-nn  ソフトウェア要求
SWA-nn  SW コンポーネント         SWD-nn  詳細設計ユニット
DEC-nn  設計判断                  CST-nn  制約条件
RSK-nn  リスク                    OPN-nn  未解決事項
UT-nn   ユニットテスト            IT-nn   結合テスト        QT-nn  適格性確認テスト
```

詳細は [DOC-00 §5](00_ASPICE_tailoring.md)。

## 文書を変更するときのルール

1. **要求を変えるなら、実装より先に文書 PR を出す**（[DOC-40 §5](40_SUP8_cm_dev_environment.md)）。
2. 要求の ID は再利用しない。廃止は取り消し線で残す。
3. 要求を追加したら [DOC-50](50_traceability.md) に上位 ID と検証 ID を書く。
4. 変更した文書の「最終更新」日付を更新する。

## まだ決まっていないこと

実装を進める前に潰すべき未解決事項は [DOC-11 §4](11_SYS2_system_req.md) の `OPN-*` にまとめています。
特に `OPN-01`（CAN Unit の型番と終端抵抗）、`OPN-02`（PORT.B の配線）、
`OPN-03`（rusEFI 側の設定）は、フェーズ P2 のブリングアップで最優先で確定させます。
