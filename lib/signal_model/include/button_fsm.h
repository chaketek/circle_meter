// SWD-06 の判定ロジック部（HW 非依存）
//
// GPIO の読み取りは lib/hal の InputDriver が行い、その結果（押されているか）だけを
// 本クラスに渡す。こうすることで短押し/長押し/チャタリング除去のロジックを
// PC 上で検証できる（UT-12）。
#pragma once

#include <cstdint>

namespace cm {

enum class ButtonEvent : uint8_t { None = 0, Short, Long };

/// SWR-64: チャタリング 20ms、短押し < 600ms、長押し >= 1500ms。
/// 長押しは「離す前に」1 回だけ発火し、その後離しても短押しは発火しない。
class ButtonFsm {
public:
    static constexpr uint32_t kDebounceMs = 20;
    static constexpr uint32_t kShortMaxMs = 600;
    static constexpr uint32_t kLongMs     = 1500;

    /// @param pressed 生のレベル（true = 押下）
    /// @param nowMs   単調増加するミリ秒
    ButtonEvent update(bool pressed, uint32_t nowMs) {
        if (pressed != m_rawState) {
            m_rawState       = pressed;
            m_lastChangeMs   = nowMs;
            m_debouncePendng = true;
        }

        // チャタリング除去: 変化から kDebounceMs 安定して初めて確定状態を更新する
        if (m_debouncePendng && (nowMs - m_lastChangeMs) >= kDebounceMs) {
            m_debouncePendng = false;
            if (m_rawState != m_stableState) {
                m_stableState = m_rawState;
                if (m_stableState) {
                    m_pressStartMs = m_lastChangeMs;
                    m_longFired    = false;
                } else {
                    // 離した瞬間
                    const uint32_t held = m_lastChangeMs - m_pressStartMs;
                    if (!m_longFired && held < kShortMaxMs) {
                        return ButtonEvent::Short;
                    }
                    return ButtonEvent::None;
                }
            }
        }

        // 押しっぱなしのまま長押し時間に到達したら、離す前に 1 回だけ発火する
        if (m_stableState && !m_longFired && (nowMs - m_pressStartMs) >= kLongMs) {
            m_longFired = true;
            return ButtonEvent::Long;
        }

        return ButtonEvent::None;
    }

    bool isDown() const { return m_stableState; }

    void reset() { *this = ButtonFsm{}; }

private:
    bool m_rawState         = false;
    bool m_stableState      = false;
    bool m_debouncePendng   = false;
    bool m_longFired        = false;
    uint32_t m_lastChangeMs = 0;
    uint32_t m_pressStartMs = 0;
};

}  // namespace cm
