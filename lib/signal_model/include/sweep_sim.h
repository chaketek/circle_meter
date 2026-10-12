// SWR-100: CAN の代わりに λ / EGT を往復スイープさせるデモ用の信号生成（HW 非依存）
//
// 物理層（CAN・PCAN）を繋がずに、UI の全ゾーン・EGT の警告域・描画 fps を目で確認するために使う。
// 値は三角波で、λ がレンジの下端から上端へ、また下端へと往復する。
// EGT は λ に比例させ、上端で DANGER（既定 920 °C 以上）まで届くようにしてある。
// 実機の EGT は rusEFI が 1 バイト × 5 °C で送る（DOC-13）ので、5 °C 刻みに量子化する。
// 刻みを 1 °C にすると毎フレーム文字が変わり、実機より重い描画負荷を測ってしまう。
#pragma once

#include <cstdint>

namespace cm {

struct SweepOutput {
    float lambda = 0.0f;
    float egtC   = 0.0f;
    float rpm    = 0.0f;
};

class SweepSim {
public:
    static constexpr float kLambdaLo    = 0.68f;   ///< DOC-23 §3.1 の表示レンジ下端（AFR 10.0）
    static constexpr float kLambdaHi    = 1.36f;   ///< 同 上端（AFR 20.0）
    static constexpr float kEgtLoC      = 300.0f;  ///< λ 下端での EGT
    static constexpr float kEgtHiC      = 960.0f;  ///< λ 上端での EGT（DANGER 920 を超える）
    static constexpr float kRpmLo       = 1200.0f;
    static constexpr float kRpmHi       = 7200.0f;
    static constexpr uint32_t kOneWayMs = 8500;  ///< 下端 -> 上端の所要時間（旧 m5dial_sim と同じ）
    static constexpr float kEgtStepC    = 5.0f;  ///< rusEFI の EGT 分解能（DOC-13）

    /// 位置（0.0 = 下端, 1.0 = 上端）を直接指定する。停止して各ゾーンの見え方を確認するための操作。
    void seek(float pos01) { m_pos = (pos01 < 0.0f) ? 0.0f : (pos01 > 1.0f ? 1.0f : pos01); }

    /// @param dtMs 前回呼び出しからの経過時間。0 なら進めずに現在値を返す（停止中の再配信に使う）
    SweepOutput step(uint32_t dtMs) {
        m_pos += m_dir * static_cast<float>(dtMs) / static_cast<float>(kOneWayMs);
        // 端で折り返す。dt が大きくて 1 往復以上進んでも破綻しないよう、ループで畳む
        while (m_pos > 1.0f || m_pos < 0.0f) {
            if (m_pos > 1.0f) {
                m_pos = 2.0f - m_pos;
                m_dir = -1.0f;
            } else {
                m_pos = -m_pos;
                m_dir = 1.0f;
            }
        }
        SweepOutput o;
        o.lambda        = kLambdaLo + (kLambdaHi - kLambdaLo) * m_pos;
        const float egt = kEgtLoC + (kEgtHiC - kEgtLoC) * m_pos;
        o.egtC          = static_cast<float>(static_cast<int32_t>(egt / kEgtStepC + 0.5f)) * kEgtStepC;
        o.rpm           = kRpmLo + (kRpmHi - kRpmLo) * m_pos;
        return o;
    }

private:
    float m_pos = 0.0f;  ///< 0.0（下端）- 1.0（上端）
    float m_dir = 1.0f;
};

}  // namespace cm
