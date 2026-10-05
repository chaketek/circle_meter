// SWD-13 / SWR-29: λ センサの状態の判定（HW / LVGL 非依存）
//
// CAN は届いているのに λ が無効なとき、「NO SIGNAL」（通信異常）と区別して、センサが停止中なのか
// 加熱中なのか故障なのかを表示するための判定（SYS-22）。情報源は 2 つ:
//   - ECU の verbose broadcast: 0x207 の λ = 0（無効）と、0x200 の O2 ヒータ許可ビット（DOC-13 §3.1.1 /
//   §3.6）
//   - rusEFI WBO 自身のフレーム: 状態（Preheat / Warmup / Running / 故障）とセンサ温度（DOC-13 §3.7）
// WBO のフレームが届いていればそちらを優先する（状態の種類と温度まで分かるため）。
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
};

struct LambdaSensorInfo {
    LambdaSensorState state = LambdaSensorState::NoSignal;
    bool hasTemp            = false;  ///< WBO の温度があるか
    float tempC             = 0.0f;
};

/// SWR-29 の優先順位で判定する（UT-23）。
inline LambdaSensorInfo classifyLambdaSensor(const Snapshot& snap) {
    LambdaSensorInfo info;

    // (1) 0x207 そのものが届いていない -> 通信異常
    float lambdaValid = 0.0f;
    if (!snap.get(SignalId::Lambda1Valid, lambdaValid)) {
        info.state = LambdaSensorState::NoSignal;
        return info;
    }

    // (2) ECU が有効な λ を送っている
    float lambda = 0.0f;
    if (lambdaValid > 0.5f && snap.get(SignalId::Lambda1, lambda)) {
        info.state = LambdaSensorState::Ok;
        return info;
    }

    // (3) WBO の状態が届いていれば、そちらで理由を出す
    float temp = 0.0f;
    if (snap.get(SignalId::WboTempC, temp)) {
        info.hasTemp = true;
        info.tempC   = temp;
    }
    float status = 0.0f;
    if (snap.get(SignalId::WboStatus, status)) {
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

    // (4) WBO が無い（または届いていない）: ECU のヒータ許可ビットで判断する。温度は出さない
    info.hasTemp  = false;
    bool heaterOn = false;
    if (snap.statusBit(status_bit::kO2Heater, heaterOn) && heaterOn) {
        info.state = LambdaSensorState::WarmingUp;
    } else {
        info.state = LambdaSensorState::SensorOff;
    }
    return info;
}

}  // namespace cm
