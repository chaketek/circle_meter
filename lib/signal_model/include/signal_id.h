// SWA-04 / SWD-02: 信号識別子
//
// 本ヘッダは HW 非依存（DEC-06 / SWR-10）。Arduino.h / esp_* / lvgl.h を include しないこと。
// 静的チェック S4 (DOC-41 §3.1) で検査される。
#pragma once

#include <cstddef>
#include <cstdint>

namespace cm {

/// rusEFI から取得する信号の識別子。
/// 追加する場合は必ず COUNT の直前に追加すること（NVS 互換性のため値を詰め替えない）。
enum class SignalId : uint8_t {
    Lambda1 = 0,
    Lambda2,
    Egt1,
    Egt2,
    Rpm,
    IgnitionTiming,
    InjDuty,
    IgnDuty,
    VehicleSpeed,
    Map,
    Clt,
    Iat,
    FuelLevel,
    OilPressure,
    OilTemp,
    FuelTemp,
    BattVolt,
    StatusFlags,
    Gear,
    // SYS-22 / SWR-29（2026-10-05 追加。NVS 互換のため末尾に足す）
    Lambda1Valid,  ///< 0x207 を受けるたびに 1（λ 有効）/ 0（ECU が λ = 0 を送った）
    WboValid,      ///< rusEFI WBO StandardData の Valid
    WboTempC,      ///< rusEFI WBO のセンサ温度 [°C]
    WboStatus,     ///< rusEFI WBO の状態（rusefi::WboStatus の値）
    COUNT
};

constexpr size_t kSignalCount = static_cast<size_t>(SignalId::COUNT);

/// SignalId を配列添字に変換する。
constexpr size_t idx(SignalId id) {
    return static_cast<size_t>(id);
}

/// `StatusFlags` に詰め込まれるビット位置（DOC-13 §3.6）。
namespace status_bit {
constexpr uint32_t kRevLimit      = 1u << 0;
constexpr uint32_t kMainRelay     = 1u << 1;
constexpr uint32_t kFuelPump      = 1u << 2;
constexpr uint32_t kCheckEngine   = 1u << 3;
constexpr uint32_t kO2Heater      = 1u << 4;
constexpr uint32_t kLambdaProtect = 1u << 5;
constexpr uint32_t kFan1          = 1u << 6;
constexpr uint32_t kFan2          = 1u << 7;
}  // namespace status_bit

}  // namespace cm
