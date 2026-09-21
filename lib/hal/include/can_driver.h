// SWA-01 / SWD-05: TWAI (CAN) ドライバ
//
// 【安全上の不変条件 — RSK-06 / SYS-07】
// 本ドライバは TWAI_MODE_LISTEN_ONLY でのみ初期化する。送信 API を公開しない。
// コードベース全体で TWAI の送信 API の呼び出しを禁止し、CI の静的チェック
// （DOC-41 §3.1 S2/S3）で混入を検出する。
#pragma once

#include <driver/twai.h>
#include <cstdint>

#include "config.h"

namespace cm {

struct CanStats {
    uint32_t rxFrames      = 0;  ///< 受理した有効フレーム総数
    uint32_t unknownId     = 0;  ///< ベース範囲外で破棄したフレーム数
    uint32_t badDlc        = 0;  ///< DLC 不足で破棄したフレーム数
    uint32_t queueOverflow = 0;  ///< 受信キュー溢れ（SWR-03）
    uint32_t busOffCount   = 0;
    uint32_t recoveryCount = 0;
    uint32_t tec           = 0;  ///< TWAI 送信エラーカウンタ
    uint32_t rec           = 0;  ///< TWAI 受信エラーカウンタ
    uint32_t framesPerSec  = 0;  ///< 直近 1 秒の受信フレーム数
};

enum class CanState : uint8_t { Stopped = 0, Running, BusOff, Recovering };

class CanDriver {
public:
    /// Listen Only モードで TWAI を開始する。
    /// @return 成功したら true
    bool begin(const Config& cfg);

    /// フレームを 1 件受信する。タイムアウト時は false。
    bool receive(twai_message_t& out, uint32_t timeoutMs);

    /// can_health タスクから 1 秒周期で呼ぶ。バスオフ検出と復旧、統計更新を行う。
    void poll(uint32_t nowMs);

    CanState        state() const { return m_state; }
    const CanStats& stats() const { return m_stats; }
    CanStats&       mutableStats() { return m_stats; }

private:
    void applyFilter(const Config& cfg, twai_filter_config_t& f) const;

    CanState m_state          = CanState::Stopped;
    CanStats m_stats;
    uint32_t m_lastFpsMs      = 0;
    uint32_t m_lastFpsFrames  = 0;
    uint32_t m_nextRetryMs    = 0;
    uint8_t  m_failedRecovery = 0;
};

}  // namespace cm
