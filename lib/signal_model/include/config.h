// SWA-06 / SWD-04: 設定構造体
//
// HW 非依存（DEC-06 / SWR-10）。NVS への保存は SWA-09 NvsStore が行う。
#pragma once

#include <cstdint>
#include "units.h"

namespace cm {

/// 構造体レイアウトを変えたら必ずインクリメントすること。
/// 不一致の保存データは既定値で上書きされる（SWR-81）。
constexpr uint16_t kConfigVersion = 1;

struct Config {
    uint16_t version = kConfigVersion;

    // ---- 表示 ----
    bool showAfr = true;             ///< true=AFR, false=λ (SYS-14)
    float stoich = kStoichGasoline;  ///< 5.0 - 20.0
    float ringLo = 0.68f;            ///< リング下端 λ (AFR 10.0 相当)
    float ringHi = 1.36f;            ///< リング上端 λ (AFR 20.0 相当)

    LambdaZoneConfig zones{0.75f, 0.85f, 1.03f, 1.10f};  ///< DOC-23 §3.2
    EgtConfig egt{850, 920};                             ///< DOC-23 §4

    uint8_t brightness = 4;     ///< 1-5 (DOC-23 §10)
    bool buzzerEnabled = true;  ///< EGT 危険時のブザー
    uint8_t lastPage   = 0;     ///< SYS-35

    // ---- CAN ----
    uint16_t canBaseId      = 0x200;  ///< SYS-02
    uint16_t canBitrateKbps = 500;    ///< 250 / 500 / 1000
    bool canExtendedId      = false;  ///< rusefiVerbose29b

    uint32_t crc32 = 0;  ///< 上記全フィールドのチェックサム（本フィールドは対象外）
};

Config defaultConfig();
uint32_t computeCrc(const Config& c);

/// 範囲・単調性・CRC を検証する。
/// @param checkCrc false にすると CRC を検査しない（メニューでの編集中の検証に使う）
bool validate(const Config& c, bool checkCrc = true);

/// ゾーン境界とリング範囲の単調性のみを検証する（SWD-04）。
///   ringLo < richHeavyMax < richMax < optimalMax < leanMax < ringHi
bool validateMonotonic(const Config& c);

}  // namespace cm
