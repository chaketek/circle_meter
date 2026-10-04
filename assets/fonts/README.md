# assets/fonts

| ファイル | 由来 | ライセンス |
|---|---|---|
| `Montserrat-Bold.ttf` | LVGL 9.2.2 の `demos/multilang/assets/fonts/` に同梱されているもの | SIL Open Font License 1.1（Copyright 2011 The Montserrat Project Authors） |
| `B612Mono-Bold.ttf` / `B612Mono-Regular.ttf` | [polarsys/b612](https://github.com/polarsys/b612)（Eclipse Foundation）の `fonts/ttf/`。2026-10-04 取得。エアバスと ONERA がコックピット表示用に設計した書体の等幅版 | SIL Open Font License 1.1（Copyright 2012 The B612 Project Authors）。全文は `B612-OFL.txt` |

`tools/make_font.py` がこの TTF から LVGL 用のビットマップフォント（`assets/lcd21/fonts/*.c`）を生成する。
生成したフォントは OFL の「Modified Version」には当たらず（グリフを点描画に変換しただけ）、
フォント単体で販売しない限り同梱・再配布できる。

B612 Mono は文字盤デザイン A（大森風・針表示）の数字に使う。数字がすべて同じ幅なので、値がパラパラ動いても
桁の位置が変わらない（航空機の表示と同じ理由で選んだ）。
