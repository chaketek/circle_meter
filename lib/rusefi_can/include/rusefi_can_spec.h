// SWA-03: rusEFI verbose broadcast のフレーム定義とスケーリング定数
//
// 一次情報源:
//   rusefi/firmware/controllers/can/can_verbose.cpp
//   rusefi/firmware/controllers/can/rusEFI_CAN_verbose.dbc
//   参照時点: master (2026-09)
//   ※ rusEFI 側の仕様が変わった場合、本ヘッダと DOC-13 を同時に更新すること (RSK-08)
//
// 仕様の詳細は docs/13_ICD_rusefi_can.md を参照。
// スケーリング値をコード中に直書きしないこと（静的チェック S5 / DOC-41 §3.1）。
#pragma once

#include <cstdint>

namespace cm::rusefi {

// ---------------------------------------------------------------- フレーム
/// verboseCanBaseAddress の既定値。設定で変更可能（SYS-02）。
constexpr uint32_t kDefaultBaseId = 0x200;

/// ベース ID からのオフセット。sendCanVerbose() の送信順に対応する。
enum FrameOffset : uint8_t {
    kOffStatus      = 0,   // BASE+0  警告・フラグ・ギア・積算距離
    kOffSpeeds      = 1,   // BASE+1  RPM・点火時期・デューティ・車速
    kOffPedalTps    = 2,   // BASE+2  ペダル・スロットル
    kOffSensors1    = 3,   // BASE+3  MAP・水温・吸気温・燃料残量
    kOffSensors2    = 4,   // BASE+4  油圧・油温・燃温・バッテリ電圧
    kOffFueling     = 5,   // BASE+5  （未使用）
    kOffFueling2    = 6,   // BASE+6  （未使用）
    kOffFueling3    = 7,   // BASE+7  ★ラムダ
    kOffCams        = 8,   // BASE+8  （未使用）
    kOffEgts        = 9,   // BASE+9  ★排気温度
    kOffKnock       = 10,  // BASE+10 （未使用）
    kOffStatus11    = 11,  // BASE+11 （未使用）
    kOffCount       = 12
};

/// 全フレーム共通の DLC。これ未満は不正フレームとして破棄する（SWR-06）。
constexpr uint8_t kFrameDlc = 8;

// ---------------------------------------------------------------- スケーリング
// DOC-13 §3 の「係数」「オフセット」に対応。
constexpr float kLambdaScale     = 0.0001f;      // uint16 LE
constexpr float kEgtScaleC       = 5.0f;         // uint8, 5 degC/LSB
constexpr float kTimingScaleDeg  = 0.02f;        // int16 LE
constexpr float kDutyScalePct    = 0.5f;         // uint8
constexpr float kMapScaleKpa     = 1.0f / 30.0f; // uint16 LE
constexpr float kOilPressScaleKpa= 1.0f / 30.0f; // uint16 LE
constexpr float kBattScaleV      = 0.001f;       // uint16 LE (mV)
constexpr float kFuelLevelScale  = 0.5f;         // uint8
constexpr float kTempOffsetC     = -40.0f;       // uint8 の温度はすべて -40 オフセット

// ---------------------------------------------------------------- 妥当性判定
// rusEFI は未構成センサに対して 0 を送る（Sensor::getOrZero）。
// 0 をそのまま表示すると「非常に濃い」「冷えている」と誤読されるため無効値として扱う。
constexpr float kLambdaMinValid = 0.30f;   // RSK-09 / SYS-42
constexpr float kLambdaMaxValid = 5.00f;
constexpr float kEgtMinValidC   = 1.0f;    // 0 degC は無効（EGT1 未構成）
constexpr float kEgtMaxValidC   = 1275.0f; // uint8 * 5 の上限

// ---------------------------------------------------------------- 小ヘルパ
/// リトルエンディアンの uint16 を取り出す。
constexpr uint16_t le16(const uint8_t* d, uint8_t off) {
    return static_cast<uint16_t>(d[off]) | static_cast<uint16_t>(static_cast<uint16_t>(d[off + 1]) << 8);
}

/// リトルエンディアンの int16 を取り出す。
constexpr int16_t le16s(const uint8_t* d, uint8_t off) {
    return static_cast<int16_t>(le16(d, off));
}

}  // namespace cm::rusefi
