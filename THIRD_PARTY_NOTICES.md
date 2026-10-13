# 第三者のソフトウェア・フォントのライセンス

circle_meter 自体（このリポジトリのコード・ツール・文書・起動ロゴ）は [MIT ライセンス](LICENSE)（Copyright (c) 2026 chaketek）です。
ただし、次のものは**それぞれのライセンスに従います**（MIT ではありません）。

## リポジトリに含まれているもの

| もの | 置き場所 | ライセンス | ライセンス文 |
|---|---|---|---|
| B612 Mono（フォント） | `assets/fonts/B612Mono-*.ttf` と、それを変換した `assets/lcd21/fonts/cm_font_b612_*.c` | SIL Open Font License 1.1（Copyright 2012 The B612 Project Authors） | [assets/fonts/B612-OFL.txt](assets/fonts/B612-OFL.txt) |
| Montserrat（フォント） | `assets/fonts/Montserrat-Bold.ttf` と、それを変換した `assets/lcd21/fonts/cm_font_{afr,egt,lambda}.c` | SIL Open Font License 1.1（Copyright 2011 The Montserrat Project Authors） | [assets/fonts/Montserrat-OFL.txt](assets/fonts/Montserrat-OFL.txt) |

OFL のフォントは、単体で販売しない限りソフトウェアに同梱して配布できます。フォントとその変換物は OFL のままです。

## ビルドのときに取得され、ファームウェアに組み込まれるもの

リポジトリには含まれません（PlatformIO がビルドのときに取得します）。**ビルドしたファームウェアを配布するとき**は、
これらのライセンスにも従う必要があります。CI の Release には、取得したライブラリのライセンス文（`THIRD-PARTY-LICENSE-*.txt`）を同梱しています。

| もの | 使う機種 | ライセンス | 入手先 |
|---|---|---|---|
| LVGL 9.2.2 | LCD-2.1 | MIT | https://github.com/lvgl/lvgl |
| ESP32_Display_Panel / ESP32_IO_Expander / esp-lib-utils | LCD-2.1 | Apache License 2.0 | https://github.com/esp-arduino-libs |
| M5Dial / M5Unified / M5GFX | M5Dial | M5Unified・M5GFX は MIT。M5Dial はリポジトリにライセンスの明示なし | https://github.com/m5stack |
| Arduino-ESP32（Arduino コア） | 両方 | **GNU LGPL 2.1 以降** | https://github.com/espressif/arduino-esp32 |
| ESP-IDF（Arduino コアが内部で使う） | 両方 | Apache License 2.0（一部のコンポーネントは各自のライセンス） | https://github.com/espressif/esp-idf |
| Unity（単体テストのみ。ファームウェアには入らない） | — | MIT | https://github.com/ThrowTheSwitch/Unity |

### LGPL 2.1（Arduino-ESP32）について

ファームウェアは Arduino-ESP32 を静的にリンクしています。LGPL 2.1 は、受け取った人がライブラリを差し替えて作り直せることを求めます。
circle_meter はソースコードとビルド手順（`platformio.ini`、[docs/user/](docs/user/README.md)、[DOC-40](docs/40_SUP8_cm_dev_environment.md)）を
すべて公開しているので、誰でも別の版の Arduino-ESP32 で作り直せます。Arduino-ESP32 のソースは上の入手先から得られます。
