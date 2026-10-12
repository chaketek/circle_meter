// SWD-14: user/user_config.h（配布先ユーザーの設定）を取り込む
//
// user_config.h が無ければ何も定義されず、仕様の既定値で動く（SWR-67）。
// 範囲の検査はここの static_assert で行い、ビルドの時点で日本語のメッセージで止める。
// 組をまたぐ関係（λ の色境界の単調性など）は起動時の applyUserConfig() が既定値と合わせて検査する。
#pragma once

#include "user_config_apply.h"

// USER_DIAL_STYLE に書く値。user_config.h より先に定義しておく
#define USER_DIAL_RING     0
#define USER_DIAL_NEEDLE_A 1

#if __has_include("user_config.h")
#include "user_config.h"
#define CM_HAS_USER_CONFIG 1
#else
#define CM_HAS_USER_CONFIG 0
#endif

// ---------------------------------------------------------------- ビルド時の検査
#ifdef USER_SHOW_AFR
static_assert(USER_SHOW_AFR == 0 || USER_SHOW_AFR == 1,
              "USER_SHOW_AFR は 1（AFR）か 0（λ）で指定してください");
#endif
#ifdef USER_DIAL_STYLE
static_assert(USER_DIAL_STYLE == USER_DIAL_RING || USER_DIAL_STYLE == USER_DIAL_NEEDLE_A,
              "USER_DIAL_STYLE は USER_DIAL_NEEDLE_A か USER_DIAL_RING で指定してください");
#endif
#ifdef USER_BRIGHTNESS
static_assert(USER_BRIGHTNESS >= 1 && USER_BRIGHTNESS <= 5, "USER_BRIGHTNESS は 1〜5 で指定してください");
#endif
#ifdef USER_STOICH
static_assert(USER_STOICH >= 5.0 && USER_STOICH <= 20.0,
              "USER_STOICH（理論空燃比）は 5.0〜20.0 で指定してください");
#endif

// λ の色境界はリングの範囲（0.68〜1.36）の内側に収める（SWD-04 の単調性）
#define CM_USER_LAMBDA_OK(v) ((v) > 0.68 && (v) < 1.36)
#ifdef USER_LAMBDA_RICH_HEAVY_MAX
static_assert(CM_USER_LAMBDA_OK(USER_LAMBDA_RICH_HEAVY_MAX),
              "USER_LAMBDA_RICH_HEAVY_MAX は 0.69〜1.35 で指定してください");
#endif
#ifdef USER_LAMBDA_RICH_MAX
static_assert(CM_USER_LAMBDA_OK(USER_LAMBDA_RICH_MAX),
              "USER_LAMBDA_RICH_MAX は 0.69〜1.35 で指定してください");
#endif
#ifdef USER_LAMBDA_OPTIMAL_MAX
static_assert(CM_USER_LAMBDA_OK(USER_LAMBDA_OPTIMAL_MAX),
              "USER_LAMBDA_OPTIMAL_MAX は 0.69〜1.35 で指定してください");
#endif
#ifdef USER_LAMBDA_LEAN_MAX
static_assert(CM_USER_LAMBDA_OK(USER_LAMBDA_LEAN_MAX),
              "USER_LAMBDA_LEAN_MAX は 0.69〜1.35 で指定してください");
#endif
#if defined(USER_LAMBDA_RICH_HEAVY_MAX) && defined(USER_LAMBDA_RICH_MAX)
static_assert(USER_LAMBDA_RICH_HEAVY_MAX < USER_LAMBDA_RICH_MAX,
              "λ の色境界は RICH_HEAVY_MAX < RICH_MAX < OPTIMAL_MAX < LEAN_MAX の順にしてください");
#endif
#if defined(USER_LAMBDA_RICH_MAX) && defined(USER_LAMBDA_OPTIMAL_MAX)
static_assert(USER_LAMBDA_RICH_MAX < USER_LAMBDA_OPTIMAL_MAX,
              "λ の色境界は RICH_HEAVY_MAX < RICH_MAX < OPTIMAL_MAX < LEAN_MAX の順にしてください");
#endif
#if defined(USER_LAMBDA_OPTIMAL_MAX) && defined(USER_LAMBDA_LEAN_MAX)
static_assert(USER_LAMBDA_OPTIMAL_MAX < USER_LAMBDA_LEAN_MAX,
              "λ の色境界は RICH_HEAVY_MAX < RICH_MAX < OPTIMAL_MAX < LEAN_MAX の順にしてください");
#endif

#ifdef USER_EGT_WARN_C
static_assert(USER_EGT_WARN_C >= 1 && USER_EGT_WARN_C <= 1275,
              "USER_EGT_WARN_C は 1〜1275（°C）で指定してください");
#endif
#ifdef USER_EGT_DANGER_C
static_assert(USER_EGT_DANGER_C >= 2 && USER_EGT_DANGER_C <= 1275,
              "USER_EGT_DANGER_C は 2〜1275（°C）で指定してください");
#endif
#if defined(USER_EGT_WARN_C) && defined(USER_EGT_DANGER_C)
static_assert(USER_EGT_WARN_C < USER_EGT_DANGER_C,
              "USER_EGT_WARN_C（警告）は USER_EGT_DANGER_C（危険）より低くしてください");
#endif

#ifdef USER_CAN_BITRATE_KBPS
static_assert(USER_CAN_BITRATE_KBPS == 250 || USER_CAN_BITRATE_KBPS == 500 || USER_CAN_BITRATE_KBPS == 1000,
              "USER_CAN_BITRATE_KBPS は 250 / 500 / 1000 のどれかで指定してください");
#endif
#ifdef USER_CAN_BASE_ID
static_assert(USER_CAN_BASE_ID >= 0 && USER_CAN_BASE_ID <= 0x7F4,
              "USER_CAN_BASE_ID は 0x000〜0x7F4 で指定してください（rusEFI の既定は 0x200）");
#endif

// 色は 0xRRGGBB。theme.h が USER_COLOR_* を読む（DOC-23 §8.1）
#define CM_USER_COLOR_OK(c) ((c) >= 0 && (c) <= 0xFFFFFF)
#define CM_USER_COLOR_CHECK(name) \
    static_assert(CM_USER_COLOR_OK(name), #name " は 0x000000〜0xFFFFFF で指定してください")
#ifdef USER_COLOR_BG
CM_USER_COLOR_CHECK(USER_COLOR_BG);
#endif
#ifdef USER_COLOR_TRACK
CM_USER_COLOR_CHECK(USER_COLOR_TRACK);
#endif
#ifdef USER_COLOR_TEXT
CM_USER_COLOR_CHECK(USER_COLOR_TEXT);
#endif
#ifdef USER_COLOR_TEXT_SUB
CM_USER_COLOR_CHECK(USER_COLOR_TEXT_SUB);
#endif
#ifdef USER_COLOR_RICH_HEAVY
CM_USER_COLOR_CHECK(USER_COLOR_RICH_HEAVY);
#endif
#ifdef USER_COLOR_RICH
CM_USER_COLOR_CHECK(USER_COLOR_RICH);
#endif
#ifdef USER_COLOR_OPTIMAL
CM_USER_COLOR_CHECK(USER_COLOR_OPTIMAL);
#endif
#ifdef USER_COLOR_LEAN
CM_USER_COLOR_CHECK(USER_COLOR_LEAN);
#endif
#ifdef USER_COLOR_LEAN_HEAVY
CM_USER_COLOR_CHECK(USER_COLOR_LEAN_HEAVY);
#endif
#ifdef USER_COLOR_WARN
CM_USER_COLOR_CHECK(USER_COLOR_WARN);
#endif
#ifdef USER_COLOR_DANGER
CM_USER_COLOR_CHECK(USER_COLOR_DANGER);
#endif
#ifdef USER_COLOR_EGT_NORMAL
CM_USER_COLOR_CHECK(USER_COLOR_EGT_NORMAL);
#endif
#ifdef USER_COLOR_DIAL_BG
CM_USER_COLOR_CHECK(USER_COLOR_DIAL_BG);
#endif
#ifdef USER_COLOR_NEEDLE
CM_USER_COLOR_CHECK(USER_COLOR_NEEDLE);
#endif
#ifdef USER_COLOR_DIAL_RIM
CM_USER_COLOR_CHECK(USER_COLOR_DIAL_RIM);
#endif
#ifdef USER_COLOR_DIAL_TICK
CM_USER_COLOR_CHECK(USER_COLOR_DIAL_TICK);
#endif
#ifdef USER_COLOR_DIAL_NUMBER
CM_USER_COLOR_CHECK(USER_COLOR_DIAL_NUMBER);
#endif
#ifdef USER_COLOR_DIAL_LABEL
CM_USER_COLOR_CHECK(USER_COLOR_DIAL_LABEL);
#endif
#ifdef USER_COLOR_DIAL_SMALL
CM_USER_COLOR_CHECK(USER_COLOR_DIAL_SMALL);
#endif

namespace cm {

/// user_config.h の USER_* から UserConfig を作る（main が起動時に applyUserConfig() へ渡す）
inline UserConfig userConfigFromBuild() {
    UserConfig u;
#ifdef USER_SHOW_AFR
    u.showAfr = {true, USER_SHOW_AFR != 0};
#endif
#ifdef USER_DIAL_STYLE
    u.dialStyle = {true, static_cast<uint8_t>(USER_DIAL_STYLE)};
#endif
#ifdef USER_BRIGHTNESS
    u.brightness = {true, static_cast<uint8_t>(USER_BRIGHTNESS)};
#endif
#ifdef USER_STOICH
    u.stoich = {true, static_cast<float>(USER_STOICH)};
#endif
#ifdef USER_LAMBDA_RICH_HEAVY_MAX
    u.richHeavyMax = {true, static_cast<float>(USER_LAMBDA_RICH_HEAVY_MAX)};
#endif
#ifdef USER_LAMBDA_RICH_MAX
    u.richMax = {true, static_cast<float>(USER_LAMBDA_RICH_MAX)};
#endif
#ifdef USER_LAMBDA_OPTIMAL_MAX
    u.optimalMax = {true, static_cast<float>(USER_LAMBDA_OPTIMAL_MAX)};
#endif
#ifdef USER_LAMBDA_LEAN_MAX
    u.leanMax = {true, static_cast<float>(USER_LAMBDA_LEAN_MAX)};
#endif
#ifdef USER_EGT_WARN_C
    u.egtWarnC = {true, static_cast<uint16_t>(USER_EGT_WARN_C)};
#endif
#ifdef USER_EGT_DANGER_C
    u.egtDangerC = {true, static_cast<uint16_t>(USER_EGT_DANGER_C)};
#endif
#ifdef USER_CAN_BITRATE_KBPS
    u.canBitrateKbps = {true, static_cast<uint16_t>(USER_CAN_BITRATE_KBPS)};
#endif
#ifdef USER_CAN_BASE_ID
    u.canBaseId = {true, static_cast<uint16_t>(USER_CAN_BASE_ID)};
#endif
    return u;
}

}  // namespace cm
