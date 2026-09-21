// SWE.4 ユニット検証: rusEFI CAN デコーダ
// UT-01, UT-02, UT-03, UT-04, UT-10  (docs/30_test_strategy.md §2)
#include <unity.h>

#include "rusefi_decoder.h"
#include "signal_id.h"

using namespace cm;
using namespace cm::rusefi;

namespace {

constexpr uint32_t kBase = kDefaultBaseId;  // 0x200

/// デコード結果から特定の信号を取り出す。見つからなければ false。
bool find(const DecodeResult& r, SignalId id, float& out) {
    for (uint8_t i = 0; i < r.count; ++i) {
        if (r.signals[i].id == id) {
            out = r.signals[i].value;
            return true;
        }
    }
    return false;
}

void zero(uint8_t* d) {
    for (int i = 0; i < 8; ++i)
        d[i] = 0;
}

}  // namespace

void setUp() {}
void tearDown() {}

// ---------------------------------------------------------------- UT-01
// SWR-04 / SWR-05 / SYS-02: ベース ID からのオフセット解釈
void test_UT01_base_id_offset() {
    uint8_t d[8];
    zero(d);
    d[0] = 0x10;
    d[1] = 0x27;  // λ = 1.0000

    // 既定ベース 0x200 では 0x207 が Fueling3
    DecodeResult r = decodeFrame(kBase + kOffFueling3, d, 8, kBase);
    TEST_ASSERT_TRUE(r.accepted);
    float lam = 0.0f;
    TEST_ASSERT_TRUE(find(r, SignalId::Lambda1, lam));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, lam);

    // ベースを 0x300 に変えると 0x307 が Fueling3 になる
    r = decodeFrame(0x307, d, 8, 0x300);
    TEST_ASSERT_TRUE(r.accepted);
    TEST_ASSERT_TRUE(find(r, SignalId::Lambda1, lam));

    // 同じフレームでもベースが違えば受理されない
    r = decodeFrame(0x207, d, 8, 0x300);
    TEST_ASSERT_FALSE(r.accepted);
    TEST_ASSERT_EQUAL_UINT8(0, r.count);
}

void test_UT01_out_of_range_id_rejected() {
    uint8_t d[8];
    zero(d);

    TEST_ASSERT_FALSE(decodeFrame(0x100, d, 8, kBase).accepted);  // ベース未満
    TEST_ASSERT_FALSE(decodeFrame(0x1FF, d, 8, kBase).accepted);  // 境界の 1 つ下
    TEST_ASSERT_TRUE(decodeFrame(0x200, d, 8, kBase).accepted);   // 下端
    TEST_ASSERT_TRUE(decodeFrame(0x20B, d, 8, kBase).accepted);   // 上端 (BASE+11)
    TEST_ASSERT_FALSE(decodeFrame(0x20C, d, 8, kBase).accepted);  // 上端の 1 つ上
    TEST_ASSERT_FALSE(decodeFrame(0x7E8, d, 8, kBase).accepted);  // OBD-II 応答は無視
}

void test_UT01_extended_id_uses_same_arithmetic() {
    // 29bit 拡張 ID でも「生の ID - baseId」で同じ解釈になる (SWR-05)
    uint8_t d[8];
    zero(d);
    d[0] = 0xA9;  // EGT1 = 845 degC

    const uint32_t extBase = 0x1FFFF000;
    DecodeResult r         = decodeFrame(extBase + kOffEgts, d, 8, extBase);
    TEST_ASSERT_TRUE(r.accepted);
    float egt = 0.0f;
    TEST_ASSERT_TRUE(find(r, SignalId::Egt1, egt));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 845.0f, egt);
}

// ---------------------------------------------------------------- UT-02
// SWR-07 / SYS-03: λ デコード
void test_UT02_lambda_decode() {
    uint8_t d[8];
    zero(d);

    // raw 10000 (0x2710) -> λ 1.0000
    d[0]      = 0x10;
    d[1]      = 0x27;
    float lam = 0.0f;
    TEST_ASSERT_TRUE(find(decodeFrame(0x207, d, 8, kBase), SignalId::Lambda1, lam));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 1.0000f, lam);

    // raw 8500 (0x2134) -> λ 0.8500
    d[0] = 0x34;
    d[1] = 0x21;
    TEST_ASSERT_TRUE(find(decodeFrame(0x207, d, 8, kBase), SignalId::Lambda1, lam));
    TEST_ASSERT_FLOAT_WITHIN(1e-5f, 0.8500f, lam);
}

void test_UT02_lambda_is_little_endian() {
    // バイト順を取り違えていれば必ず落ちるケース
    uint8_t d[8];
    zero(d);
    d[0]      = 0x00;
    d[1]      = 0x27;  // LE: 0x2700 = 9984 -> 0.9984  /  BE なら 0x0027 = 39 -> 0.0039
    float lam = 0.0f;
    TEST_ASSERT_TRUE(find(decodeFrame(0x207, d, 8, kBase), SignalId::Lambda1, lam));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.9984f, lam);
}

void test_UT02_lambda_validity() {
    // SYS-42: rusEFI は未構成センサに 0 を送る。表示してはならない。
    TEST_ASSERT_FALSE(isLambdaValid(0.0f));
    TEST_ASSERT_FALSE(isLambdaValid(0.2999f));
    TEST_ASSERT_TRUE(isLambdaValid(0.30f));
    TEST_ASSERT_TRUE(isLambdaValid(1.0f));
    TEST_ASSERT_TRUE(isLambdaValid(5.0f));
    TEST_ASSERT_FALSE(isLambdaValid(5.01f));
}

// ---------------------------------------------------------------- UT-03
// SWR-08 / SYS-04: EGT デコード
void test_UT03_egt_decode() {
    uint8_t d[8];
    zero(d);

    d[0]           = 0xA9;  // 169 * 5 = 845
    d[1]           = 0x64;  // 100 * 5 = 500
    float egt      = 0.0f;
    DecodeResult r = decodeFrame(0x209, d, 8, kBase);
    TEST_ASSERT_TRUE(find(r, SignalId::Egt1, egt));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 845.0f, egt);
    TEST_ASSERT_TRUE(find(r, SignalId::Egt2, egt));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 500.0f, egt);

    d[0] = 0xFF;  // 上限 255 * 5 = 1275
    TEST_ASSERT_TRUE(find(decodeFrame(0x209, d, 8, kBase), SignalId::Egt1, egt));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 1275.0f, egt);
}

void test_UT03_egt_validity() {
    // RSK-09: EGT 未構成時の 0 degC を「冷えている」と誤読させない
    TEST_ASSERT_FALSE(isEgtValid(0.0f));
    TEST_ASSERT_TRUE(isEgtValid(5.0f));
    TEST_ASSERT_TRUE(isEgtValid(1275.0f));
    TEST_ASSERT_FALSE(isEgtValid(1275.1f));
}

// ---------------------------------------------------------------- UT-04
// SWR-09 / SYS-05: 副信号のデコード
void test_UT04_speeds_frame() {
    uint8_t d[8];
    zero(d);
    d[0] = 0xB8;
    d[1] = 0x0B;  // RPM = 3000
    d[2] = 0x2C;
    d[3] = 0x01;  // timing = 300 * 0.02 = 6.0 deg
    d[4] = 0x14;  // injDuty = 20 * 0.5 = 10 %
    d[6] = 0x3C;  // vss = 60 km/h

    DecodeResult r = decodeFrame(0x201, d, 8, kBase);
    float v        = 0.0f;
    TEST_ASSERT_TRUE(find(r, SignalId::Rpm, v));
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 3000.0f, v);
    TEST_ASSERT_TRUE(find(r, SignalId::IgnitionTiming, v));
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 6.0f, v);
    TEST_ASSERT_TRUE(find(r, SignalId::InjDuty, v));
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 10.0f, v);
    TEST_ASSERT_TRUE(find(r, SignalId::VehicleSpeed, v));
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 60.0f, v);
}

void test_UT04_ignition_timing_is_signed() {
    uint8_t d[8];
    zero(d);
    // -500 * 0.02 = -10.0 deg  (0xFE0C)
    d[2]    = 0x0C;
    d[3]    = 0xFE;
    float v = 0.0f;
    TEST_ASSERT_TRUE(find(decodeFrame(0x201, d, 8, kBase), SignalId::IgnitionTiming, v));
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, -10.0f, v);
}

void test_UT04_sensors1_temperature_offset() {
    uint8_t d[8];
    zero(d);
    d[0] = 0xB8;
    d[1] = 0x0B;  // MAP = 3000 / 30 = 100.0 kPa
    d[2] = 125;   // CLT = 125 - 40 = 85 degC
    d[3] = 60;    // IAT = 60 - 40 = 20 degC
    d[7] = 120;   // FuelLevel = 120 * 0.5 = 60 %

    DecodeResult r = decodeFrame(0x203, d, 8, kBase);
    float v        = 0.0f;
    TEST_ASSERT_TRUE(find(r, SignalId::Map, v));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, v);
    TEST_ASSERT_TRUE(find(r, SignalId::Clt, v));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 85.0f, v);
    TEST_ASSERT_TRUE(find(r, SignalId::Iat, v));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 20.0f, v);
    TEST_ASSERT_TRUE(find(r, SignalId::FuelLevel, v));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 60.0f, v);

    // 氷点下も扱えること
    zero(d);
    d[2] = 30;  // -10 degC
    TEST_ASSERT_TRUE(find(decodeFrame(0x203, d, 8, kBase), SignalId::Clt, v));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, -10.0f, v);
}

void test_UT04_sensors2_battery_and_padding() {
    uint8_t d[8];
    zero(d);
    // byte0-1 はパディング。ここに値が入っていても油圧に混ざらないこと。
    d[0] = 0xFF;
    d[1] = 0xFF;
    d[2] = 0xB8;
    d[3] = 0x0B;  // OilPress = 3000 / 30 = 100.0 kPa
    d[4] = 130;   // OilTemp = 90 degC
    d[6] = 0xE8;
    d[7] = 0x35;  // BattVolt = 13800 mV = 13.8 V

    DecodeResult r = decodeFrame(0x204, d, 8, kBase);
    float v        = 0.0f;
    TEST_ASSERT_TRUE(find(r, SignalId::OilPressure, v));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 100.0f, v);
    TEST_ASSERT_TRUE(find(r, SignalId::OilTemp, v));
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 90.0f, v);
    TEST_ASSERT_TRUE(find(r, SignalId::BattVolt, v));
    TEST_ASSERT_FLOAT_WITHIN(1e-3f, 13.8f, v);
}

void test_UT04_status_flag_bit_positions() {
    uint8_t d[8];
    zero(d);
    d[4] = status_bit::kCheckEngine | status_bit::kLambdaProtect;
    d[5] = 3;  // gear

    DecodeResult r = decodeFrame(0x200, d, 8, kBase);
    float v        = 0.0f;
    TEST_ASSERT_TRUE(find(r, SignalId::StatusFlags, v));
    const uint32_t flags = static_cast<uint32_t>(v);
    TEST_ASSERT_TRUE((flags & status_bit::kCheckEngine) != 0);
    TEST_ASSERT_TRUE((flags & status_bit::kLambdaProtect) != 0);
    TEST_ASSERT_FALSE((flags & status_bit::kRevLimit) != 0);
    TEST_ASSERT_FALSE((flags & status_bit::kFan1) != 0);

    TEST_ASSERT_TRUE(find(r, SignalId::Gear, v));
    TEST_ASSERT_FLOAT_WITHIN(0.5f, 3.0f, v);
}

// ---------------------------------------------------------------- UT-10
// SWR-06: 不正フレームの扱い
void test_UT10_short_dlc_rejected() {
    uint8_t d[8];
    zero(d);
    d[0] = 0x10;
    d[1] = 0x27;

    for (uint8_t dlc = 0; dlc < 8; ++dlc) {
        DecodeResult r = decodeFrame(0x207, d, dlc, kBase);
        TEST_ASSERT_FALSE(r.accepted);
        TEST_ASSERT_EQUAL_UINT8(0, r.count);
    }
    TEST_ASSERT_TRUE(decodeFrame(0x207, d, 8, kBase).accepted);
}

void test_UT10_null_pointer_is_safe() {
    DecodeResult r = decodeFrame(0x207, nullptr, 8, kBase);
    TEST_ASSERT_FALSE(r.accepted);
    TEST_ASSERT_EQUAL_UINT8(0, r.count);
}

void test_UT10_unused_frames_accepted_without_signals() {
    // 本機が使わないフレーム（Cams など）は「受理したが信号なし」となり、
    // 診断の unknownId カウンタを無用に増やさない
    uint8_t d[8];
    zero(d);
    DecodeResult r = decodeFrame(kBase + kOffCams, d, 8, kBase);
    TEST_ASSERT_TRUE(r.accepted);
    TEST_ASSERT_EQUAL_UINT8(0, r.count);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_UT01_base_id_offset);
    RUN_TEST(test_UT01_out_of_range_id_rejected);
    RUN_TEST(test_UT01_extended_id_uses_same_arithmetic);
    RUN_TEST(test_UT02_lambda_decode);
    RUN_TEST(test_UT02_lambda_is_little_endian);
    RUN_TEST(test_UT02_lambda_validity);
    RUN_TEST(test_UT03_egt_decode);
    RUN_TEST(test_UT03_egt_validity);
    RUN_TEST(test_UT04_speeds_frame);
    RUN_TEST(test_UT04_ignition_timing_is_signed);
    RUN_TEST(test_UT04_sensors1_temperature_offset);
    RUN_TEST(test_UT04_sensors2_battery_and_padding);
    RUN_TEST(test_UT04_status_flag_bit_positions);
    RUN_TEST(test_UT10_short_dlc_rejected);
    RUN_TEST(test_UT10_null_pointer_is_safe);
    RUN_TEST(test_UT10_unused_frames_accepted_without_signals);
    return UNITY_END();
}
