# 1. 準備するもの

[← 目次](README.md) ｜ 次: [2. Claude Code と Git を入れる →](02_install_claude_git.md)

## メータの部品

| もの | 内容 | 備考 |
|---|---|---|
| 本体 | **Waveshare ESP32-S3-Touch-LCD-2.1**（または 2.1B） | 丸い 2.1 インチの画面。付属の 12 ピンのケーブルを使う |
| CAN ユニット | **M5Stack Unit Mini CAN** | 車の CAN の信号を本体に渡し、車の 12V から本体用の 5V も作る |
| Grove ケーブル | Mini CAN に付属 | Mini CAN と本体の 12 ピンのケーブルを繋ぐ |
| USB ケーブル | **USB-C、データ通信できるもの** | 充電専用のケーブルでは書き込めません |

## 車の側

- **rusEFI の ECU**（uaEFI など）と、**rusEFI のワイドバンドコントローラ（WBO）**。uaEFI は WBO を内蔵しています。
- 排気温度も表示したい場合は、ECU で排気温度センサ（EGT）が設定済みであること。
- CAN の配線（CAN_H / CAN_L）にメータを繋げる場所。
- ECU の設定を見るための **TunerStudio**（[4. 書き込みと配線](04_flash_and_wire.md) で設定を確認します）。

## PC とアカウント

| もの | 内容 |
|---|---|
| Windows の PC | Windows 11 / 10。空き容量 **5 GB 以上**（書き込み用の道具を初回に自動でダウンロードします） |
| インターネット | 初回の準備と、更新のときに使います |
| **Claude のアカウント（有料プラン）** | Claude Code を使うのに必要です。どのプランで使えるかは Anthropic の案内（claude.com）で確認してください |
| GitHub のアカウント | **不要** です（プログラムは公開されているので、そのまま手に入ります） |

用意できたら [2. Claude Code と Git を入れる](02_install_claude_git.md) へ進みます。
