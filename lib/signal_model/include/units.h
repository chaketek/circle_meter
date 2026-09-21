// SWA-05 / SWD-03: 単位換算・ゾーン判定・平滑化
//
// HW 非依存（DEC-06 / SWR-10）。
#pragma once

#include <cstdint>

namespace cm {

// ---------------------------------------------------------------- λ ゾーン
enum class LambdaZone : uint8_t { RichHeavy = 0, Rich, Optimal, Lean, LeanHeavy };

/// ゾーン境界。区間は「下限 < λ <= 上限」の半開区間（SWD-03 / UT-05）。
///   λ <= richHeavyMax          -> RichHeavy
///   richHeavyMax < λ <= richMax-> Rich
///   richMax     < λ <= optMax  -> Optimal
///   optMax      < λ <= leanMax -> Lean
///   leanMax     < λ            -> LeanHeavy
struct LambdaZoneConfig {
    float richHeavyMax;  ///< 既定 0.75
    float richMax;       ///< 既定 0.85
    float optimalMax;    ///< 既定 1.03
    float leanMax;       ///< 既定 1.10
};

/// λ を 1e-4 単位の整数で表した固定小数点値。rusEFI の raw 値と同じスケール。
/// float の丸め誤差で境界判定が揺れるのを避けるため、比較はこの型で行う（SWD-01 §1.3）。
using LambdaQ4 = int32_t;

constexpr LambdaQ4 toQ4(float lambda) {
    return static_cast<LambdaQ4>(lambda * 10000.0f + (lambda >= 0.0f ? 0.5f : -0.5f));
}

constexpr float fromQ4(LambdaQ4 v) {
    return static_cast<float>(v) * 0.0001f;
}

/// 固定小数点版のゾーン判定。これが唯一の実装であり、float 版は本関数に委譲する。
constexpr LambdaZone zoneOfQ4(LambdaQ4 v, const LambdaZoneConfig& cfg) {
    if (v <= toQ4(cfg.richHeavyMax)) return LambdaZone::RichHeavy;
    if (v <= toQ4(cfg.richMax))      return LambdaZone::Rich;
    if (v <= toQ4(cfg.optimalMax))   return LambdaZone::Optimal;
    if (v <= toQ4(cfg.leanMax))      return LambdaZone::Lean;
    return LambdaZone::LeanHeavy;
}

constexpr LambdaZone zoneOf(float lambda, const LambdaZoneConfig& cfg) {
    return zoneOfQ4(toQ4(lambda), cfg);
}

// ---------------------------------------------------------------- EGT 警告
enum class EgtLevel : uint8_t { Normal = 0, Warn, Danger };

struct EgtConfig {
    uint16_t warnC;    ///< 既定 850
    uint16_t dangerC;  ///< 既定 920
};

/// EGT の警告レベル。閾値「以上」で次のレベルに入る（UT-07: 849=Normal, 850=Warn）。
constexpr EgtLevel levelOf(float egtC, const EgtConfig& cfg) {
    if (egtC >= static_cast<float>(cfg.dangerC)) return EgtLevel::Danger;
    if (egtC >= static_cast<float>(cfg.warnC))   return EgtLevel::Warn;
    return EgtLevel::Normal;
}

// ---------------------------------------------------------------- λ / AFR
constexpr float kStoichGasoline = 14.70f;

constexpr float lambdaToAfr(float lambda, float stoich) {
    return lambda * stoich;
}

constexpr float afrToLambda(float afr, float stoich) {
    return (stoich > 0.0f) ? afr / stoich : 0.0f;
}

// ---------------------------------------------------------------- リング比率
/// λ をリングの塗り比率 0.0-1.0 に変換する。範囲外はクランプする（DOC-23 §3.1）。
constexpr float ringRatio(float lambda, float rangeLo, float rangeHi) {
    if (rangeHi <= rangeLo) return 0.0f;
    const float r = (lambda - rangeLo) / (rangeHi - rangeLo);
    if (r < 0.0f) return 0.0f;
    if (r > 1.0f) return 1.0f;
    return r;
}

// ---------------------------------------------------------------- 1 次ローパス
/// SWR-24: 表示のちらつきを抑える 1 次 IIR。
/// 入力が無効になったら必ず reset() すること（古い値から緩やかに追従する挙動を防ぐ）。
class Lpf1 {
public:
    void configure(float timeConstantMs) {
        m_tauMs = (timeConstantMs > 0.0f) ? timeConstantMs : 0.0f;
    }

    void reset(float value) {
        m_value       = value;
        m_initialized = true;
    }

    bool initialized() const { return m_initialized; }
    float value() const { return m_value; }

    /// @param dtMs 前回呼び出しからの経過時間。0 のときは前回値をそのまま返す。
    float update(float input, uint32_t dtMs) {
        if (!m_initialized) {
            reset(input);
            return m_value;
        }
        if (dtMs == 0 || m_tauMs <= 0.0f) {
            if (m_tauMs <= 0.0f) m_value = input;
            return m_value;
        }
        // alpha = dt / (tau + dt)  （オイラー近似。dt >> tau でも 1.0 を超えない）
        const float dt    = static_cast<float>(dtMs);
        const float alpha = dt / (m_tauMs + dt);
        m_value += alpha * (input - m_value);
        return m_value;
    }

private:
    float m_tauMs       = 0.0f;
    float m_value       = 0.0f;
    bool  m_initialized = false;
};

}  // namespace cm
