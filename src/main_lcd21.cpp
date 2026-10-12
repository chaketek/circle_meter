// SWA-10 AppController（Waveshare ESP32-S3-Touch-LCD-2.1 版）
//
// 起動順序は SWD-09:
//   設定 -> 表示 HAL（バックライト OFF）-> LVGL -> 全ページの onCreate -> 信号源（CAN またはシミュレータ）
//   -> スプラッシュ（バックライトのフェード）-> 本画面
//
// 信号源は 2 通り:
//   通常      : TWAI で rusEFI の verbose broadcast を受信する（GPIO20 TX / GPIO19 RX。OPN-13
//   は実機で未確認） CM_ENABLE_CAN_SIM : λ / EGT をスイープする（SWR-100）。物理層なしで画面を確認する
//
// この版では CAN への送出は一切行わない（CLAUDE.md 規則 1 / RSK-06）。
#include <Arduino.h>

#include <atomic>

#include "config.h"
#include "display_hal.h"
#include "display_policy.h"
#include "lvgl_port.h"
#include "page_lambda.h"
#include "page_splash.h"
#include "signal_store.h"
#include "sweep_sim.h"

#ifndef CM_ENABLE_CAN_SIM
#include "can_driver.h"
#include "can_rx_task.h"
#endif

#ifndef CM_FW_VERSION
#define CM_FW_VERSION "unknown"
#endif

using namespace cm;

// LVGL の描画は loopTask（Core 1）で行う。arc のマスク処理でスタックを使うので既定の 8 KB より増やす
SET_LOOP_TASK_STACK_SIZE(16 * 1024);

namespace {

Config g_cfg;
SignalStore g_store;
DisplayHal g_display;
ui::LvglPort g_port;
ui::PageSplash g_splash;
ui::PageLambda g_lambda;

#ifndef CM_ENABLE_CAN_SIM
CanDriver g_can;
#endif

// SYS-12 は 30 fps 以上。ちょうど 30 Hz で回すと、ゾーン境界を跨ぐ 1 フレームが 33 ms を超えた分だけ
// 平均が 30 を割る（実測 29.4 fps）。40 Hz で回して余裕を持たせる。LPF（SWR-24）は dt
// を受け取るので周期は自由
constexpr uint32_t kUiPeriodMs = 25;

// DOC-23 §7: 0.8 s フェードイン -> 1.6 s 保持 -> 0.6 s フェードアウト（受信済みなら保持 0.4 s）
constexpr uint32_t kSplashFadeInMs    = 800;
constexpr uint32_t kSplashHoldMs      = 1600;
constexpr uint32_t kSplashHoldShortMs = 400;
constexpr uint32_t kSplashFadeOutMs   = 600;
constexpr uint32_t kMainFadeInMs      = 200;

uint32_t nowMs() {
    return millis();
}
uint32_t nowUs() {
    return micros();
}

void lvglLog(lv_log_level_t, const char* buf) {
    Serial.print("[lvgl] ");
    Serial.print(buf);
}

// ---------------------------------------------------------------- 信号源
#ifdef CM_ENABLE_CAN_SIM
// デモの操作（UART0 から 1 文字）。タッチ操作（SWD-06）の実装前に、各状態を止めて見るためのもの。
//   h: 停止/再開   0-9: その位置（λ レンジ下端 0 〜 上端 9）で停止   m: AFR/λ 表示切替
std::atomic<bool> g_simHold{false};
std::atomic<float> g_simSeek{-1.0f};

// SWR-100: 実 CAN の代わりに λ / EGT をスイープする。20 Hz で SignalStore を更新する（rusEFI の 50 ms
// 周期相当）
void simTask(void*) {
    SweepSim sim;
    uint32_t last = millis();
    for (;;) {
        vTaskDelay(pdMS_TO_TICKS(50));
        const uint32_t now = millis();
        const float seek   = g_simSeek.exchange(-1.0f);
        if (seek >= 0.0f) {
            sim.seek(seek);
        }
        // 停止中も dt = 0 で現在値を配信し続ける（止めると鮮度が落ちて STALE/LOST になってしまう）
        const SweepOutput o = sim.step(g_simHold.load() ? 0 : now - last);
        last                = now;
        g_store.update(SignalId::Lambda1, o.lambda, now);
        g_store.update(SignalId::Egt1, o.egtC, now);
        g_store.update(SignalId::Rpm, o.rpm, now);
        g_store.update(SignalId::Clt, 85.0f, now);
        g_store.update(SignalId::BattVolt, 13.8f, now);
    }
}
#endif

bool signalSourceSeen() {
#ifdef CM_ENABLE_CAN_SIM
    return false;  // デモではスプラッシュを短縮しない（ロゴを見せる）
#else
    return g_can.stats().rxFrames > 0;
#endif
}

// ---------------------------------------------------------------- 表示ヘルパ
/// 一定時間、LVGL を回し続ける（フェード中も描画と flush を止めない）。
void pump(uint32_t durationMs) {
    const uint32_t t0 = millis();
    while (millis() - t0 < durationMs) {
        const uint32_t wait = g_port.service();
        delay(wait < 1 ? 1 : (wait > 8 ? 8 : wait));
    }
}

/// バックライトを PWM で滑らかに変える。画素に触らないので階調が崩れず、CPU もほぼ使わない（DOC-23 §7）。
void fadeBacklight(int fromPercent, int toPercent, uint32_t durationMs) {
    constexpr uint32_t kStepMs = 16;
    const uint32_t steps       = durationMs / kStepMs ? durationMs / kStepMs : 1;
    for (uint32_t i = 0; i <= steps; ++i) {
        const int v = fromPercent + (toPercent - fromPercent) * static_cast<int>(i) / static_cast<int>(steps);
        g_display.setBrightnessPercent(static_cast<uint8_t>(v));
        pump(kStepMs);
    }
}

#ifdef CM_ENABLE_CAN_SIM
void handleDebugKeys() {
    while (Serial.available() > 0) {
        const int c = Serial.read();
        if (c == 'h') {
            g_simHold = !g_simHold.load();
            Serial.printf("sim: %s\n", g_simHold.load() ? "HOLD" : "RUN");
        } else if (c >= '0' && c <= '9') {
            g_simSeek = static_cast<float>(c - '0') / 9.0f;
            g_simHold = true;
            Serial.printf("sim: hold at %c/9\n", c);
        } else if (c == 'm') {
            g_cfg.showAfr = !g_cfg.showAfr;
            Serial.printf("display: %s\n", g_cfg.showAfr ? "AFR" : "LAMBDA");
        }
    }
}
#endif

void printBanner() {
    Serial.printf("\ncircle_meter %s  (LCD-2.1 / LVGL %d.%d.%d)\n", CM_FW_VERSION, LVGL_VERSION_MAJOR,
                  LVGL_VERSION_MINOR, LVGL_VERSION_PATCH);
    Serial.printf("sdk %s / psram %u KB free / sram %u KB free\n", ESP.getSdkVersion(),
                  static_cast<unsigned>(ESP.getFreePsram() / 1024),
                  static_cast<unsigned>(ESP.getFreeHeap() / 1024));
#ifdef CM_ENABLE_CAN_SIM
    Serial.println("signal source: SIMULATOR (sweep)");
#else
    Serial.printf("signal source: CAN TX=GPIO%d RX=GPIO%d\n", CM_TWAI_TX_GPIO, CM_TWAI_RX_GPIO);
#endif
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(100);
    g_cfg = defaultConfig();
    printBanner();

    // バックライト OFF のまま初期化する（DOC-23 §7）
    if (!g_display.begin()) {
        Serial.println("display init FAILED");
        return;
    }
    if (!g_port.begin(g_display, nowMs, nowUs)) {
        Serial.println("lvgl init FAILED");
        return;
    }
    lv_log_register_print_cb(lvglLog);

    // 全ページのオブジェクトを起動時に作る。以降、生成・破棄はしない（CLAUDE.md 規則 5）
    g_splash.setVersion(CM_FW_VERSION);
    g_splash.onCreate(g_port.screen(), g_cfg);
    g_lambda.onCreate(g_port.screen(), g_cfg);
    g_lambda.onHide();
    g_splash.onShow();

    // SYS-18: スプラッシュ表示中も信号源は動いている
#ifdef CM_ENABLE_CAN_SIM
    xTaskCreatePinnedToCore(simTask, "sim", 4096, nullptr, 10, nullptr, 0);
#else
    if (!g_can.begin(g_cfg)) {
        Serial.println("CAN init FAILED");
    }
    startCanRxTask(g_can, g_store, g_cfg);
    startCanHealthTask(g_can);
#endif

    // スプラッシュを描き終えてからバックライトを上げる（起動時のちらつき対策）
    pump(100);
    const int target = brightnessPercent(g_cfg.brightness);
    fadeBacklight(0, target, kSplashFadeInMs);
    pump(signalSourceSeen() ? kSplashHoldShortMs : kSplashHoldMs);
    fadeBacklight(target, 0, kSplashFadeOutMs);

    // 本画面を最初の 1 フレームまで描いてからバックライトを戻す（黒画面の一瞬を見せない）
    g_splash.onHide();
    g_lambda.onShow();
    g_lambda.onUpdate(g_store.snapshot(millis()), g_cfg);
    pump(150);
    fadeBacklight(0, target, kMainFadeInMs);

    Serial.printf("ui ready: sram %u KB free / psram %u KB free\n",
                  static_cast<unsigned>(ESP.getFreeHeap() / 1024),
                  static_cast<unsigned>(ESP.getFreePsram() / 1024));
}

void loop() {
    static uint32_t nextUiMs   = 0;
    static uint32_t nextStatMs = 1000;

    const uint32_t now = millis();
    if (static_cast<int32_t>(now - nextUiMs) >= 0) {
        // 前回の予定時刻に足す（now + 周期 にすると待ちの丸め誤差が積もって 28 fps 前後になる）。
        // 大きく遅れたとき（スプラッシュ明け・長い処理の後）は追いつこうとせず、現在時刻から再開する
        nextUiMs += kUiPeriodMs;
        if (static_cast<int32_t>(now - nextUiMs) >= 0) {
            nextUiMs = now + kUiPeriodMs;
        }
        g_lambda.onUpdate(g_store.snapshot(now), g_cfg);
    }

#ifdef CM_ENABLE_CAN_SIM
    handleDebugKeys();
#endif
    const uint32_t wait = g_port.service();

    if (static_cast<int32_t>(now - nextStatMs) >= 0) {
        nextStatMs += 1000;
        const ui::RenderStats s = g_port.takeStats();
        lv_mem_monitor_t mon;
        lv_mem_monitor(&mon);
        const uint32_t perFrameUs = s.frames ? (s.renderUs + s.flushUs) / s.frames : 0;

        char diag[24];
        const unsigned fps = (s.frames > 99) ? 99u : static_cast<unsigned>(s.frames);
        const unsigned ms  = (perFrameUs / 1000 > 99) ? 99u : static_cast<unsigned>(perFrameUs / 1000);
        snprintf(diag, sizeof(diag), "%u fps  %u ms", fps, ms);
        g_lambda.setDiagText(diag);

        Serial.printf(
            "fps=%2u | per frame: render=%5u us flush=%5u us px=%6u (%u flush) | lvgl heap used=%u%% (max %u "
            "KB) "
            "| sram=%u KB psram=%u KB\n",
            static_cast<unsigned>(s.frames), static_cast<unsigned>(s.frames ? s.renderUs / s.frames : 0),
            static_cast<unsigned>(s.frames ? s.flushUs / s.frames : 0),
            static_cast<unsigned>(s.frames ? s.flushPixels / s.frames : 0),
            static_cast<unsigned>(s.frames ? s.flushes / s.frames : 0), static_cast<unsigned>(mon.used_pct),
            static_cast<unsigned>((mon.total_size - mon.free_biggest_size) / 1024),
            static_cast<unsigned>(ESP.getFreeHeap() / 1024),
            static_cast<unsigned>(ESP.getFreePsram() / 1024));
    }

    delay(wait < 1 ? 1 : (wait > 4 ? 4 : wait));
}
