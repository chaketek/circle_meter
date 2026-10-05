// SWA-03 / SWD-01: rusEFI CAN フレームのデコード
//
// 状態を持たない純粋関数のみ。HW 非依存（DEC-06 / SWR-10）。
#pragma once

#include <cstdint>
#include "signal_id.h"
#include "rusefi_can_spec.h"

namespace cm::rusefi {

struct DecodedSignal {
    SignalId id;
    float value;
};

/// 1 フレームから取り出せる信号の最大数（Sensors1 が 7 個で最大）。
constexpr uint8_t kMaxSignalsPerFrame = 8;

struct DecodeResult {
    bool accepted = false;  ///< このフレームを処理したか
    uint8_t count = 0;      ///< signals に格納された有効要素数
    DecodedSignal signals[kMaxSignalsPerFrame]{};
};

/// フレームをデコードする。
///
/// 事前条件: data != nullptr（nullptr の場合は accepted=false を返し、クラッシュしない）
/// 事後条件: dlc < kFrameDlc、または id がベース範囲外なら accepted=false, count=0
///
/// @param id      受信した CAN ID（標準/拡張いずれも生の値）
/// @param data    8 バイトのデータ
/// @param dlc     データ長
/// @param baseId  verboseCanBaseAddress（既定 0x200）
DecodeResult decodeFrame(uint32_t id, const uint8_t* data, uint8_t dlc, uint32_t baseId);

/// rusEFI WBO の StandardData / DiagData をデコードする（SWR-11 / DOC-13 §3.7）。
///   StandardData（0x190 + 2n）-> WboValid, WboTempC
///   DiagData    （0x191 + 2n）-> WboStatus
/// Version が kWboVersion 以外、DLC が 8 未満、ID が範囲外なら accepted=false。
/// @param wboIndex WBO の CAN インデックス n（既定 0）
DecodeResult decodeWboFrame(uint32_t id, const uint8_t* data, uint8_t dlc, uint8_t wboIndex = 0);

/// λ が表示可能な値か（SYS-42 / RSK-09）。
constexpr bool isLambdaValid(float lambda) {
    return lambda >= kLambdaMinValid && lambda <= kLambdaMaxValid;
}

/// EGT が表示可能な値か（RSK-09）。
constexpr bool isEgtValid(float egtC) {
    return egtC >= kEgtMinValidC && egtC <= kEgtMaxValidC;
}

}  // namespace cm::rusefi
