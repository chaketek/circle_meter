// SWE.4 ユニット検証: 信号モデル（鮮度・単位換算・設定）
// UT-05 〜 UT-09, UT-11 〜 UT-15  (docs/30_test_strategy.md §2)
#include <unity.h>

#include <atomic>
#include <cstring>
#include <new>
#include <thread>

#include "button_fsm.h"
#include "config.h"
#include "rusefi_decoder.h"
#include "signal_store.h"
#include "units.h"

using namespace cm;

namespace {
const LambdaZoneConfig kZones{0.75f, 0.85f, 1.03f, 1.10f};
const EgtConfig kEgt{850, 920};
}  // namespace

void setUp() {}
void tearDown() {}

// ---------------------------------------------------------------- UT-05
// SWR-27 / SYS-11: ゾーン境界は「下限 < λ <= 上限」
void test_UT05_zone_boundaries_exact() {
    TEST_ASSERT_EQUAL(static_cast<int>(LambdaZone::RichHeavy), static_cast<int>(zoneOf(0.60f, kZones)));
    TEST_ASSERT_EQUAL(static_cast<int>(LambdaZone::RichHeavy), static_cast<int>(zoneOf(0.75f, kZones)));
    TEST_ASSERT_EQUAL(static_cast<int>(LambdaZone::Rich), static_cast<int>(zoneOf(0.7501f, kZones)));
    TEST_ASSERT_EQUAL(static_cast<int>(LambdaZone::Rich), static_cast<int>(zoneOf(0.85f, kZones)));
    TEST_ASSERT_EQUAL(static_cast<int>(LambdaZone::Optimal), static_cast<int>(zoneOf(0.8501f, kZones)));
    TEST_ASSERT_EQUAL(static_cast<int>(LambdaZone::Optimal), static_cast<int>(zoneOf(1.00f, kZones)));
    TEST_ASSERT_EQUAL(static_cast<int>(LambdaZone::Optimal), static_cast<int>(zoneOf(1.03f, kZones)));
    TEST_ASSERT_EQUAL(static_cast<int>(LambdaZone::Lean), static_cast<int>(zoneOf(1.0301f, kZones)));
    TEST_ASSERT_EQUAL(static_cast<int>(LambdaZone::Lean), static_cast<int>(zoneOf(1.10f, kZones)));
    TEST_ASSERT_EQUAL(static_cast<int>(LambdaZone::LeanHeavy), static_cast<int>(zoneOf(1.1001f, kZones)));
    TEST_ASSERT_EQUAL(static_cast<int>(LambdaZone::LeanHeavy), static_cast<int>(zoneOf(1.50f, kZones)));
}

void test_UT05_float_and_fixed_point_agree() {
    // SWD-01 §1.3: raw(1e-4 固定小数点) 版と float 版が境界でも一致すること
    for (LambdaQ4 raw = 5000; raw <= 15000; ++raw) {
        const LambdaZone a = zoneOfQ4(raw, kZones);
        const LambdaZone b = zoneOf(fromQ4(raw), kZones);
        TEST_ASSERT_EQUAL(static_cast<int>(a), static_cast<int>(b));
    }
}

void test_UT05_custom_zones_applied() {
    const LambdaZoneConfig turbo{0.70f, 0.80f, 0.95f, 1.02f};
    TEST_ASSERT_EQUAL(static_cast<int>(LambdaZone::Lean), static_cast<int>(zoneOf(1.00f, turbo)));
    TEST_ASSERT_EQUAL(static_cast<int>(LambdaZone::Optimal), static_cast<int>(zoneOf(1.00f, kZones)));
}

void test_UT05_ring_ratio_clamped() {
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, ringRatio(0.68f, 0.68f, 1.36f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, ringRatio(1.36f, 0.68f, 1.36f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.5f, ringRatio(1.02f, 0.68f, 1.36f));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, ringRatio(0.10f, 0.68f, 1.36f));  // 下限未満
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 1.0f, ringRatio(9.00f, 0.68f, 1.36f));  // 上限超過
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.0f, ringRatio(1.00f, 1.36f, 0.68f));  // 不正レンジ
}

// ---------------------------------------------------------------- UT-06
// SWR-25 / SYS-14: λ <-> AFR
void test_UT06_lambda_to_afr() {
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 14.70f, lambdaToAfr(1.00f, kStoichGasoline));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 12.495f, lambdaToAfr(0.85f, kStoichGasoline));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 9.765f, lambdaToAfr(1.00f, 9.765f));  // E85
}

void test_UT06_round_trip() {
    for (float lam = 0.60f; lam <= 1.40f; lam += 0.01f) {
        const float afr  = lambdaToAfr(lam, kStoichGasoline);
        const float back = afrToLambda(afr, kStoichGasoline);
        TEST_ASSERT_FLOAT_WITHIN(1e-5f, lam, back);
    }
}

void test_UT06_zero_stoich_is_safe() {
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.0f, afrToLambda(14.7f, 0.0f));
}

// ---------------------------------------------------------------- UT-07
// SWR-28 / SYS-16: EGT 警告レベル
void test_UT07_egt_levels() {
    TEST_ASSERT_EQUAL(static_cast<int>(EgtLevel::Normal), static_cast<int>(levelOf(849.0f, kEgt)));
    TEST_ASSERT_EQUAL(static_cast<int>(EgtLevel::Warn), static_cast<int>(levelOf(850.0f, kEgt)));
    TEST_ASSERT_EQUAL(static_cast<int>(EgtLevel::Warn), static_cast<int>(levelOf(919.0f, kEgt)));
    TEST_ASSERT_EQUAL(static_cast<int>(EgtLevel::Danger), static_cast<int>(levelOf(920.0f, kEgt)));
    TEST_ASSERT_EQUAL(static_cast<int>(EgtLevel::Danger), static_cast<int>(levelOf(1200.0f, kEgt)));
}

void test_UT07_custom_thresholds() {
    const EgtConfig turbo{780, 860};
    TEST_ASSERT_EQUAL(static_cast<int>(EgtLevel::Warn), static_cast<int>(levelOf(800.0f, turbo)));
    TEST_ASSERT_EQUAL(static_cast<int>(EgtLevel::Normal), static_cast<int>(levelOf(800.0f, kEgt)));
}

// ---------------------------------------------------------------- UT-08
// SWR-20/21/22 / SYS-40/41 / RSK-01: 鮮度管理
void test_UT08_freshness_thresholds() {
    SignalStore st;
    st.update(SignalId::Lambda1, 0.95f, 1000);

    TEST_ASSERT_EQUAL(static_cast<int>(Freshness::Fresh),
                      static_cast<int>(st.snapshot(1000).freshnessOf(SignalId::Lambda1)));
    TEST_ASSERT_EQUAL(static_cast<int>(Freshness::Fresh),
                      static_cast<int>(st.snapshot(1499).freshnessOf(SignalId::Lambda1)));
    TEST_ASSERT_EQUAL(static_cast<int>(Freshness::Stale),
                      static_cast<int>(st.snapshot(1500).freshnessOf(SignalId::Lambda1)));
    TEST_ASSERT_EQUAL(static_cast<int>(Freshness::Stale),
                      static_cast<int>(st.snapshot(2999).freshnessOf(SignalId::Lambda1)));
    TEST_ASSERT_EQUAL(static_cast<int>(Freshness::Lost),
                      static_cast<int>(st.snapshot(3000).freshnessOf(SignalId::Lambda1)));
}

void test_UT08_never_received_is_lost() {
    SignalStore st;
    TEST_ASSERT_EQUAL(static_cast<int>(Freshness::Lost),
                      static_cast<int>(st.snapshot(0).freshnessOf(SignalId::Egt1)));
}

void test_UT08_lost_signal_must_not_leak_stale_value() {
    // RSK-01（最上位ハザード）: 途絶後に古い値を返してはならない
    SignalStore st;
    st.update(SignalId::Lambda1, 0.95f, 1000);

    float out = -1.0f;
    TEST_ASSERT_TRUE(st.snapshot(1400).get(SignalId::Lambda1, out));
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.95f, out);

    // Stale では表示を続けてよい（グレー化は UI 側の責務）
    out = -1.0f;
    TEST_ASSERT_TRUE(st.snapshot(1600).get(SignalId::Lambda1, out));

    // Lost では false を返し、out を書き換えない
    out = -1.0f;
    TEST_ASSERT_FALSE(st.snapshot(3500).get(SignalId::Lambda1, out));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -1.0f, out);
}

void test_UT08_snapshot_is_per_signal() {
    SignalStore st;
    st.update(SignalId::Lambda1, 0.95f, 1000);
    st.update(SignalId::Egt1, 800.0f, 2900);

    const Snapshot s = st.snapshot(3000);
    float v          = 0.0f;
    TEST_ASSERT_FALSE(s.get(SignalId::Lambda1, v));  // 2000ms 経過 -> Lost
    TEST_ASSERT_TRUE(s.get(SignalId::Egt1, v));      // 100ms -> Fresh
    TEST_ASSERT_FLOAT_WITHIN(0.01f, 800.0f, v);
}

void test_UT08_status_bit_helper() {
    SignalStore st;
    st.update(SignalId::StatusFlags, static_cast<float>(status_bit::kCheckEngine | status_bit::kFuelPump),
              1000);

    const Snapshot s = st.snapshot(1100);
    bool bit         = false;
    TEST_ASSERT_TRUE(s.statusBit(status_bit::kCheckEngine, bit));
    TEST_ASSERT_TRUE(bit);
    TEST_ASSERT_TRUE(s.statusBit(status_bit::kLambdaProtect, bit));
    TEST_ASSERT_FALSE(bit);

    // Lost のときはフラグも取得できない
    TEST_ASSERT_FALSE(st.snapshot(5000).statusBit(status_bit::kCheckEngine, bit));
}

// ---------------------------------------------------------------- UT-09
// SWR-23 / SYS-42 / RSK-09: 無効値
void test_UT09_invalid_values_rejected_before_store() {
    // デコーダの妥当性判定を通った値だけをストアに入れる、という運用を模擬する
    SignalStore st;

    const float lambdaFromEcu = 0.0f;  // センサ未ウォームアップ
    if (rusefi::isLambdaValid(lambdaFromEcu)) {
        st.update(SignalId::Lambda1, lambdaFromEcu, 1000);
    }
    const float egtFromEcu = 0.0f;  // EGT1 未構成
    if (rusefi::isEgtValid(egtFromEcu)) {
        st.update(SignalId::Egt1, egtFromEcu, 1000);
    }

    float v = -1.0f;
    TEST_ASSERT_FALSE(st.snapshot(1000).get(SignalId::Lambda1, v));
    TEST_ASSERT_FALSE(st.snapshot(1000).get(SignalId::Egt1, v));
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, -1.0f, v);
}

// ---------------------------------------------------------------- UT-11
// SWR-24: 1 次ローパス
void test_UT11_lpf_step_response() {
    Lpf1 f;
    f.configure(80.0f);
    f.reset(0.0f);

    // dt=33ms, tau=80ms -> alpha = 33/113 = 0.29204
    const float y1 = f.update(1.0f, 33);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, 0.292035f, y1);

    const float y2 = f.update(1.0f, 33);
    TEST_ASSERT_FLOAT_WITHIN(1e-4f, y1 + 0.292035f * (1.0f - y1), y2);
}

void test_UT11_lpf_zero_dt_holds_value() {
    Lpf1 f;
    f.configure(80.0f);
    f.reset(0.5f);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.5f, f.update(1.0f, 0));
}

void test_UT11_lpf_reset_jumps_immediately() {
    Lpf1 f;
    f.configure(200.0f);
    f.reset(0.0f);
    f.update(1000.0f, 33);
    f.reset(845.0f);
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 845.0f, f.value());
}

void test_UT11_lpf_first_update_seeds_value() {
    // 未初期化のまま update すると初回入力で seed される（起動直後の緩慢な立ち上がりを防ぐ）
    Lpf1 f;
    f.configure(80.0f);
    TEST_ASSERT_FALSE(f.initialized());
    TEST_ASSERT_FLOAT_WITHIN(1e-6f, 0.93f, f.update(0.93f, 33));
    TEST_ASSERT_TRUE(f.initialized());
}

// ---------------------------------------------------------------- UT-12
// SWR-64: ボタンの短押し / 長押し / チャタリング除去
namespace {
/// ボタンを押し、hold ms 保持してから離す。その間 1ms 刻みで FSM を回し、
/// 発生したイベントを返す（複数発生した場合は最初の 1 件）。
ButtonEvent pressFor(ButtonFsm& b, uint32_t startMs, uint32_t holdMs, uint32_t tailMs = 200) {
    ButtonEvent first  = ButtonEvent::None;
    const uint32_t end = startMs + holdMs + tailMs;
    for (uint32_t t = startMs; t <= end; ++t) {
        const bool pressed  = (t >= startMs) && (t < startMs + holdMs);
        const ButtonEvent e = b.update(pressed, t);
        if (e != ButtonEvent::None && first == ButtonEvent::None)
            first = e;
    }
    return first;
}
}  // namespace

void test_UT12_chatter_is_ignored() {
    ButtonFsm b;
    TEST_ASSERT_EQUAL(static_cast<int>(ButtonEvent::None), static_cast<int>(pressFor(b, 100, 10)));
    TEST_ASSERT_EQUAL(static_cast<int>(ButtonEvent::None), static_cast<int>(pressFor(b, 1000, 19)));
}

void test_UT12_short_press() {
    ButtonFsm b;
    TEST_ASSERT_EQUAL(static_cast<int>(ButtonEvent::Short), static_cast<int>(pressFor(b, 100, 100)));
    b.reset();
    TEST_ASSERT_EQUAL(static_cast<int>(ButtonEvent::Short), static_cast<int>(pressFor(b, 100, 599)));
}

void test_UT12_between_short_and_long_yields_nothing() {
    // 600ms 以上 1500ms 未満で離した場合は短押しにも長押しにもしない
    ButtonFsm b;
    TEST_ASSERT_EQUAL(static_cast<int>(ButtonEvent::None), static_cast<int>(pressFor(b, 100, 900)));
}

void test_UT12_long_press_fires_once_before_release() {
    ButtonFsm b;
    ButtonEvent first = ButtonEvent::None;
    int longCount     = 0;
    int shortCount    = 0;
    // 3 秒押しっぱなしにしてから離す
    for (uint32_t t = 0; t <= 3500; ++t) {
        const bool pressed  = (t >= 100) && (t < 3100);
        const ButtonEvent e = b.update(pressed, t);
        if (e == ButtonEvent::Long) {
            ++longCount;
            if (first == ButtonEvent::None)
                first = e;
        }
        if (e == ButtonEvent::Short)
            ++shortCount;
    }
    TEST_ASSERT_EQUAL(static_cast<int>(ButtonEvent::Long), static_cast<int>(first));
    TEST_ASSERT_EQUAL_INT(1, longCount);   // 1 回だけ
    TEST_ASSERT_EQUAL_INT(0, shortCount);  // 離しても短押しにならない
}

// ---------------------------------------------------------------- UT-13
// SWR-80/81 / SWD-04: 設定の検証
void test_UT13_default_config_is_valid() {
    const Config c = defaultConfig();
    TEST_ASSERT_TRUE(validate(c));
}

void test_UT13_crc_is_independent_of_padding() {
    // 構造体のメモリ像をそのまま CRC にかけるとパディングの不定値が混ざり、
    // 同じ設定値でも CRC が変わりうる（SWD-04）。フィールド単位で積んでいることを確認する。
    alignas(Config) unsigned char rawA[sizeof(Config)];
    alignas(Config) unsigned char rawB[sizeof(Config)];
    memset(rawA, 0x00, sizeof(rawA));
    memset(rawB, 0xFF, sizeof(rawB));

    Config* a = new (rawA) Config();
    Config* b = new (rawB) Config();
    *a        = defaultConfig();
    *b        = defaultConfig();

    TEST_ASSERT_EQUAL_UINT32(computeCrc(*a), computeCrc(*b));
    TEST_ASSERT_TRUE(validate(*a));
    TEST_ASSERT_TRUE(validate(*b));
}

void test_UT13_every_field_affects_crc() {
    // computeCrc へのフィールド追加漏れを検出する
    const Config base = defaultConfig();

    Config c   = base;
    c.lastPage = 3;
    TEST_ASSERT_NOT_EQUAL(computeCrc(base), computeCrc(c));

    c               = base;
    c.canExtendedId = true;
    TEST_ASSERT_NOT_EQUAL(computeCrc(base), computeCrc(c));

    c             = base;
    c.egt.dangerC = 950;
    TEST_ASSERT_NOT_EQUAL(computeCrc(base), computeCrc(c));

    c                  = base;
    c.zones.optimalMax = 1.05f;
    TEST_ASSERT_NOT_EQUAL(computeCrc(base), computeCrc(c));

    c               = base;
    c.buzzerEnabled = false;
    TEST_ASSERT_NOT_EQUAL(computeCrc(base), computeCrc(c));
}

void test_UT13_crc_mismatch_detected() {
    Config c     = defaultConfig();
    c.brightness = 2;  // CRC を更新せずに値だけ変える
    TEST_ASSERT_FALSE(validate(c));
    c.crc32 = computeCrc(c);
    TEST_ASSERT_TRUE(validate(c));
}

void test_UT13_version_mismatch_detected() {
    Config c  = defaultConfig();
    c.version = kConfigVersion + 1;
    c.crc32   = computeCrc(c);
    TEST_ASSERT_FALSE(validate(c));
}

void test_UT13_monotonic_violation_rejected() {
    Config c = defaultConfig();

    c.zones.richMax = 0.70f;  // richHeavyMax(0.75) より小さい
    TEST_ASSERT_FALSE(validateMonotonic(c));
    c.crc32 = computeCrc(c);
    TEST_ASSERT_FALSE(validate(c));

    c        = defaultConfig();
    c.ringHi = 1.05f;  // leanMax(1.10) より小さい
    c.crc32  = computeCrc(c);
    TEST_ASSERT_FALSE(validate(c));

    c        = defaultConfig();
    c.ringLo = 0.80f;  // richHeavyMax(0.75) より大きい
    c.crc32  = computeCrc(c);
    TEST_ASSERT_FALSE(validate(c));
}

void test_UT13_range_violations_rejected() {
    Config c = defaultConfig();

    c.stoich = 0.0f;
    c.crc32  = computeCrc(c);
    TEST_ASSERT_FALSE(validate(c));

    c            = defaultConfig();
    c.brightness = 0;
    c.crc32      = computeCrc(c);
    TEST_ASSERT_FALSE(validate(c));

    c            = defaultConfig();
    c.brightness = 6;
    c.crc32      = computeCrc(c);
    TEST_ASSERT_FALSE(validate(c));

    c           = defaultConfig();
    c.egt.warnC = 950;  // dangerC(920) 以上は不正
    c.crc32     = computeCrc(c);
    TEST_ASSERT_FALSE(validate(c));

    c                = defaultConfig();
    c.canBitrateKbps = 125;
    c.crc32          = computeCrc(c);
    TEST_ASSERT_FALSE(validate(c));
}

// ---------------------------------------------------------------- UT-14
// SWD-02 §2.3: 32bit ミリ秒カウンタのラップアラウンド
void test_UT14_millis_wraparound() {
    SignalStore st;
    st.update(SignalId::Lambda1, 0.95f, 0xFFFFFF00u);

    // 0xFFFFFF00 -> 0x00000050 は 336ms 経過 -> Fresh（ラップをまたいでも正しく減算できる）
    const Snapshot s = st.snapshot(0x00000050u);
    TEST_ASSERT_EQUAL(static_cast<int>(Freshness::Fresh), static_cast<int>(s.freshnessOf(SignalId::Lambda1)));

    // 0xFFFFFF00 -> 0x00000100 は 512ms 経過 -> Stale
    TEST_ASSERT_EQUAL(static_cast<int>(Freshness::Stale),
                      static_cast<int>(st.snapshot(0x00000100u).freshnessOf(SignalId::Lambda1)));

    // 0xFFFFFF00 -> 0x00000500 は 1536ms 経過 -> Stale
    TEST_ASSERT_EQUAL(static_cast<int>(Freshness::Stale),
                      static_cast<int>(st.snapshot(0x00000500u).freshnessOf(SignalId::Lambda1)));

    // 0xFFFFFF00 -> 0x00000900 は 2560ms 経過 -> Lost
    TEST_ASSERT_EQUAL(static_cast<int>(Freshness::Lost),
                      static_cast<int>(st.snapshot(0x00000900u).freshnessOf(SignalId::Lambda1)));
}

// ---------------------------------------------------------------- UT-15
// SWD-02 / DOC-21 §5.1: seqlock の読み出しが書き込みと競合しても、
// 受信中の信号を Lost と報告してはならない。
// 実機で「受信は 120 f/s で継続しているのに一瞬だけ NO SIGNAL が出る」という形で
// 踏んだ不具合の回帰テスト。
//
// 論理時刻を固定して呼ぶため鮮度は常に Fresh のはず。ここで Lost が観測されたら、
// スナップショットが一度も埋められずに返ったことを意味する。

/// 書き込みを止めずに回し続けるスレッド。
/// busyGap を 0 にすると seqlock のデューティが 100 % に近くなり、
/// 読み出しが構造的に成立しなくなる（seqlock の既知の性質。飢餓）。
struct Writer {
    SignalStore& st;
    uint32_t t;
    int busyGap;
    std::atomic<bool> stop{false};
    std::thread th;

    Writer(SignalStore& s, uint32_t time, int gap) : st(s), t(time), busyGap(gap) {
        th = std::thread([this] {
            float v = 0.70f;
            while (!stop.load(std::memory_order_relaxed)) {
                v = (v > 1.29f) ? 0.70f : v + 0.01f;
                // 2 つの λ は常に同じ値に保つ。読み側で食い違ったら世代が混ざっている。
                st.update(SignalId::Lambda1, v, t);
                st.update(SignalId::Lambda2, v, t);
                st.update(SignalId::Egt1, 500.0f, t);
                for (volatile int k = 0; k < busyGap; ++k) {
                }
            }
        });
    }
    ~Writer() {
        stop.store(true, std::memory_order_relaxed);
        th.join();
    }
};

/// 書き込みが飢餓を起こすほど高頻度でも、受信済みの信号を Lost と報告しないこと。
/// 旧実装は空のスナップショットを返すため必ず失敗する。
void test_UT15_no_false_lost_even_when_reader_starves() {
    SignalStore st;
    constexpr uint32_t kT = 1000;

    st.update(SignalId::Lambda1, 0.95f, kT);
    st.update(SignalId::Lambda2, 0.95f, kT);
    st.update(SignalId::Egt1, 500.0f, kT);
    (void)st.snapshot(kT);  // 競合の無い状態で 1 回確定させる（起動直後に相当）

    Writer w(st, kT, 0);  // 隙間なしで書き込み続ける = 読み出しは飢餓になりうる

    int lostSeen = 0;
    for (int i = 0; i < 100000; ++i) {
        const Snapshot s = st.snapshot(kT);
        float v          = 0.0f;
        if (!s.get(SignalId::Lambda1, v)) {
            ++lostSeen;
        }
        if (!s.get(SignalId::Egt1, v)) {
            ++lostSeen;
        }
    }
    TEST_ASSERT_EQUAL_INT(0, lostSeen);
}

/// 実機相当の書き込み頻度（隙間あり）なら、スナップショットは毎回成立すること。
/// 書き込み中に待たずリトライする旧実装ではここが 0 にならない。
void test_UT15_snapshot_succeeds_under_realistic_load() {
    SignalStore st;
    constexpr uint32_t kT = 1000;

    st.update(SignalId::Lambda1, 0.95f, kT);
    (void)st.snapshot(kT);

    Writer w(st, kT, 2000);  // 書き込みのあいだに十分な隙間を空ける

    for (int i = 0; i < 20000; ++i) {
        (void)st.snapshot(kT);
    }
    TEST_ASSERT_EQUAL_UINT32(0, st.snapshotFailures());
}

/// 世代が混ざらない（ティアリングしない）こと。
void test_UT15_snapshot_is_not_torn() {
    SignalStore st;
    constexpr uint32_t kT = 1000;
    st.update(SignalId::Lambda1, 1.0f, kT);
    st.update(SignalId::Lambda2, 1.0f, kT);
    (void)st.snapshot(kT);

    Writer w(st, kT, 200);

    int mismatched = 0;
    for (int i = 0; i < 50000; ++i) {
        const Snapshot s = st.snapshot(kT);
        float a = 0.0f, b = 0.0f;
        if (s.get(SignalId::Lambda1, a) && s.get(SignalId::Lambda2, b)) {
            // Lambda1 を書いた直後・Lambda2 を書く前の世代を読むのは正当なので、
            // 差は 1 ステップ (0.01) 以内に収まるはず。それ以上は破損を意味する。
            const float diff = (a > b) ? (a - b) : (b - a);
            if (diff > 0.011f && diff < 0.58f) {
                ++mismatched;
            }
        }
    }
    TEST_ASSERT_EQUAL_INT(0, mismatched);
}

int main(int, char**) {
    UNITY_BEGIN();
    RUN_TEST(test_UT05_zone_boundaries_exact);
    RUN_TEST(test_UT05_float_and_fixed_point_agree);
    RUN_TEST(test_UT05_custom_zones_applied);
    RUN_TEST(test_UT05_ring_ratio_clamped);
    RUN_TEST(test_UT06_lambda_to_afr);
    RUN_TEST(test_UT06_round_trip);
    RUN_TEST(test_UT06_zero_stoich_is_safe);
    RUN_TEST(test_UT07_egt_levels);
    RUN_TEST(test_UT07_custom_thresholds);
    RUN_TEST(test_UT08_freshness_thresholds);
    RUN_TEST(test_UT08_never_received_is_lost);
    RUN_TEST(test_UT08_lost_signal_must_not_leak_stale_value);
    RUN_TEST(test_UT08_snapshot_is_per_signal);
    RUN_TEST(test_UT08_status_bit_helper);
    RUN_TEST(test_UT09_invalid_values_rejected_before_store);
    RUN_TEST(test_UT11_lpf_step_response);
    RUN_TEST(test_UT11_lpf_zero_dt_holds_value);
    RUN_TEST(test_UT11_lpf_reset_jumps_immediately);
    RUN_TEST(test_UT11_lpf_first_update_seeds_value);
    RUN_TEST(test_UT12_chatter_is_ignored);
    RUN_TEST(test_UT12_short_press);
    RUN_TEST(test_UT12_between_short_and_long_yields_nothing);
    RUN_TEST(test_UT12_long_press_fires_once_before_release);
    RUN_TEST(test_UT13_default_config_is_valid);
    RUN_TEST(test_UT13_crc_is_independent_of_padding);
    RUN_TEST(test_UT13_every_field_affects_crc);
    RUN_TEST(test_UT13_crc_mismatch_detected);
    RUN_TEST(test_UT13_version_mismatch_detected);
    RUN_TEST(test_UT13_monotonic_violation_rejected);
    RUN_TEST(test_UT13_range_violations_rejected);
    RUN_TEST(test_UT14_millis_wraparound);
    RUN_TEST(test_UT15_no_false_lost_even_when_reader_starves);
    RUN_TEST(test_UT15_snapshot_succeeds_under_realistic_load);
    RUN_TEST(test_UT15_snapshot_is_not_torn);
    return UNITY_END();
}
