// SWA-10 AppController
//
// フェーズ P1/P2（DOC-40 §7）のブリングアップ用ファームウェア。
//   - CAN を Listen Only で開始し、rusEFI の verbose broadcast を受信する
//   - λ / EGT / 状態をシリアルと画面に最小限のテキストで出す
//   - OPN-02（PORT.B の GPIO 割当・TX/RX の向き）を実測で確定させるのが目的
//
// LVGL によるページ実装（SWA-11/20/21/22）はフェーズ P4 で追加する。
#include <M5Dial.h>

#include "can_driver.h"
#include "config.h"
#include "rusefi_decoder.h"
#include "signal_store.h"
#include "units.h"

#ifndef CM_FW_VERSION
#define CM_FW_VERSION "unknown"
#endif

using namespace cm;

namespace {

Config      g_cfg;
SignalStore g_store;
CanDriver   g_can;

constexpr uint32_t kUiPeriodMs = 33;  // 約 30fps（SYS-12）

// ---------------------------------------------------------------- CAN 受信タスク
// SWA-02 CanRxTask: Core 0 に固定する（DEC-03）
void canRxTask(void*) {
#ifdef CM_ENABLE_CAN_SIM
    // SWR-100: 実 CAN の代わりに λ / EGT をスイープする
    float    lambda = 0.68f;
    float    egtC   = 300.0f;
    int8_t   dir    = 1;
    for (;;) {
        const uint32_t now = millis();
        lambda += 0.004f * dir;
        if (lambda > 1.36f || lambda < 0.68f) dir = -dir;
        egtC = 300.0f + (lambda - 0.68f) * 900.0f;

        g_store.update(SignalId::Lambda1, lambda, now);
        g_store.update(SignalId::Egt1, egtC, now);
        g_store.update(SignalId::Rpm, 1200.0f + (lambda - 0.68f) * 6000.0f, now);
        g_store.update(SignalId::Clt, 85.0f, now);
        g_store.update(SignalId::BattVolt, 13.8f, now);
        g_can.mutableStats().rxFrames++;
        vTaskDelay(pdMS_TO_TICKS(50));
    }
#else
    twai_message_t msg;
    for (;;) {
        if (!g_can.receive(msg, 100)) {
            continue;
        }
        const uint32_t now = millis();
        const auto     r =
            rusefi::decodeFrame(msg.identifier, msg.data, msg.data_length_code, g_cfg.canBaseId);

        if (!r.accepted) {
            if (msg.data_length_code < rusefi::kFrameDlc) {
                g_can.mutableStats().badDlc++;
            } else {
                g_can.mutableStats().unknownId++;
            }
            continue;
        }
        g_can.mutableStats().rxFrames++;

        for (uint8_t i = 0; i < r.count; ++i) {
            const SignalId id = r.signals[i].id;
            const float    v  = r.signals[i].value;

            // SYS-42 / RSK-09: rusEFI が未構成センサに送る 0 をストアへ入れない。
            // 入れなければ鮮度が Fresh にならず、UI は自動的に "--" を出す。
            if ((id == SignalId::Lambda1 || id == SignalId::Lambda2) && !rusefi::isLambdaValid(v)) {
                continue;
            }
            if ((id == SignalId::Egt1 || id == SignalId::Egt2) && !rusefi::isEgtValid(v)) {
                continue;
            }
            g_store.update(id, v, now);
        }
    }
#endif
}

// ---------------------------------------------------------------- CAN 健全性タスク
void canHealthTask(void*) {
    for (;;) {
        g_can.poll(millis());
        vTaskDelay(pdMS_TO_TICKS(1000));
    }
}

// ---------------------------------------------------------------- 暫定 UI
const char* freshnessLabel(Freshness f) {
    switch (f) {
        case Freshness::Fresh: return "OK";
        case Freshness::Stale: return "STALE";
        default:               return "LOST";
    }
}

uint16_t zoneColor(LambdaZone z) {
    // DOC-23 §8 のパレット（P4 で Theme に移す）
    switch (z) {
        case LambdaZone::RichHeavy: return M5Dial.Display.color565(0x2E, 0x7D, 0xFF);
        case LambdaZone::Rich:      return M5Dial.Display.color565(0x00, 0xC8, 0xD7);
        case LambdaZone::Optimal:   return M5Dial.Display.color565(0x17, 0xD1, 0x4B);
        case LambdaZone::Lean:      return M5Dial.Display.color565(0xFF, 0xC4, 0x00);
        default:                    return M5Dial.Display.color565(0xFF, 0x2D, 0x2D);
    }
}

void drawBringupScreen(const Snapshot& snap) {
    auto& d = M5Dial.Display;
    d.startWrite();
    d.fillScreen(TFT_BLACK);

    float lambda = 0.0f;
    const bool hasLambda = snap.get(SignalId::Lambda1, lambda);
    const auto zone      = zoneOf(lambda, g_cfg.zones);

    d.setTextDatum(middle_center);
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.setTextSize(1);
    d.drawString(g_cfg.showAfr ? "AFR" : "LAMBDA", 120, 62);

    char buf[16];
    if (hasLambda) {
        if (g_cfg.showAfr) {
            snprintf(buf, sizeof(buf), "%.1f", lambdaToAfr(lambda, g_cfg.stoich));
        } else {
            snprintf(buf, sizeof(buf), "%.3f", lambda);
        }
        d.setTextColor(zoneColor(zone), TFT_BLACK);
    } else {
        snprintf(buf, sizeof(buf), "--");
        d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    }
    d.setTextSize(4);
    d.drawString(buf, 120, 105);

    // 外周バーの簡易版（P4 で LambdaRing に置き換える / SWD-08）
    if (hasLambda) {
        const float ratio = ringRatio(lambda, g_cfg.ringLo, g_cfg.ringHi);
        const int   sweep = static_cast<int>(ratio * 270.0f);
        d.fillArc(120, 120, 96, 118, 135, 135 + 270, d.color565(0x1A, 0x1A, 0x1A));
        if (sweep > 0) {
            d.fillArc(120, 120, 96, 118, 135, 135 + sweep, zoneColor(zone));
        }
    }

    float egtC = 0.0f;
    if (snap.get(SignalId::Egt1, egtC)) {
        const auto lv = levelOf(egtC, g_cfg.egt);
        d.setTextColor(lv == EgtLevel::Danger   ? d.color565(0xFF, 0x2D, 0x2D)
                       : lv == EgtLevel::Warn   ? d.color565(0xFF, 0xC4, 0x00)
                                                : TFT_LIGHTGREY,
                       TFT_BLACK);
        snprintf(buf, sizeof(buf), "%dC", static_cast<int>(egtC + 0.5f));
    } else {
        d.setTextColor(TFT_DARKGREY, TFT_BLACK);
        snprintf(buf, sizeof(buf), "--C");
    }
    d.setTextSize(2);
    d.drawString(buf, 120, 158);

    // ブリングアップ用ステータス（P2 で OPN-02 を潰すための情報）
    d.setTextSize(1);
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    snprintf(buf, sizeof(buf), "%lu f/s", static_cast<unsigned long>(g_can.stats().framesPerSec));
    d.drawString(buf, 120, 182);
    d.drawString(freshnessLabel(snap.freshnessOf(SignalId::Lambda1)), 120, 196);

    d.endWrite();
}

}  // namespace

void setup() {
    auto cfg = M5.config();
    M5Dial.begin(cfg, /*enableEncoder=*/true, /*enableRFID=*/false);

    M5Dial.Display.setBrightness(0);  // DOC-23 §7: 初回描画までバックライトを落とす
    M5Dial.Display.fillScreen(TFT_BLACK);

    Serial.begin(115200);
    delay(50);
    Serial.printf("\ncircle_meter %s\n", CM_FW_VERSION);

    // SWD-09: 設定 -> CAN 開始 -> 受信タスク -> 画面、の順。
    // SYS-18 によりスプラッシュ表示中も受信していなければならない。
    g_cfg = defaultConfig();

#ifndef CM_ENABLE_CAN_SIM
    if (!g_can.begin(g_cfg)) {
        Serial.println("CAN init FAILED");
    }
#endif

    xTaskCreatePinnedToCore(canRxTask, "can_rx", 4096, nullptr, 10, nullptr, 0);
    xTaskCreatePinnedToCore(canHealthTask, "can_health", 3072, nullptr, 5, nullptr, 0);

    M5Dial.Display.setBrightness(200);
}

void loop() {
    static uint32_t nextUiMs  = 0;
    static uint32_t nextLogMs = 0;

    M5Dial.update();
    const uint32_t now = millis();

    if (static_cast<int32_t>(now - nextUiMs) >= 0) {
        nextUiMs = now + kUiPeriodMs;
        drawBringupScreen(g_store.snapshot(now));
    }

    // SYS-31: ボタン短押しで AFR / λ 表示を切り替える
    if (M5Dial.BtnA.wasClicked()) {
        g_cfg.showAfr = !g_cfg.showAfr;
    }

    if (static_cast<int32_t>(now - nextLogMs) >= 0) {
        nextLogMs        = now + 1000;
        const auto& s    = g_can.stats();
        const auto  snap = g_store.snapshot(now);
        float       lam = 0.0f, egt = 0.0f;
        const bool  hasLam = snap.get(SignalId::Lambda1, lam);
        const bool  hasEgt = snap.get(SignalId::Egt1, egt);
        Serial.printf("rx=%lu f/s=%lu unk=%lu dlc=%lu ovf=%lu tec=%lu rec=%lu | lam=%s%.3f egt=%s%.0f\n",
                      (unsigned long)s.rxFrames, (unsigned long)s.framesPerSec,
                      (unsigned long)s.unknownId, (unsigned long)s.badDlc,
                      (unsigned long)s.queueOverflow, (unsigned long)s.tec, (unsigned long)s.rec,
                      hasLam ? "" : "(none)", lam, hasEgt ? "" : "(none)", egt);
    }

    delay(2);
}
