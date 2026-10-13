#!/usr/bin/env bash
# SPL.2 / DOC-40 §4.2: ビルド済みのファームウェアを、Release に添付する名前で 1 つのフォルダに集める（CI の build ジョブが使う）
#
#   bash tools/collect_firmware.sh <出力フォルダ> <環境名...>
#   例: bash tools/collect_firmware.sh dist lcd21 lcd21_sim lcd21_1m
#
# 環境ごとに <環境名>-<元の名前> とする。Release はファイル名だけで添付するので、名前が重なると上書きされるため。
#   firmware.factory.bin : ブートローダ・パーティション込みの完成イメージ。アドレス 0x0 に書く（pioarduino の環境だけ作られる）
#   bootloader.bin / partitions.bin / firmware.bin : 個別に書く場合（0x0 / 0x8000 / 0x10000）
#   firmware.elf : 落ちたときのスタックトレースの解析用
set -euo pipefail

out="$1"
shift
mkdir -p "$out"
for env in "$@"; do
    dir=".pio/build/$env"
    if [ ! -f "$dir/firmware.bin" ]; then
        echo "::error::$dir/firmware.bin がありません（ビルドに失敗したか、環境名が違います）"
        exit 1
    fi
    for f in firmware.factory.bin bootloader.bin partitions.bin firmware.bin firmware.elf; do
        if [ -f "$dir/$f" ]; then
            cp "$dir/$f" "$out/$env-$f"
        fi
    done
done
ls -l "$out"
