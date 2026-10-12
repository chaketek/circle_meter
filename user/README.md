# user フォルダ（あなた用の設定を置く場所）

メータの表示を自分好みに変えるときは、**このフォルダの中だけ**を変えます。
このフォルダのファイルは作者の更新（最新版の取り込み）で上書きされません。

| ファイル | 内容 |
|---|---|
| `user_config.example.h` | 設定の雛形（作者が用意。直接は編集しない） |
| `user_config.h` | **あなたの設定**。雛形をコピーして作る。Claude Code の「環境を作って」で自動で作られる |
| `logo.png` | 起動ロゴにしたい画像（任意） |
| `generated/` | `logo.png` を機種ごとに変換したもの（自動で作られる。手で編集しない） |
| `logo_preview_lcd21.png` / `logo_preview_m5dial.png` | 変換したロゴを丸い画面に置いた見え方 |

やり方は [docs/user/05_customize.md](../docs/user/05_customize.md)（表示）と
[docs/user/06_logo.md](../docs/user/06_logo.md)（ロゴ）を読んでください。
Claude Code に「針の色を青にして」のように頼めば、ここのファイルを書き換えて書き込みまで行います。

元に戻したいときは `user_config.h` と `generated/` フォルダを削除すると、作者の既定の表示に戻ります。
