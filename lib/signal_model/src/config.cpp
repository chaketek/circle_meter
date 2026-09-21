// SWA-06 / SWD-04 実装
#include "config.h"

#include <cstddef>
#include <cstring>
#include <type_traits>

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

/// CRC を「構造体のメモリ像」ではなく「フィールドの列」に対して取るための補助。
///
/// 構造体をそのまま reinterpret_cast して CRC を取ると、**パディングバイトが混ざる**。
/// パディングの中身は不定であり、同じ設定値でもビルドや実行のたびに CRC が変わりうる。
/// それに気づかないまま NVS に保存すると「保存したのに次回起動で既定値に戻る」
/// という再現性の低い不具合になるため、フィールド単位で積む（SWD-04 / UT-13）。
class CrcAccumulator {
public:
    template <typename T>
    void add(const T& v) {
        static_assert(std::is_trivially_copyable<T>::value, "trivially copyable なフィールドのみ");
        m_crc = crc32Update(m_crc, reinterpret_cast<const uint8_t*>(&v), sizeof(T));
    }

    uint32_t value() const { return ~m_crc; }

private:
    uint32_t m_crc = 0xFFFFFFFFu;
};

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
    // crc32 自身は対象外。フィールドを追加したら必ずここにも追加すること
    // （追加を忘れると、そのフィールドを変えても CRC が変わらない）。
    CrcAccumulator a;
    a.add(c.version);
    a.add(c.showAfr);
    a.add(c.stoich);
    a.add(c.ringLo);
    a.add(c.ringHi);
    a.add(c.zones.richHeavyMax);
    a.add(c.zones.richMax);
    a.add(c.zones.optimalMax);
    a.add(c.zones.leanMax);
    a.add(c.egt.warnC);
    a.add(c.egt.dangerC);
    a.add(c.brightness);
    a.add(c.buzzerEnabled);
    a.add(c.lastPage);
    a.add(c.canBaseId);
    a.add(c.canBitrateKbps);
    a.add(c.canExtendedId);
    return a.value();
}

bool validateMonotonic(const Config& c) {
    return c.ringLo < c.zones.richHeavyMax && c.zones.richHeavyMax < c.zones.richMax &&
           c.zones.richMax < c.zones.optimalMax && c.zones.optimalMax < c.zones.leanMax &&
           c.zones.leanMax < c.ringHi;
}

bool validate(const Config& c, bool checkCrc) {
    if (c.version != kConfigVersion)
        return false;

    if (!inRange(c.stoich, 5.0f, 20.0f))
        return false;
    if (!inRange(c.ringLo, 0.30f, 2.0f))
        return false;
    if (!inRange(c.ringHi, 0.30f, 5.0f))
        return false;
    if (!validateMonotonic(c))
        return false;

    if (c.egt.warnC == 0 || c.egt.warnC >= c.egt.dangerC)
        return false;
    if (c.egt.dangerC > 1275)
        return false;

    if (c.brightness < 1 || c.brightness > 5)
        return false;

    if (c.canBaseId > 0x7FF - 11 && !c.canExtendedId)
        return false;
    if (c.canBitrateKbps != 250 && c.canBitrateKbps != 500 && c.canBitrateKbps != 1000)
        return false;

    if (checkCrc && c.crc32 != computeCrc(c))
        return false;

    return true;
}

}  // namespace cm
