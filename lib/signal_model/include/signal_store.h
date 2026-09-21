// SWA-04 / SWD-02: 信号ストア（値 + 鮮度 + 妥当性）
//
// HW 非依存（DEC-06 / SWR-10）。
//
// 【最重要の設計意図 — RSK-01】
// Lost（信号喪失）の値を外部に返さない。これを規律ではなく「型」で強制するため、
// 値と鮮度を別々に取得する API は提供しない。get() は Lost のとき false を返し、
// 出力変数を変更しない。呼び出し側は古い値を表示するコードを書けない。
#pragma once

#include <atomic>
#include <cstdint>
#include "signal_id.h"

namespace cm {

enum class Freshness : uint8_t { Lost = 0, Stale, Fresh };

/// 鮮度判定の閾値（SYS-40 / SYS-41 / DOC-13 §4）
constexpr uint32_t kStaleAfterMs = 500;
constexpr uint32_t kLostAfterMs  = 2000;

struct SignalValue {
    float    value        = 0.0f;
    uint32_t lastUpdateMs = 0;
    bool     everReceived = false;
};

/// ある瞬間の全信号の一貫したコピー。UI はこれを 1 フレームに 1 回取得する。
class Snapshot {
public:
    uint32_t takenAtMs = 0;

    Freshness freshnessOf(SignalId id) const {
        const SignalValue& v = m_s[idx(id)];
        if (!v.everReceived) return Freshness::Lost;
        // 32bit ラップアラウンド対応: 符号なし減算で差分を取る（UT-14）
        const uint32_t age = takenAtMs - v.lastUpdateMs;
        if (age < kStaleAfterMs) return Freshness::Fresh;
        if (age < kLostAfterMs) return Freshness::Stale;
        return Freshness::Lost;
    }

    /// Lost のときは false を返し、out を変更しない（RSK-01 / SWR-22）。
    bool get(SignalId id, float& out) const {
        if (freshnessOf(id) == Freshness::Lost) return false;
        out = m_s[idx(id)].value;
        return true;
    }

    /// 状態フラグ（DOC-13 §3.6）。Lost のときは false。
    bool statusBit(uint32_t mask, bool& out) const {
        float f = 0.0f;
        if (!get(SignalId::StatusFlags, f)) return false;
        out = (static_cast<uint32_t>(f) & mask) != 0;
        return true;
    }

private:
    friend class SignalStore;
    SignalValue m_s[kSignalCount];
};

/// 書き込みは CAN タスク（Core0）、読み出しは UI タスク（Core1）。
/// seqlock により書き込み側が一切ブロックしない（DOC-21 §5.1 / SWR-26）。
class SignalStore {
public:
    /// CAN タスクからのみ呼ぶ。
    void update(SignalId id, float v, uint32_t nowMs) {
        m_seq.fetch_add(1, std::memory_order_acquire);  // 奇数にする
        std::atomic_thread_fence(std::memory_order_release);

        SignalValue& s  = m_s[idx(id)];
        s.value         = v;
        s.lastUpdateMs  = nowMs;
        s.everReceived  = true;

        std::atomic_thread_fence(std::memory_order_release);
        m_seq.fetch_add(1, std::memory_order_release);  // 偶数に戻す
    }

    /// UI タスクからのみ呼ぶ。全信号を一貫した世代から読み出す。
    Snapshot snapshot(uint32_t nowMs) const {
        Snapshot out;
        out.takenAtMs = nowMs;

        // 書き込みが頻繁でもリトライは実用上 1-2 回で収束する。
        // 上限を設けて無限ループを防ぐ（最後の試行の内容をそのまま使う）。
        for (int attempt = 0; attempt < kMaxRetry; ++attempt) {
            const uint32_t s0 = m_seq.load(std::memory_order_acquire);
            if (s0 & 1u) continue;  // 書き込み中

            for (size_t i = 0; i < kSignalCount; ++i) {
                out.m_s[i] = m_s[i];
            }

            std::atomic_thread_fence(std::memory_order_acquire);
            if (m_seq.load(std::memory_order_relaxed) == s0) break;
        }
        return out;
    }

private:
    static constexpr int kMaxRetry = 8;

    mutable std::atomic<uint32_t> m_seq{0};
    SignalValue                   m_s[kSignalCount];
};

}  // namespace cm
