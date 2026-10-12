// SWR-67 / SWR-68: ユーザー設定が効いたかを起動ログに出す（LCD-2.1 版・M5Dial 版で共通）
//
// tools/meter_log.py と手順書（docs/user/）がこの行を読むので、形式を変えるときはそちらも直す。
#pragma once

#include <Arduino.h>

#include "user_overrides.h"

namespace cm {

inline void printUserGroups(const char* label, uint16_t mask) {
    Serial.printf("  %s:", label);
    if (mask == 0) {
        Serial.print(" (none)");
    }
    for (uint8_t g = 0; g < static_cast<uint8_t>(UserGroup::Count); ++g) {
        if (mask & (1u << g)) {
            Serial.printf(" %s", userGroupName(static_cast<UserGroup>(g)));
        }
    }
    Serial.println();
}

inline void printUserConfig(const UserConfigResult& r) {
    Serial.printf("user config: %s\n",
                  CM_HAS_USER_CONFIG ? "present (user/user_config.h)" : "none (defaults)");
    if (!CM_HAS_USER_CONFIG) {
        return;
    }
    printUserGroups("applied", r.appliedMask);
    if (r.rejectedMask != 0) {
        // static_assert をすり抜けた組（既定値との組み合わせで単調性が崩れたなど）。既定値のまま動く
        printUserGroups("REJECTED (kept defaults)", r.rejectedMask);
    }
}

}  // namespace cm
