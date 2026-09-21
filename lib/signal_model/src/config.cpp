// SWA-06 / SWD-04 実装
#include "config.h"

#include <cstring>

namespace cm {
namespace {

/// CRC-32 (IEEE 802.3, reflected)。テーブルを持たないビット単位実装。
/// 設定の保存・読み出し時に数回しか呼ばれないため速度は問題にならない。
uint32_t crc32Update(uint32_t crc, const uint8_t* data, size_t len) {
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int b = 0; b < 8; ++b) {
            crc = (crc >> 1) ^ (0xEDB88320u & (~(crc & 1u) + 1u));
        }
    }
    return crc;
}

bool inRange(float v, float lo, float hi) {
    return v >= lo && v <= hi;
}

}  // namespace

Config defaultConfig() {
    Config c;  // メンバ初期化子が既定値そのもの
    c.crc32 = computeCrc(c);
    return c;
}

uint32_t computeCrc(const Config& c) {
    // crc32 フィールド自身を除く先頭部分を対象にする。
    // 構造体末尾に crc32 があることを前提とする（config.h のレイアウトを変えるときは注意）。
    const size_t len = offsetof(Config, crc32);
    return ~crc32Update(0xFFFFFFFFu, reinterpret_cast<const uint8_t*>(&c), len);
}

bool validateMonotonic(const Config& c) {
    return c.ringLo < c.zones.richHeavyMax && c.zones.richHeavyMax < c.zones.richMax &&
           c.zones.richMax < c.zones.optimalMax && c.zones.optimalMax < c.zones.leanMax &&
           c.zones.leanMax < c.ringHi;
}

bool validate(const Config& c, bool checkCrc) {
    if (c.version != kConfigVersion) return false;

    if (!inRange(c.stoich, 5.0f, 20.0f)) return false;
    if (!inRange(c.ringLo, 0.30f, 2.0f)) return false;
    if (!inRange(c.ringHi, 0.30f, 5.0f)) return false;
    if (!validateMonotonic(c)) return false;

    if (c.egt.warnC == 0 || c.egt.warnC >= c.egt.dangerC) return false;
    if (c.egt.dangerC > 1275) return false;

    if (c.brightness < 1 || c.brightness > 5) return false;

    if (c.canBaseId > 0x7FF - 11 && !c.canExtendedId) return false;
    if (c.canBitrateKbps != 250 && c.canBitrateKbps != 500 && c.canBitrateKbps != 1000) return false;

    if (checkCrc && c.crc32 != computeCrc(c)) return false;

    return true;
}

}  // namespace cm
