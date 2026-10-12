// SWD-13 / SWR-29: λ センサの状態の判定（HW / LVGL 非依存）
//
// λ が表示できないとき、その理由（通信なし / WBO だけ届かない / 停止中 / 加熱中 /
// 故障）を出すための判定（SYS-22）。 λ・状態・温度はすべて rusEFI WBO のフレームから取る（SYS-03 /
// DEC-11。2026-10-05 改訂。当初は ECU の 0x207）。 ECU のフレーム（StatusFlags）は「WBO
// だけが届かない」のか「通信全体が無い」のかを見分けるためだけに使う。
#pragma once

#include <cstdint>

#include "signal_store.h"

namespace cm {

enum class LambdaSensorState : uint8_t {
    Ok = 0,          ///< λ 有効。通常表示
    NoSignal,        ///< 0x207 が届いていない（通信異常）
    SensorOff,       ///< 停止中（エンジン停止でヒータ許可なし / WBO Preheat）
    WarmingUp,       ///< 加熱中
    Check,           ///< WBO は正常（閉ループ）だが λ が無効
    FaultNoHeat,     ///< WBO: SensorDidntHeat
    FaultOverheat,   ///< WBO: SensorOverheat
    FaultUnderheat,  ///< WBO: SensorUnderheat
    NoWbo,           ///< ECU は届くが WBO のフレームが届かない
};

struct LambdaSensorInfo {
    LambdaSensorState state = LambdaSensorState::NoSignal;
    bool hasTemp            = false;  ///< WBO の温度があるか
    float tempC             = 0.0f;
};

/// SWR-29 の優先順位で判定する（UT-23）。
inline LambdaSensorInfo classifyLambdaSensor(const Snapshot& snap) {
    LambdaSensorInfo info;

    // (1) WBO の StandardData が届いていない。ECU が届いていれば WBO だけの問題
    float wboValid = 0.0f;
    if (!snap.get(SignalId::WboValid, wboValid)) {
        float flags = 0.0f;
        info.state =
            snap.get(SignalId::StatusFlags, flags) ? LambdaSensorState::NoWbo : LambdaSensorState::NoSignal;
        return info;
    }

    // (2) WBO が有効な λ を送っている
    float lambda = 0.0f;
    if (wboValid > 0.5f && snap.get(SignalId::WboLambda, lambda)) {
        info.state = LambdaSensorState::Ok;
        return info;
    }

    // (3) λ が無効な理由を WBO の状態と温度で出す
    float temp = 0.0f;
    if (snap.get(SignalId::WboTempC, temp)) {
        info.hasTemp = true;
        info.tempC   = temp;
    }
    float status = 0.0f;
    if (!snap.get(SignalId::WboStatus, status)) {
        info.state = LambdaSensorState::Check;  // (4) DiagData だけが届かない
        return info;
    }
    switch (static_cast<int>(status + 0.5f)) {
        case 0:
            info.state = LambdaSensorState::SensorOff;
            break;
        case 1:
            info.state = LambdaSensorState::WarmingUp;
            break;
        case 2:
            info.state = LambdaSensorState::Check;
            break;
        case 3:
            info.state = LambdaSensorState::FaultNoHeat;
            break;
        case 4:
            info.state = LambdaSensorState::FaultOverheat;
            break;
        default:
            info.state = LambdaSensorState::FaultUnderheat;
            break;
    }
    return info;
}

}  // namespace cm
