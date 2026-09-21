#!/usr/bin/env bash
# DOC-41 §3.1: 安全・依存分離の静的チェック（CI の guard ジョブの実体）
#
# CI とローカルで同じ検査を走らせるためにスクリプト化している。
#   bash tools/guard.sh
#
# 【コメントを除外する理由】
# 「twai_transmit を使ってはならない」のような禁止事項は、コード中のコメントに
# 書いてこそ効果がある。単純な grep だとその注意書き自体に反応して失敗するため、
# 行コメント（行頭が // * /*）を除外してから判定する。
# 行末コメントは除外しない（安全側に倒す）。
set -uo pipefail

fail=0

# grep -rn の出力 "path:line:content" のうち、content が行コメントで始まるものを落とす
drop_comment_lines() {
    grep -vE '^[^:]+:[0-9]+:[[:space:]]*(//|\*|/\*)' || true
}

# check <ID> <説明> <正規表現> <エラーメッセージ> <対象パス...>
check() {
    local id="$1" desc="$2" re="$3" msg="$4"
    shift 4
    local hits
    hits=$(grep -rnE "$re" "$@" 2>/dev/null | drop_comment_lines)
    if [ -n "$hits" ]; then
        echo "$hits"
        echo "::error::[$id] $msg"
        fail=1
    else
        echo "  OK  $id  $desc"
    fi
}

echo "== guard (DOC-41 §3.1) =="

check S2 "CAN 送信 API の混入禁止 (RSK-06 / SYS-07)" \
    'twai_transmit' \
    "CAN 送信 API が含まれています。本機はフレームを送出しません (SYS-07 / RSK-06)" \
    src lib

check S3 "自己テストモード(NO_ACK)の指定禁止 (RSK-10)" \
    'TWAI_MODE_NO_ACK' \
    "TWAI_MODE_NO_ACK は ACK を返さず RSK-10 を再発させます (DEC-04)" \
    src lib

check S4 "ドメイン層の HW 非依存 (DEC-06 / SWR-10)" \
    '#include[[:space:]]*[<"](Arduino\.h|lvgl\.h|driver/|esp_|freertos/)' \
    "ドメイン層 (rusefi_can / signal_model) は HW 非依存でなければなりません" \
    lib/rusefi_can lib/signal_model

check S5 "CAN スケーリング定数の直書き禁止 (SWD-01)" \
    '0\.0001f|0\.0333|1275\.0f|\* 5\.0f' \
    "スケーリング値は rusefi_can_spec.h の定数を使ってください" \
    lib/rusefi_can/src

if [ "$fail" -ne 0 ]; then
    echo "== guard: FAILED =="
    exit 1
fi
echo "== guard: OK =="
