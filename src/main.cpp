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
#include "logo.h"
#include "rusefi_decoder.h"
#include "signal_store.h"
#include "units.h"

#ifndef CM_FW_VERSION
#define CM_FW_VERSION "unknown"
#endif

using namespace cm;

namespace {

Config g_cfg;
SignalStore g_store;
CanDriver g_can;

// 暫定 UI 用のオフスクリーンバッファ。
// 直接ディスプレイに描くと全消去 -> 再描画の間が見えて明滅する。
// 1 フレーム分をまとめて転送することでティアリングと明滅を消す。
// 本番 UI (P4) では LVGL の部分バッファ 2 面に置き換える (DEC-05)。
M5Canvas g_canvas(&M5Dial.Display);
bool g_canvasReady = false;

constexpr uint32_t kUiPeriodMs = 33;  // 約 30fps（SYS-12）

// オープニング画面の時間（DOC-23 §7）。合計 3.0 s
constexpr uint32_t kSplashFadeInMs    = 800;
constexpr uint32_t kSplashHoldMs      = 1600;
constexpr uint32_t kSplashHoldShortMs = 400;  // CAN を受信済みのとき
constexpr uint32_t kSplashFadeOutMs   = 600;

// 円形画面のレイアウト（DOC-23 §2）
constexpr int kCx         = 120;
constexpr int kCy         = 120;
constexpr int kRingOuter  = 118;
constexpr int kRingInner  = 96;  // この内側が数値の描画領域
constexpr int kTextMargin = 2;   // リングに触れないための余白

// 縦方向のバンド割り。各要素が重ならないよう高さ上限を決めておく。
constexpr int kLabelY   = 52;   // 単位ラベル（その 18px 上に鮮度）
constexpr int kMainY    = 104;  // 主数値の垂直中心
constexpr int kMainMaxH = 80;   // 主数値の高さ上限
constexpr int kEgtY     = 168;  // 排気温度の垂直中心
constexpr int kEgtMaxH  = 44;   // 排気温度の高さ上限
constexpr int kStatusY  = 202;  // ブリングアップ用ステータス

// 実フレームレートの計測（SYS-12 の早期データ点。正式には QT-03 で測る）
uint32_t g_frameCount   = 0;
uint32_t g_frameUsTotal = 0;

// ---------------------------------------------------------------- CAN 受信タスク
// SWA-02 CanRxTask: Core 0 に固定する（DEC-03）
void canRxTask(void*) {
#ifdef CM_ENABLE_CAN_SIM
    // SWR-100: 実 CAN の代わりに λ / EGT をスイープする
    float lambda = 0.68f;
    float egtC   = 300.0f;
    int8_t dir   = 1;
    for (;;) {
        const uint32_t now = millis();
        lambda += 0.004f * dir;
        if (lambda > 1.36f || lambda < 0.68f)
            dir = -dir;
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
        const auto r = rusefi::decodeFrame(msg.identifier, msg.data, msg.data_length_code, g_cfg.canBaseId);

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
            const float v     = r.signals[i].value;

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

// ---------------------------------------------------------------- 輝度
/// DOC-23 §10: 輝度レベル 1-5 を PWM デューティへ。
uint8_t brightnessPwm(uint8_t level) {
    static const uint8_t kTable[5] = {26, 64, 128, 191, 255};
    if (level < 1) {
        level = 1;
    }
    if (level > 5) {
        level = 5;
    }
    return kTable[level - 1];
}

/// バックライトを滑らかに変化させる。
/// 画素を触らず PWM だけを動かすため、フェード中の CPU 負荷はほぼゼロで、
/// かつ完全に均一なフェードになる（アルファ合成では 16bit 階調が破綻する）。
void fadeBacklight(int from, int to, uint32_t durationMs) {
    constexpr uint32_t kStepMs = 16;
    const uint32_t steps       = durationMs / kStepMs;
    for (uint32_t i = 0; i <= steps; ++i) {
        const int v = from + (to - from) * static_cast<int>(i) / static_cast<int>(steps ? steps : 1);
        M5Dial.Display.setBrightness(static_cast<uint8_t>(v));
        delay(kStepMs);
    }
    M5Dial.Display.setBrightness(static_cast<uint8_t>(to));
}

// ---------------------------------------------------------------- オープニング画面
// SYS-18 / SYS-19 / SWR-48 / DOC-23 §7
//   0.0 - 0.8 s  フェードイン
//   0.8 - 2.4 s  保持（CAN を受信済みなら 0.4 s に短縮する）
//   2.4 - 3.0 s  フェードアウト
// この間も CAN 受信タスクは Core 0 で動いている（SWD-09 の起動順序）。
void showSplash(uint8_t targetLevel) {
    auto& d = M5Dial.Display;

    // 描き終わるまでバックライトは消したまま（起動時のちらつき・残像対策）
    d.setBrightness(0);
    d.fillScreen(TFT_BLACK);

    const int x = (d.width() - cm_logo_width) / 2;
    const int y = (d.height() - cm_logo_height) / 2 - 8;
    d.pushImage(x, y, cm_logo_width, cm_logo_height, reinterpret_cast<const lgfx::rgb565_t*>(cm_logo_data));

    d.setTextDatum(middle_center);
    d.setTextSize(1);
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.drawString("circle_meter", 120, 186);
    d.drawString(CM_FW_VERSION, 120, 200);

    const int target = brightnessPwm(targetLevel);
    fadeBacklight(0, target, kSplashFadeInMs);

    // SYS-18: ECU からの受信が既に始まっていれば長く見せる必要はない
    delay((g_can.stats().rxFrames > 0) ? kSplashHoldShortMs : kSplashHoldMs);

    fadeBacklight(target, 0, kSplashFadeOutMs);
    d.fillScreen(TFT_BLACK);
}

// ---------------------------------------------------------------- 文字サイズの自動決定
// 丸型画面では「画面に入る」ではなく「円に入る」かどうかで最大サイズが決まる。
// 文字列の外接矩形の四隅のうち中心から最も遠い点で判定する。
// これにより λ (1.000 = 5 文字) と AFR (14.7 = 4 文字) で自動的に適切な大きさになる。
struct TextFit {
    const lgfx::IFont* font;
    float size;  // LovyanGFX の setTextSize は小数倍を受け付ける
};

/// 文字列を半径 r の円に収めたときの最大の書体を返す。
///
/// @param yc         文字列の垂直中心
/// @param r          収めたい円の半径（リング内径より内側にする）
/// @param numericOnly 収録文字が数字と '.' だけの大きい書体を使ってよいか
/// @param maxHeight  他の要素とぶつからないための高さ上限
///
/// 候補は大きい順。整数倍だけだと AFR で 1.5 倍程度しか取れないため、
/// Font6 を小数倍でスケールする段を挟んで細かく詰める。
TextFit fitText(LovyanGFX& d, const char* str, int yc, int r, bool numericOnly, int maxHeight) {
    // Font8 / Font6 は数字・記号のみ収録。英字を含む文字列には使えない。
    static const TextFit kNumeric[] = {
        {&fonts::Font8, 1.0f}, {&fonts::Font6, 1.8f}, {&fonts::Font6, 1.7f}, {&fonts::Font6, 1.6f},
        {&fonts::Font6, 1.5f}, {&fonts::Font6, 1.4f}, {&fonts::Font6, 1.3f}, {&fonts::Font6, 1.2f},
        {&fonts::Font6, 1.1f}, {&fonts::Font6, 1.0f}, {&fonts::Font4, 1.5f}, {&fonts::Font4, 1.0f},
        {&fonts::Font0, 2.0f},
    };
    static const TextFit kAscii[] = {
        {&fonts::Font4, 2.4f}, {&fonts::Font4, 2.2f}, {&fonts::Font4, 2.0f}, {&fonts::Font4, 1.8f},
        {&fonts::Font4, 1.6f}, {&fonts::Font4, 1.4f}, {&fonts::Font4, 1.2f}, {&fonts::Font4, 1.0f},
        {&fonts::Font2, 1.5f}, {&fonts::Font2, 1.0f}, {&fonts::Font0, 2.0f}, {&fonts::Font0, 1.0f},
    };

    const TextFit* head = numericOnly ? kNumeric : kAscii;
    const size_t count =
        numericOnly ? (sizeof(kNumeric) / sizeof(kNumeric[0])) : (sizeof(kAscii) / sizeof(kAscii[0]));

    TextFit fallback{&fonts::Font0, 1.0f};

    for (size_t i = 0; i < count; ++i) {
        d.setFont(head[i].font);
        d.setTextSize(head[i].size);
        const int w = d.textWidth(str);
        const int h = d.fontHeight();

        fallback = head[i];  // 全部外れたら最後（=最小）の候補を使う
        if (h > maxHeight) {
            continue;
        }

        // 外接矩形の上辺・下辺のうち、画面中心から遠いほうで判定する
        const int dyTop    = abs(yc - h / 2 - kCy);
        const int dyBottom = abs(yc + h / 2 - kCy);
        const int dy       = (dyTop > dyBottom) ? dyTop : dyBottom;
        if (dy >= r) {
            continue;
        }
        const int avail = 2 * static_cast<int>(sqrtf(static_cast<float>(r * r - dy * dy)));
        if (w <= avail) {
            return head[i];
        }
    }
    return fallback;
}

/// fitText で選んだ書体で描く。
void drawFitted(LovyanGFX& d, const char* str, int yc, int r, bool numericOnly, int maxHeight,
                uint16_t color) {
    const TextFit f = fitText(d, str, yc, r, numericOnly, maxHeight);
    d.setFont(f.font);
    d.setTextSize(f.size);
    d.setTextColor(color, TFT_BLACK);
    d.setTextDatum(middle_center);
    d.drawString(str, kCx, yc);
}

// ---------------------------------------------------------------- 暫定 UI
const char* freshnessLabel(Freshness f) {
    switch (f) {
        case Freshness::Fresh:
            return "OK";
        case Freshness::Stale:
            return "STALE";
        default:
            return "LOST";
    }
}

uint16_t zoneColor(LambdaZone z) {
    // DOC-23 §8 のパレット（P4 で Theme に移す）
    switch (z) {
        case LambdaZone::RichHeavy:
            return M5Dial.Display.color565(0x2E, 0x7D, 0xFF);
        case LambdaZone::Rich:
            return M5Dial.Display.color565(0x00, 0xC8, 0xD7);
        case LambdaZone::Optimal:
            return M5Dial.Display.color565(0x17, 0xD1, 0x4B);
        case LambdaZone::Lean:
            return M5Dial.Display.color565(0xFF, 0xC4, 0x00);
        default:
            return M5Dial.Display.color565(0xFF, 0x2D, 0x2D);
    }
}

void drawBringupScreen(const Snapshot& snap) {
    // スプライトが確保できていればそこへ、駄目なら直接ディスプレイへ描く
    LovyanGFX& d =
        g_canvasReady ? static_cast<LovyanGFX&>(g_canvas) : static_cast<LovyanGFX&>(M5Dial.Display);
    d.fillScreen(TFT_BLACK);

    float lambda         = 0.0f;
    const bool hasLambda = snap.get(SignalId::Lambda1, lambda);
    const auto zone      = zoneOf(lambda, g_cfg.zones);

    // 外周バーの簡易版（P4 で LambdaRing に置き換える / SWD-08）
    // 数値より先に描くことで、はみ出した数値が上書きされずバーの上に載る
    d.fillArc(kCx, kCy, kRingInner, kRingOuter, 135, 135 + 270, d.color565(0x1A, 0x1A, 0x1A));
    if (hasLambda) {
        const float ratio = ringRatio(lambda, g_cfg.ringLo, g_cfg.ringHi);
        const int sweep   = static_cast<int>(ratio * 270.0f);
        if (sweep > 0) {
            d.fillArc(kCx, kCy, kRingInner, kRingOuter, 135, 135 + sweep, zoneColor(zone));
        }
    }

    const int rText = kRingInner - kTextMargin;
    char buf[16];

    // 鮮度（上端）
    d.setFont(&fonts::Font0);
    d.setTextSize(1);
    d.setTextDatum(middle_center);
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.drawString(freshnessLabel(snap.freshnessOf(SignalId::Lambda1)), kCx, kLabelY - 18);

    // 単位ラベル
    d.drawString(g_cfg.showAfr ? "AFR" : "LAMBDA", kCx, kLabelY);

    // 主数値（λ / AFR）
    if (hasLambda) {
        if (g_cfg.showAfr) {
            snprintf(buf, sizeof(buf), "%.1f", lambdaToAfr(lambda, g_cfg.stoich));
        } else {
            snprintf(buf, sizeof(buf), "%.3f", lambda);
        }
        drawFitted(d, buf, kMainY, rText, true, kMainMaxH, zoneColor(zone));
    } else {
        drawFitted(d, "--", kMainY, rText, true, kMainMaxH, TFT_DARKGREY);
    }

    // 排気温度
    float egtC = 0.0f;
    if (snap.get(SignalId::Egt1, egtC)) {
        const auto lv           = levelOf(egtC, g_cfg.egt);
        const uint16_t egtColor = (lv == EgtLevel::Danger) ? d.color565(0xFF, 0x2D, 0x2D)
                                  : (lv == EgtLevel::Warn) ? d.color565(0xFF, 0xC4, 0x00)
                                                           : TFT_LIGHTGREY;
        snprintf(buf, sizeof(buf), "%dC", static_cast<int>(egtC + 0.5f));
        drawFitted(d, buf, kEgtY, rText, false, kEgtMaxH, egtColor);
    } else {
        drawFitted(d, "--C", kEgtY, rText, false, kEgtMaxH, TFT_DARKGREY);
    }

    // ブリングアップ用ステータス（下端）
    d.setFont(&fonts::Font0);
    d.setTextSize(1);
    d.setTextDatum(middle_center);
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    snprintf(buf, sizeof(buf), "%lu f/s  %lu fps", static_cast<unsigned long>(g_can.stats().framesPerSec),
             static_cast<unsigned long>(g_frameUsTotal ? 1000000UL * g_frameCount / g_frameUsTotal : 0));
    d.drawString(buf, kCx, kStatusY);

    if (g_canvasReady) {
        g_canvas.pushSprite(0, 0);
    }
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

    // 選ばれた文字サイズをログに出す（レイアウト調整の確認用）
    {
        auto& dd    = M5Dial.Display;
        const int r = kRingInner - kTextMargin;
        struct {
            const char* s;
            int yc;
            int maxH;
            bool num;
        } probes[] = {
            {"14.7", kMainY, kMainMaxH, true},
            {"1.000", kMainY, kMainMaxH, true},
            {"845C", kEgtY, kEgtMaxH, false},
        };
        for (auto& pr : probes) {
            const TextFit f = fitText(dd, pr.s, pr.yc, r, pr.num, pr.maxH);
            dd.setFont(f.font);
            dd.setTextSize(f.size);
            Serial.printf("fit '%s': size=%.1f w=%d h=%d\n", pr.s, f.size, dd.textWidth(pr.s),
                          dd.fontHeight());
        }
    }

    // 全画面スプライト 240x240x16bit = 112.5 KB。確保できなければ直接描画へフォールバックする。
    g_canvas.setColorDepth(16);
    g_canvas.setPsram(false);  // DOC-12 §7.1: PSRAM は無い
    g_canvasReady = g_canvas.createSprite(240, 240);
    Serial.printf("offscreen canvas: %s (free heap %u)\n", g_canvasReady ? "ok" : "FAILED",
                  (unsigned)ESP.getFreeHeap());

    xTaskCreatePinnedToCore(canRxTask, "can_rx", 4096, nullptr, 10, nullptr, 0);
    xTaskCreatePinnedToCore(canHealthTask, "can_health", 3072, nullptr, 5, nullptr, 0);

    // オープニング画面（SYS-18）。CAN 受信タスクは既に走っている。
    showSplash(g_cfg.brightness);

    // 最初の本画面を描いてからバックライトを戻す（黒画面の一瞬を見せない）
    drawBringupScreen(g_store.snapshot(millis()));
    fadeBacklight(0, brightnessPwm(g_cfg.brightness), 200);
}

void loop() {
    static uint32_t nextUiMs  = 0;
    static uint32_t nextLogMs = 0;

    M5Dial.update();
    const uint32_t now = millis();

    if (static_cast<int32_t>(now - nextUiMs) >= 0) {
        nextUiMs          = now + kUiPeriodMs;
        const uint32_t t0 = micros();
        drawBringupScreen(g_store.snapshot(now));
        g_frameUsTotal += micros() - t0;
        g_frameCount++;
        if (g_frameCount >= 64) {  // 直近 64 フレームの平均に落とす
            g_frameUsTotal /= 2;
            g_frameCount /= 2;
        }
    }

    // SYS-31: ボタン短押しで AFR / λ 表示を切り替える
    if (M5Dial.BtnA.wasClicked()) {
        g_cfg.showAfr = !g_cfg.showAfr;
    }

    if (static_cast<int32_t>(now - nextLogMs) >= 0) {
        nextLogMs       = now + 1000;
        const auto& s   = g_can.stats();
        const auto snap = g_store.snapshot(now);
        float lam = 0.0f, egt = 0.0f;
        const bool hasLam      = snap.get(SignalId::Lambda1, lam);
        const bool hasEgt      = snap.get(SignalId::Egt1, egt);
        const uint32_t frameUs = g_frameCount ? g_frameUsTotal / g_frameCount : 0;
        Serial.printf(
            "rx=%lu f/s=%lu unk=%lu dlc=%lu ovf=%lu tec=%lu rec=%lu | lam=%s%.3f egt=%s%.0f | "
            "draw=%luus (max %lu fps) heap=%u\n",
            (unsigned long)s.rxFrames, (unsigned long)s.framesPerSec, (unsigned long)s.unknownId,
            (unsigned long)s.badDlc, (unsigned long)s.queueOverflow, (unsigned long)s.tec,
            (unsigned long)s.rec, hasLam ? "" : "(none)", lam, hasEgt ? "" : "(none)", egt,
            (unsigned long)frameUs, (unsigned long)(frameUs ? 1000000UL / frameUs : 0),
            (unsigned)ESP.getFreeHeap());
    }

    delay(2);
}
