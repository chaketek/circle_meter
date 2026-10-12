// SWD-14: 配布先ユーザーの設定（user/user_config.h）を Config へ重ねる
//
// HW 非依存（DEC-06）。user_config.h そのものはここでは読まない。値は include/user_overrides.h が
// UserConfig に詰めて渡す。単体テストが利用者の設定に左右されないようにするため。
#pragma once

#include <cstdint>

#include "config.h"

namespace cm {

/// 指定されたときだけ set = true になる値
template <typename T>
struct UserValue {
    bool set = false;
    T value{};
};

struct UserConfig {
    UserValue<bool> showAfr;
    UserValue<uint8_t> dialStyle;
    UserValue<uint8_t> brightness;
    UserValue<float> stoich;
    // λ の色境界。単調性（SWD-04）があるので 4 つを 1 組で扱う
    UserValue<float> richHeavyMax;
    UserValue<float> richMax;
    UserValue<float> optimalMax;
    UserValue<float> leanMax;
    // EGT 閾値。warn < danger の関係があるので 2 つを 1 組で扱う
    UserValue<uint16_t> egtWarnC;
    UserValue<uint16_t> egtDangerC;
    UserValue<uint16_t> canBitrateKbps;
    UserValue<uint16_t> canBaseId;
};

/// 反映・却下の単位。ビット位置として使う
enum class UserGroup : uint8_t {
    ShowAfr,
    DialStyle,
    Brightness,
    Stoich,
    Zones,
    Egt,
    CanBitrate,
    CanBaseId,
    Count
};

struct UserConfigResult {
    uint16_t appliedMask  = 0;  ///< 反映した組（1 << UserGroup）
    uint16_t rejectedMask = 0;  ///< 範囲外で既定値のまま残した組
};

/// 組ごとに cfg へ書き込み、validate(cfg, false) が通らなければその組だけ元に戻す（SWR-67）。
/// 呼ぶ前の cfg が validate() を満たしていれば、呼んだ後も満たす（CRC は取り直す）。
UserConfigResult applyUserConfig(Config& cfg, const UserConfig& user);

/// ログ用の組の名前（英小文字、"zones" など）
const char* userGroupName(UserGroup g);

}  // namespace cm
