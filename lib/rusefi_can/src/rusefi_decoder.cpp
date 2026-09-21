// SWA-03 / SWD-01 実装
#include "rusefi_decoder.h"

namespace cm::rusefi {
namespace {

inline void push(DecodeResult& r, SignalId id, float v) {
    if (r.count < kMaxSignalsPerFrame) {
        r.signals[r.count].id    = id;
        r.signals[r.count].value = v;
        ++r.count;
    }
}

/// uint8 の温度信号（-40 オフセット）
inline float temp8(uint8_t raw) {
    return static_cast<float>(raw) + kTempOffsetC;
}

void decodeStatus(DecodeResult& r, const uint8_t* d) {
    // DOC-13 §3.6: byte4 にビットフラグが並ぶ。ビット位置は signal_id.h の status_bit と一致させる。
    push(r, SignalId::StatusFlags, static_cast<float>(d[4]));
    push(r, SignalId::Gear, static_cast<float>(d[5]));
}

void decodeSpeeds(DecodeResult& r, const uint8_t* d) {
    push(r, SignalId::Rpm, static_cast<float>(le16(d, 0)));
    push(r, SignalId::IgnitionTiming, static_cast<float>(le16s(d, 2)) * kTimingScaleDeg);
    push(r, SignalId::InjDuty, static_cast<float>(d[4]) * kDutyScalePct);
    push(r, SignalId::IgnDuty, static_cast<float>(d[5]) * kDutyScalePct);
    push(r, SignalId::VehicleSpeed, static_cast<float>(d[6]));
}

void decodeSensors1(DecodeResult& r, const uint8_t* d) {
    push(r, SignalId::Map, static_cast<float>(le16(d, 0)) * kMapScaleKpa);
    push(r, SignalId::Clt, temp8(d[2]));
    push(r, SignalId::Iat, temp8(d[3]));
    push(r, SignalId::FuelLevel, static_cast<float>(d[7]) * kFuelLevelScale);
}

void decodeSensors2(DecodeResult& r, const uint8_t* d) {
    // byte0-1 はパディング（DOC-13 §3.5）
    push(r, SignalId::OilPressure, static_cast<float>(le16(d, 2)) * kOilPressScaleKpa);
    push(r, SignalId::OilTemp, temp8(d[4]));
    push(r, SignalId::FuelTemp, temp8(d[5]));
    push(r, SignalId::BattVolt, static_cast<float>(le16(d, 6)) * kBattScaleV);
}

void decodeFueling3(DecodeResult& r, const uint8_t* d) {
    push(r, SignalId::Lambda1, static_cast<float>(le16(d, 0)) * kLambdaScale);
    push(r, SignalId::Lambda2, static_cast<float>(le16(d, 2)) * kLambdaScale);
}

void decodeEgts(DecodeResult& r, const uint8_t* d) {
    // rusEFI 本体が送るのは Egt1/Egt2 のみ。Egt3-8 は常に 0 のため読まない。
    push(r, SignalId::Egt1, static_cast<float>(d[0]) * kEgtScaleC);
    push(r, SignalId::Egt2, static_cast<float>(d[1]) * kEgtScaleC);
}

}  // namespace

DecodeResult decodeFrame(uint32_t id, const uint8_t* data, uint8_t dlc, uint32_t baseId) {
    DecodeResult r;

    if (data == nullptr || dlc < kFrameDlc) {
        return r;  // SWR-06: 不正フレームは破棄
    }
    if (id < baseId || id >= baseId + kOffCount) {
        return r;  // SWR-04: ベース範囲外は無視
    }

    const uint8_t off = static_cast<uint8_t>(id - baseId);
    switch (off) {
        case kOffStatus:
            decodeStatus(r, data);
            break;
        case kOffSpeeds:
            decodeSpeeds(r, data);
            break;
        case kOffSensors1:
            decodeSensors1(r, data);
            break;
        case kOffSensors2:
            decodeSensors2(r, data);
            break;
        case kOffFueling3:
            decodeFueling3(r, data);
            break;
        case kOffEgts:
            decodeEgts(r, data);
            break;
        default:
            // 範囲内だが本機が使わないフレーム（PedalTps / Fueling / Cams / Knock / Status11）。
            // 「受理したが取り出す信号はない」として accepted=true, count=0 を返す。
            // これにより診断の unknownId カウンタが無用に増えない。
            break;
    }

    r.accepted = true;
    return r;
}

}  // namespace cm::rusefi
