// LVGL 9.2.2 の設定（DOC-40 §1.1 でバージョン固定）
//
// 必要な項目だけを書く。書かなかった項目は lv_conf_internal.h の既定値になる。
// ビルドフラグ -DLV_CONF_INCLUDE_SIMPLE により、このファイルが include パスから読まれる。
//
// 方針（DOC-21 §2.4 / DEC-05）:
//   - 描画バッファと LVGL ヒープは内蔵 SRAM に置く。PSRAM は RGB パネルのフレームバッファ専用
//   - LVGL の API を呼ぶのは UI タスク 1 本だけ（OS 抽象は使わない）。他のタスクは SignalStore 経由
//   - 数字フォントは tools/make_font.py が生成する（assets/lcd21/fonts）。内蔵は小さい字だけ
#if 1 /* このヘッダを有効にする */

#ifndef LV_CONF_H
#define LV_CONF_H

#define LV_COLOR_DEPTH 16

// ---- メモリ（動的確保は LVGL 内部のみ。CLAUDE.md 規則 5）
#define LV_USE_STDLIB_MALLOC  LV_STDLIB_BUILTIN
#define LV_USE_STDLIB_STRING  LV_STDLIB_CLIB
#define LV_USE_STDLIB_SPRINTF LV_STDLIB_CLIB
#define LV_MEM_SIZE           (64 * 1024U)  // 内蔵 SRAM の .bss。使用量は起動ログで確認する（SYS-62）

// ---- 動作
#define LV_DEF_REFR_PERIOD 16  // [ms] 約 60 fps が上限。実際は描画量が律速（OPN-12）
#define LV_DPI_DEF         160
#define LV_USE_OS          LV_OS_NONE

// ---- 描画: ソフトウェアのみ。RGB565 で完結させる
#define LV_USE_DRAW_SW            1
#define LV_DRAW_SW_COMPLEX        1  // 円弧・線の anti-alias に必要
#define LV_DRAW_SW_SUPPORT_RGB565 1
#define LV_USE_DRAW_SW_ASM        LV_DRAW_SW_ASM_NONE
#define LV_DRAW_BUF_ALIGN         4

// ---- ログ（UART0 へ。出力先は main で登録する）
#define LV_USE_LOG         1
#define LV_LOG_LEVEL       LV_LOG_LEVEL_WARN
#define LV_LOG_PRINTF      0
#define LV_LOG_USE_TIMESTAMP 0

// ---- フォント（内蔵は小さい字だけ。大きい数字は生成フォント）
#define LV_FONT_MONTSERRAT_14 1  // LVGL 内部の既定フォントが要求する
#define LV_FONT_MONTSERRAT_24 1
#define LV_FONT_MONTSERRAT_32 1
#define LV_FONT_DEFAULT       &lv_font_montserrat_24

// ---- 使わない機能
#define LV_USE_PERF_MONITOR 0
#define LV_USE_SYSMON       0
#define LV_USE_DEMO_WIDGETS 0
#define LV_USE_THEME_DEFAULT 0  // 配色は theme.h で明示する。テーマのスタイル解決コストとメモリを払わない

#endif /* LV_CONF_H */
#endif /* 有効化 */
