// =====================================================================
//  circle_meter ユーザー設定（雛形）
// =====================================================================
//
//  使い方
//    1. このファイルを同じフォルダに「user_config.h」という名前でコピーする
//       （Claude Code の「環境を作って」でコピーされます）
//    2. user_config.h の中で、変えたい行の先頭の「// 」を消して値を書き換える
//    3. 書き込み直す（Claude Code に「書き込んで」と頼む）
//
//  ・「// 」が付いたままの行は使われず、作者の決めた既定値になります
//  ・範囲外の値を書くと、ビルドの時点で日本語のメッセージが出て止まります
//  ・元に戻したいときは、その行の先頭に「// 」を付け直します
//  ・このファイル（user_config.example.h）は作者の更新で書き換わります。直接編集しないでください
//
//  詳しい説明: docs/user/05_customize.md
// =====================================================================
#pragma once

// ---------------------------------------------------------------------
//  既定の表示
// ---------------------------------------------------------------------

// 主数値を AFR（空燃比、例 14.7）で出すなら 1、λ（例 1.00）で出すなら 0
// #define USER_SHOW_AFR 1

// 表示デザイン: USER_DIAL_NEEDLE_A（指針式）か USER_DIAL_RING（外周リング）
// #define USER_DIAL_STYLE USER_DIAL_NEEDLE_A

// 画面の明るさ: 1（暗い）〜 5（明るい）
// #define USER_BRIGHTNESS 4

// 理論空燃比（AFR 表示の換算に使う）: 5.0〜20.0。ガソリン 14.7 / E85 は 9.8 程度
// #define USER_STOICH 14.7

// ---------------------------------------------------------------------
//  λ の色の境目（λ の値。0.69〜1.35、左から順に大きくすること）
//    〜RICH_HEAVY_MAX: 濃すぎ（青）  〜RICH_MAX: 濃いめ（水色）
//    〜OPTIMAL_MAX: 適正（緑）       〜LEAN_MAX: 薄め（黄）   それより上: 薄すぎ（赤）
// ---------------------------------------------------------------------
// #define USER_LAMBDA_RICH_HEAVY_MAX 0.75
// #define USER_LAMBDA_RICH_MAX       0.85
// #define USER_LAMBDA_OPTIMAL_MAX    1.03
// #define USER_LAMBDA_LEAN_MAX       1.10

// ---------------------------------------------------------------------
//  排気温度（EGT）の警告（°C。警告 < 危険 にすること。1〜1275）
//    警告を超えると黄色、危険を超えると赤色になり外周が点滅します
// ---------------------------------------------------------------------
// #define USER_EGT_WARN_C   850
// #define USER_EGT_DANGER_C 920

// ---------------------------------------------------------------------
//  色（0xRRGGBB の 16 進数。例: 赤 0xFF0000 / 緑 0x00FF00 / 青 0x0000FF / 白 0xFFFFFF）
//  ※ 警告・危険の色は、走行中に目の端で気づけるよう明るく目立つ色にしてください
// ---------------------------------------------------------------------

// λ ゾーンの色（リングの色・主数値の色）
// #define USER_COLOR_RICH_HEAVY 0x2E7DFF   // 濃すぎ（青）
// #define USER_COLOR_RICH       0x00C8D7   // 濃いめ（水色）
// #define USER_COLOR_OPTIMAL    0x17D14B   // 適正（緑）
// #define USER_COLOR_LEAN       0xFFC400   // 薄め（黄）
// #define USER_COLOR_LEAN_HEAVY 0xFF2D2D   // 薄すぎ（赤）

// 排気温度の色
// #define USER_COLOR_EGT_NORMAL 0xE0E0E0   // 通常
// #define USER_COLOR_WARN       0xFFC400   // 警告
// #define USER_COLOR_DANGER     0xFF2D2D   // 危険（NO SIGNAL などの赤い文字もこの色）

// 文字・背景（主にリングのデザイン）
// #define USER_COLOR_TEXT       0xFFFFFF   // 主な文字
// #define USER_COLOR_TEXT_SUB   0x8A8A8A   // 補助の文字
// #define USER_COLOR_BG         0x000000   // 背景
// #define USER_COLOR_TRACK      0x1A1A1A   // リングの未到達部分

// 指針式デザインの盤面
// #define USER_COLOR_DIAL_BG     0x060606  // 盤面
// #define USER_COLOR_NEEDLE      0xFF7014  // 針
// #define USER_COLOR_DIAL_RIM    0x787878  // 外周の線
// #define USER_COLOR_DIAL_TICK   0xEBEBEB  // 目盛り
// #define USER_COLOR_DIAL_NUMBER 0xF0F0F0  // 目盛りの数字
// #define USER_COLOR_DIAL_LABEL  0xC8C8C8  // 「A/F」の文字
// #define USER_COLOR_DIAL_SMALL  0x8C8C8C  // 「AIR FUEL RATIO」の文字

// ---------------------------------------------------------------------
//  CAN（車両の ECU の設定に合わせる。違うと何も表示されません）
// ---------------------------------------------------------------------

// 通信速度（kbps）: 250 / 500 / 1000。rusEFI の既定は 500
// #define USER_CAN_BITRATE_KBPS 500

// rusEFI の verbose broadcast のベース ID（TunerStudio の verboseCanBaseAddress）。既定 0x200
// #define USER_CAN_BASE_ID 0x200

// 起動ロゴは user フォルダに logo.png を置いて変換します（docs/user/06_logo.md）
