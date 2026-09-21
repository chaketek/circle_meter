// SWA-01 / SWD-05 実装
#include "can_driver.h"

#include <esp_log.h>

#include "rusefi_can_spec.h"

namespace cm {
namespace {
constexpr char kTag[] = "can";

/// バスオフ復旧の再試行間隔。5 回連続失敗したらバックオフする（SWD-05 §5.1）。
constexpr uint32_t kRetryMs     = 1000;
constexpr uint32_t kBackoffMs   = 10000;
constexpr uint8_t kBackoffAfter = 5;

twai_timing_config_t timingFor(uint16_t kbps) {
    switch (kbps) {
        case 250: {
            twai_timing_config_t t = TWAI_TIMING_CONFIG_250KBITS();
            return t;
        }
        case 1000: {
            twai_timing_config_t t = TWAI_TIMING_CONFIG_1MBITS();
            return t;
        }
        case 500:
        default: {
            twai_timing_config_t t = TWAI_TIMING_CONFIG_500KBITS();
            return t;
        }
    }
}

}  // namespace

void CanDriver::applyFilter(const Config& cfg, twai_filter_config_t& f) const {
    if (cfg.canExtendedId) {
        // 29bit の単一フィルタは範囲指定と相性が悪いため、ハードウェアでは絞らず
        // decodeFrame() 側の ID 判定に任せる（SYS-02 / DOC-12 §4）。
        f = TWAI_FILTER_CONFIG_ACCEPT_ALL();
        return;
    }

    // 標準 ID の single filter: acceptance_code のビット 31-21 が ID[10:0]。
    // ベース ID から 16 個（BASE+0 .. BASE+15）を通し、下位 4 ビットを don't care にする。
    // rusEFI が使うのは BASE+0..BASE+11 なので余剰の 12..15 は decodeFrame が弾く。
    f.acceptance_code = static_cast<uint32_t>(cfg.canBaseId & 0x7F0u) << 21;
    f.acceptance_mask = (0x00Fu << 21) | 0x1FFFFFu;
    f.single_filter   = true;
}

bool CanDriver::begin(const Config& cfg) {
    // DEC-04 / RSK-10: NORMAL で初期化し、受信フレームに ACK を返す。
    // 自己テスト用の TWAI_MODE_NO_ACK に変更してはならない（ACK を返さなくなる）。
    twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(
        static_cast<gpio_num_t>(CM_TWAI_TX_GPIO), static_cast<gpio_num_t>(CM_TWAI_RX_GPIO), TWAI_MODE_NORMAL);
    g.rx_queue_len = 64;  // SWR-03: 32 以上
    // SWR-1A / RSK-06 の第 2 層: 送信キューを持たない。
    // 万一 TWAI の送信 API が呼ばれてもキューイングできない。ACK 生成はキューを使わない。
    g.tx_queue_len = 0;

    twai_timing_config_t t = timingFor(cfg.canBitrateKbps);
    twai_filter_config_t f{};
    applyFilter(cfg, f);

    esp_err_t err = twai_driver_install(&g, &t, &f);
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "twai_driver_install failed: %d", err);
        return false;
    }
    err = twai_start();
    if (err != ESP_OK) {
        ESP_LOGE(kTag, "twai_start failed: %d", err);
        twai_driver_uninstall();
        return false;
    }

    m_state = CanState::Running;
    ESP_LOGI(kTag, "TWAI started: rx-only(ack), %u kbps, base 0x%03X, tx=%d rx=%d", cfg.canBitrateKbps,
             cfg.canBaseId, CM_TWAI_TX_GPIO, CM_TWAI_RX_GPIO);
    return true;
}

bool CanDriver::receive(twai_message_t& out, uint32_t timeoutMs) {
    if (m_state != CanState::Running)
        return false;
    return twai_receive(&out, pdMS_TO_TICKS(timeoutMs)) == ESP_OK;
}

void CanDriver::poll(uint32_t nowMs) {
    twai_status_info_t st{};
    if (twai_get_status_info(&st) == ESP_OK) {
        m_stats.tec = st.tx_error_counter;
        m_stats.rec = st.rx_error_counter;
        // ドライバ内キューの取りこぼしはここでしか観測できない
        m_stats.queueOverflow = st.rx_overrun_count;
    }

    // 1 秒あたりの受信フレーム数（SWR-92）
    if (m_lastFpsMs == 0 || static_cast<uint32_t>(nowMs - m_lastFpsMs) >= 1000) {
        m_stats.framesPerSec = m_stats.rxFrames - m_lastFpsFrames;
        m_lastFpsFrames      = m_stats.rxFrames;
        m_lastFpsMs          = nowMs;
    }

    switch (m_state) {
        case CanState::Running:
            if (st.state == TWAI_STATE_BUS_OFF) {
                ESP_LOGW(kTag, "bus-off detected");
                m_stats.busOffCount++;
                m_state       = CanState::BusOff;
                m_nextRetryMs = nowMs;
            }
            break;

        case CanState::BusOff:
            if (static_cast<int32_t>(nowMs - m_nextRetryMs) >= 0) {
                if (twai_initiate_recovery() == ESP_OK) {
                    m_state = CanState::Recovering;
                } else {
                    m_nextRetryMs = nowMs + kRetryMs;
                }
            }
            break;

        case CanState::Recovering:
            if (st.state == TWAI_STATE_STOPPED) {
                if (twai_start() == ESP_OK) {
                    m_stats.recoveryCount++;
                    m_failedRecovery = 0;
                    m_state          = CanState::Running;
                    ESP_LOGI(kTag, "bus recovered (%u)", m_stats.recoveryCount);
                } else {
                    if (m_failedRecovery < 255)
                        m_failedRecovery++;
                    m_state       = CanState::BusOff;
                    m_nextRetryMs = nowMs + (m_failedRecovery >= kBackoffAfter ? kBackoffMs : kRetryMs);
                }
            }
            break;

        case CanState::Stopped:
        default:
            break;
    }
}

}  // namespace cm
