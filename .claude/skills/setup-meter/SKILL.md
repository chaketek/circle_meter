---
name: setup-meter
description: circle_meter（丸型の空燃比・排気温度メータ）のファームウェアを Windows でビルド・書き込みできるように、Git・Python・PlatformIO の確認と導入、初回ビルド（ツールチェーンの取得）、本体の COM ポート確認、ユーザー設定ファイル user/user_config.h の作成までを一通り行う。配布先ユーザーが「環境を作って」「セットアップして」「初めて使う」「ビルドできるようにして」と言ったときに使う。
---

# 環境を作る（配布先ユーザー向け）

相手は **Git も Claude Code もほとんど使ったことがない人**。手順書は `docs/user/03_get_code.md`。

## 守ること（この Skill の間ずっと）

- 専門用語は使わないか、使うなら一言で言い換える（`docs/user/glossary.md` の言い方に合わせる）。
- 1 回の返事で頼む操作は 1 つだけにする。結果を確かめてから次へ進む。
- **ビルドと書き込みは PowerShell で実行する**（Git Bash では `idf_tools.py` が `MSys/Mingw is not supported` で失敗する）。
- 書き換えてよいのは `user/` フォルダだけ。インストールはユーザーの承認を得てから行う。
- 本体を CAN に繋いでいても、PC から CAN へ何かを送る操作はしない。

## 手順

### 1. 今の状態を調べる（読むだけ）

PowerShell で次を確かめ、結果を表にしてユーザーに見せる。

| 項目 | 確認 | 合格 |
|---|---|---|
| フォルダ | `git rev-parse --show-toplevel` と `Test-Path platformio.ini` | circle_meter のフォルダで開いている |
| Git | `git --version` | 2.x |
| Python | `python --version` | **3.11 以上**。「Microsoft Store を開く」だけで終わる場合は未導入とみなす |
| PlatformIO | `python -m platformio --version` | 6.1 以上 |
| Python の部品 | `python -c "import serial, PIL, numpy"` | エラーが出ない |

circle_meter のフォルダで開いていなければ、ここで止めて `docs/user/03_get_code.md` の手順（clone してフォルダを開き直す）を案内する。

### 2. 足りないものを入れる（1 つずつ承認を得る）

| 足りないもの | コマンド | 注意 |
|---|---|---|
| Git | `winget install --id Git.Git -e --source winget` | Claude Code の動作にも必要なので、普通は入っている |
| Python | `winget install --id Python.Python.3.12 -e --source winget` | 入れたあとは **Claude Code を一度閉じて開き直す**必要がある（新しい PATH を読むため）。そう伝えて、開き直したら「続きをやって」と言ってもらう |
| PlatformIO と部品 | `python -m pip install --user --upgrade platformio pyserial pillow numpy` | 数分かかる |

`winget` が無い・失敗する場合は、公式サイトからの入れ方を案内する（ダウンロードはユーザー自身に行ってもらう）:
Python は python.org の Windows 用インストーラで、**「Add python.exe to PATH」にチェックを入れる**。

### 3. 設定ファイルを作る

`user/user_config.h` が無ければ `user/user_config.example.h` をコピーして作る（既にあれば触らない）。
「表示を変えたくなったら、このファイルを書き換えます。今は何も変えていないので作者の既定の表示です」と伝える。

### 4. 初回ビルド（10〜20 分）

PowerShell で、**バックグラウンド実行**にして待つ（初回はツールチェーンを 1 GB 以上ダウンロードする）。

```powershell
python -m platformio run -e lcd21_sim
```

待っている間に「初回だけ時間がかかります。2 回目からは 1〜2 分です」と伝える。
失敗したら出力の最後の 30 行を読み、`docs/user/08_troubleshooting.md` の「ビルドが失敗する」に沿って対処する。
推測で設定ファイルやコードを書き換えない。

### 5. 本体を繋いで COM ポートを確かめる

ユーザーに頼む: 「本体の **UART と書かれた USB-C** に、**データ通信できる** USB ケーブルで PC と繋いでください。
もう一方の USB-C（ネイティブ USB）には何も挿さないでください」。

```powershell
python -m platformio device list
```

`USB-Enhanced-SERIAL CH343`（または VID 1A86）の COM 番号が見えれば合格。見えなければ:

1. ケーブルを別のもの（データ通信対応）に替える
2. 挿す口が UART 側か確かめる
3. それでも出なければ、WCH 社の CH343 ドライバが要る（ユーザー自身にダウンロードしてもらう。`docs/user/08_troubleshooting.md`）

### 6. 終わったら

できたことを短くまとめ、次の一歩を 1 つだけ提案する: 「デモを書き込んで画面が動くか見てみましょうか？」（`flash-meter` Skill）。
