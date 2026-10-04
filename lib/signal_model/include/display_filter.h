// SWR-24 / SWR-49 / RSK-01: 表示用の値の取り出し（HW / LVGL 非依存）
//
// λ ページの全デザイン（リング / 指針式）で共通の判断をここに集める:
//   - SignalStore のスナップショットから λ / EGT を取り出し、鮮度（Fresh / Stale / Lost）を付ける
//   - 表示用の 1 次 IIR（SWR-24: λ 80 ms / EGT 200 ms）を掛ける
//   - Lost になったら LPF の状態を捨て、復帰した最初の値で初期化する（古い値から針が泳いでこない）
// デザインごとに書くと、喪失時の扱い（RSK-01）がデザインによって食い違う危険があるため。
#pragma once

#include <cstdint>

#include "config.h"
#include "signal_store.h"
#include "units.h"

namespace cm {

struct DisplayValues {
    // λ
    bool hasLambda     = false;  ///< false = Lost。lambda は使ってはならない
    bool lambdaStale   = false;
    float lambda       = 0.0f;  ///< LPF 後
    LambdaZone zone    = LambdaZone::Optimal;
    float ringRatio    = 0.0f;  ///< 0.0-1.0（リング / 針の位置）
    Freshness lambdaFr = Freshness::Lost;
    // EGT
    bool hasEgt       = false;
    bool egtStale     = false;
    float egtC        = 0.0f;  ///< LPF 後
    EgtLevel egtLevel = EgtLevel::Normal;
};

class DisplayFilter {
public:
    static constexpr float kLambdaTauMs = 80.0f;   ///< SWR-24
    static constexpr float kEgtTauMs    = 200.0f;  ///< SWR-24

    DisplayFilter() {
        m_lpfLambda.configure(kLambdaTauMs);
        m_lpfEgt.configure(kEgtTauMs);
    }

    /// UI の 1 フレームごとに呼ぶ。dt はスナップショットの時刻差から求める。
    DisplayValues update(const Snapshot& snap, const Config& cfg) {
        const uint32_t dtMs = m_hasLast ? snap.takenAtMs - m_lastMs : 0;
        m_lastMs            = snap.takenAtMs;
        m_hasLast           = true;

        DisplayValues v;
        float lambda  = 0.0f;
        v.hasLambda   = snap.get(SignalId::Lambda1, lambda);  // Lost なら false（RSK-01）
        v.lambdaFr    = snap.freshnessOf(SignalId::Lambda1);
        v.lambdaStale = (v.lambdaFr == Freshness::Stale);
        if (v.hasLambda) {
            if (!m_lambdaSeeded) {
                m_lpfLambda.reset(lambda);
                m_lambdaSeeded = true;
            }
            v.lambda    = m_lpfLambda.update(lambda, dtMs);
            v.zone      = zoneOf(v.lambda, cfg.zones);
            v.ringRatio = cm::ringRatio(v.lambda, cfg.ringLo, cfg.ringHi);
        } else {
            m_lambdaSeeded = false;
        }

        float egtC = 0.0f;
        v.hasEgt   = snap.get(SignalId::Egt1, egtC);
        v.egtStale = (snap.freshnessOf(SignalId::Egt1) == Freshness::Stale);
        if (v.hasEgt) {
            if (!m_egtSeeded) {
                m_lpfEgt.reset(egtC);
                m_egtSeeded = true;
            }
            v.egtC     = m_lpfEgt.update(egtC, dtMs);
            v.egtLevel = levelOf(v.egtC, cfg.egt);
        } else {
            m_egtSeeded = false;
        }
        return v;
    }

private:
    Lpf1 m_lpfLambda;
    Lpf1 m_lpfEgt;
    bool m_lambdaSeeded = false;
    bool m_egtSeeded    = false;
    uint32_t m_lastMs   = 0;
    bool m_hasLast      = false;
};

}  // namespace cm
