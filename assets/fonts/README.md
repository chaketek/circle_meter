# assets/fonts

| ファイル | 由来 | ライセンス |
|---|---|---|
| `Montserrat-Bold.ttf` | LVGL 9.2.2 の `demos/multilang/assets/fonts/` に同梱されているもの | SIL Open Font License 1.1（Copyright 2011 The Montserrat Project Authors） |

`tools/make_font.py` がこの TTF から LVGL 用のビットマップフォント（`assets/lcd21/fonts/*.c`）を生成する。
生成したフォントは OFL の「Modified Version」には当たらず（グリフを点描画に変換しただけ）、
フォント単体で販売しない限り同梱・再配布できる。
