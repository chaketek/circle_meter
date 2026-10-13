---
name: customize-meter
description: circle_meter（丸型の空燃比・排気温度メータ）の表示を好みに変える。色（針・盤面・λ ゾーン・警告）、既定の表示（AFR/λ、指針式/リング、明るさ、理論空燃比）、λ の色の境目、排気温度の警告温度、CAN の通信速度とベース ID、起動ロゴの画像を、会話で聞き取って user/user_config.h（とロゴの変換）だけを変え、書き込みまで行う。配布先ユーザーが「色を変えたい」「針を青に」「AFR じゃなく λ で」「警告を 800 度に」「ロゴを自分の画像に」「元に戻して」と言ったときに使う。
---

# 表示を変える（配布先ユーザー向け）

手順書は `docs/user/05_customize.md`（設定）と `docs/user/06_logo.md`（ロゴ）。
設定の一覧と範囲は `user/user_config.example.h` が正。

## 守ること

- **書き換えてよいのは `user/` フォルダの中だけ**（`user_config.h`、`logo.png`、変換で作る `generated/` と `logo_preview_*.png`）。
  `lib/`・`src/`・`include/`・`platformio.ini`・`docs/` は変えない。`user_config.h` で変えられないことを頼まれたら、
  できないことを正直に伝え、作者（GitHub の Issue）に相談する方法を案内する。
- **`user_config.example.h` は編集しない**（作者の更新で上書きされる）。
- 専門用語は一言で言い換える。色は名前と 16 進数（`0x00A0FF` など）の両方で示す。
- 警告・危険の色（`USER_COLOR_WARN` / `USER_COLOR_DANGER` / `USER_COLOR_LEAN_HEAVY`）を暗い色や盤面と似た色にしようとしたら、
  「走行中に目の端で気づけるよう、明るく目立つ色を勧めます」と一度だけ伝える（決めるのはユーザー）。
- CAN の送信に関わる設定は存在しない。頼まれても作らない。

## 手順

### 1. 準備

`user/user_config.h` が無ければ `user/user_config.example.h` をコピーして作る。

### 2. 何を変えるか聞き取る

ユーザーの言葉を `USER_*` の項目に置き換える。曖昧なら候補を 2〜3 個出して選んでもらう
（例: 「青」→ `0x2E7DFF`（明るい青）/ `0x0050FF`（濃い青）/ `0x00A0FF`（空色））。

| 言われそうなこと | 項目 |
|---|---|
| AFR / λ で表示 | `USER_SHOW_AFR`（1 = AFR、0 = λ） |
| 針のメータ / リングのメータ | `USER_DIAL_STYLE`（`USER_DIAL_NEEDLE_A` / `USER_DIAL_RING`）。**M5Dial はリングのみ**なので変えられないと伝える |
| 明るさ | `USER_BRIGHTNESS`（1〜5） |
| E85 など燃料が違う | `USER_STOICH`（理論空燃比） |
| 緑の範囲を広く・狭く | `USER_LAMBDA_*_MAX`（λ の値。小さい順にすること） |
| 排気温度の警告 | `USER_EGT_WARN_C` / `USER_EGT_DANGER_C`（警告 < 危険） |
| 針・盤面・目盛り・文字の色 | `USER_COLOR_NEEDLE` / `USER_COLOR_DIAL_*`（LCD-2.1 の指針式だけ）/ `USER_COLOR_TEXT*` / `USER_COLOR_BG`（M5Dial の画面全体の背景） |
| ゾーンの色 | `USER_COLOR_RICH_HEAVY` 〜 `USER_COLOR_LEAN_HEAVY` |
| 車に繋いでも表示されない（通信速度） | `USER_CAN_BITRATE_KBPS`（250 / 500 / 1000） |
| rusEFI のベース ID を変えている | `USER_CAN_BASE_ID` |
| 起動ロゴ | 手順 5 |

### 3. `user/user_config.h` を書き換える

- 該当行の先頭の `// ` を消し、値を書き換える。**同じ `#define` を 2 回書かない**（既にあればその行を直す）。
- 範囲は `user_config.example.h` のコメントのとおり。範囲外ならビルドが日本語のメッセージで止まる。
- 書き換えたら、変更前と変更後を表で見せる。

| 項目 | 変更前 | 変更後 |
|---|---|---|
| 針の色 | オレンジ `0xFF7014`（既定） | 空色 `0x00A0FF` |

### 4. 書き込んで確かめる

`flash-meter` Skill の手順 2〜3 に従う（今使っているのがデモか車両用かを確かめて、同じ種類で書き込む）。
`meter_log.py` の「反映した項目」に変えた項目が出ていること、`範囲外` が無いことを確かめる。
**色（`USER_COLOR_*`）はビルド時に埋め込まれるので「反映した項目」には出ない**（出るのは表示・閾値・CAN の組だけ）。色は画面で確かめてもらう。
そのうえで「画面の針は空色になりましたか？」のように、変えた点を 1 つずつ見てもらう。

### 5. 起動ロゴを変える

1. 画像の場所を聞き、**ユーザーの許可を得てから** `user/logo.png` にコピーする（PNG 以外は Pillow で PNG に変換してよい）。
2. 画像を Read で見て、種類を判断する。
   - 黒い背景・透過背景・写真やカラーのロゴ → そのまま: `python tools/make_logo.py --user`
   - **白い背景に黒（と 1 色）の文字** → 黒地に反転: `python tools/make_logo.py --user --invert`
3. 使っている機種の `user/logo_preview_lcd21.png` または `user/logo_preview_m5dial.png`（丸い画面に置いた見え方）を Read で見て、ユーザーにもファイルを開いて見てもらう。変換は両方の機種の分が一度にできる（`user/generated/<機種>/`）。
   - 白い四角が浮いている → `--invert` を試す
   - 小さすぎる・余白が多い → 画像の余白を切ってもらうか、横長の画像にしてもらう（枠は LCD-2.1 が 408x174、M5Dial が 204x140）
4. 手順 4 で書き込む。起動直後の 3 秒にロゴが出る。

### 6. 元に戻す

- 1 項目だけ: その行の先頭に `// ` を付け直す。
- 全部: `user/user_config.h` を `user/user_config.example.h` で上書きする（**上書き前に確認する**）。
- ロゴ: `user/generated/` フォルダを削除する（`logo.png` は残してよい）。削除の前に確認する。

戻したら手順 4 で書き込む。
