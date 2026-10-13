// SWA-10 AppController（M5Stack Dial v1.1 版。SWR-68 / SWD-15）
//
// LCD-2.1 版（main_lcd21.cpp）と同じドメイン層・受信タスクを使い、描画だけを M5GFX
// で行う（リングのデザインのみ）。
//   - λ は rusEFI WBO の StandardData から取る（SYS-03 / DEC-11。共通の CanRxTask）
//   - λ が無効なときはセンサの状態を出す（SYS-22。M5GFX の内蔵フォントは ASCII のみなので温度の単位は C）
//   - 配布先ユーザーの設定 user/user_config.h と起動ロゴを使う（SWR-67）
//   - 起動ログは LCD-2.1 版と同じ形式。シリアルから 'i' で出し直す（ネイティブ USB
//   は起動直後の出力を取りこぼすため）
//
// この版では CAN への送出は一切行わない（CLAUDE.md 規則 1 / RSK-06）。
#include <M5Dial.h>

#include <atomic>

#include "config.h"
#include "display_filter.h"
#include "display_text.h"
#include "lambda_sensor_state.h"
#include "signal_store.h"
#include "sweep_sim.h"
#include "theme_colors.h"
#include "units.h"
#include "user_config_log.h"
#include "user_overrides.h"

// SWD-14: 配布先ユーザーのロゴ（tools/select_logo.py が CM_USER_LOGO を定義する）
#ifdef CM_USER_LOGO
#include "user_logo.h"
#else
#include "logo.h"
#endif

#ifndef CM_ENABLE_CAN_SIM
#include "can_driver.h"
#include "can_rx_task.h"
#endif

#ifndef CM_FW_VERSION
#define CM_FW_VERSION "unknown"
#endif

using namespace cm;
namespace color = cm::ui::color;

namespace {

Config g_cfg;
SignalStore g_store;
DisplayFilter g_filter;
UserConfigResult g_userResult;
bool g_uiReady = false;
#ifndef CM_ENABLE_CAN_SIM
CanDriver g_can;
#endif

// 全画面のオフスクリーンバッファ。直接描くと全消去 -> 再描画の間が見えて明滅するため、1
// フレーム分をまとめて転送する
M5Canvas g_canvas(&M5Dial.Display);
bool g_canvasReady = false;

constexpr uint32_t kUiPeriodMs = 33;  // 約 30fps（SYS-12）

// オープニング画面の時間（DOC-23 §7）。合計 3.0 s
constexpr uint32_t kSplashFadeInMs    = 800;
constexpr uint32_t kSplashHoldMs      = 1600;
constexpr uint32_t kSplashHoldShortMs = 400;  // CAN を受信済みのとき
constexpr uint32_t kSplashFadeOutMs   = 600;

// 円形画面のレイアウト（DOC-23 §2 / §8.2）
constexpr int kCx         = 120;
constexpr int kCy         = 120;
constexpr int kRingOuter  = 118;
constexpr int kRingInner  = 96;  // この内側が数値の描画領域
constexpr int kTextMargin = 2;   // リングに触れないための余白

// 縦方向のバンド割り。各要素が重ならないよう高さ上限を決めておく。
constexpr int kLabelY     = 52;   // 単位ラベル（その 18px 上に鮮度）
constexpr int kMainY      = 104;  // 主数値の垂直中心
constexpr int kMainMaxH   = 80;   // 主数値の高さ上限
constexpr int kSensorY    = 136;  // λ センサの状態（λ が無効なとき。DOC-23 §8.2）
constexpr int kSensorMaxH = 16;
constexpr int kEgtY       = 168;  // 排気温度の垂直中心
constexpr int kEgtMaxH    = 44;   // 排気温度の高さ上限
constexpr int kStatusY    = 202;  // 診断（f/s・fps）

// 実フレームレートの計測（SYS-12 の早期データ点。正式には QT-03 で測る）
uint32_t g_frameCount   = 0;
uint32_t g_frameUsTotal = 0;

uint16_t c565(uint32_t hex) {
    return M5Dial.Display.color565((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF);
}

// ---------------------------------------------------------------- 信号源
#ifdef CM_ENABLE_CAN_SIM
// デモの操作（USB シリアルから 1 文字）: h 停止/再開、0-9 その位置で停止、m AFR/λ 切替、i 起動ログ
std::atomic<bool> g_simHold{false};
std::atomic<float> g_simSeek{-1.0f};

// SWR-100: 実 CAN の代わりに λ / EGT をスイープする（LCD-2.1 版と同じ。WBO の信号として入れる）
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
        const SweepOutput o = sim.step(g_simHold.load() ? 0 : now - last);
        last                = now;
        g_store.update(SignalId::WboLambda, o.lambda, now);
        g_store.update(SignalId::WboValid, 1.0f, now);
        g_store.update(SignalId::WboStatus, 2.0f, now);
        g_store.update(SignalId::WboTempC, 780.0f, now);
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

    // tools/make_logo.py の --user のプレビューと同じ位置（中央から 8 px 上）
    const int x = (d.width() - cm_logo_width) / 2;
    const int y = (d.height() - cm_logo_height) / 2 - 8;
    d.pushImage(x, y, cm_logo_width, cm_logo_height, reinterpret_cast<const lgfx::rgb565_t*>(cm_logo_data));

    d.setFont(&fonts::Font0);
    d.setTextDatum(middle_center);
    d.setTextSize(1);
    d.setTextColor(TFT_DARKGREY, TFT_BLACK);
    d.drawString("circle_meter", 120, 186);
    d.drawString(CM_FW_VERSION, 120, 200);

    const int target = brightnessPwm(targetLevel);
    fadeBacklight(0, target, kSplashFadeInMs);

    // SYS-18: ECU からの受信が既に始まっていれば長く見せる必要はない
    delay(signalSourceSeen() ? kSplashHoldShortMs : kSplashHoldMs);

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
    d.setTextColor(color, c565(color::kBg));
    d.setTextDatum(middle_center);
    d.drawString(str, kCx, yc);
}

// ---------------------------------------------------------------- 本画面（リング。DOC-23 §8.2）
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

/// DOC-23 §6.1: センサ状態の文字の色（LCD-2.1 版の sensorStatusColorHex と同じ規則）
uint32_t sensorColorHex(const LambdaSensorInfo& info) {
    switch (info.state) {
        case LambdaSensorState::SensorOff:
            return color::kTextSub;
        case LambdaSensorState::WarmingUp:
        case LambdaSensorState::Check:
            return color::kWarn;
        default:
            return color::kDanger;  // 故障・通信なし
    }
}

uint32_t egtColorHex(const DisplayValues& v) {
    if (v.egtStale) {
        return color::kDisabled;
    }
    switch (v.egtLevel) {
        case EgtLevel::Danger:
            return color::kDanger;
        case EgtLevel::Warn:
            return color::kWarn;
        default:
            return color::kEgtNormal;
    }
}

void drawMainScreen(const DisplayValues& v) {
    // スプライトが確保できていればそこへ、駄目なら直接ディスプレイへ描く
    LovyanGFX& d =
        g_canvasReady ? static_cast<LovyanGFX&>(g_canvas) : static_cast<LovyanGFX&>(M5Dial.Display);
    d.fillScreen(c565(color::kBg));

    // 外周バー。数値より先に描くことで、はみ出した数値が上書きされずバーの上に載る
    d.fillArc(kCx, kCy, kRingInner, kRingOuter, 135, 135 + 270, c565(color::kTrack));
    if (v.hasLambda) {
        const int sweep = static_cast<int>(v.ringRatio * 270.0f);
        if (sweep > 0) {
            d.fillArc(kCx, kCy, kRingInner, kRingOuter, 135, 135 + sweep,
                      c565(v.lambdaStale ? color::kDisabled : ui::zoneColorHex(v.zone)));
        }
    }

    const int rText = kRingInner - kTextMargin;
    char buf[32];

    // 鮮度（上端）と単位ラベル
    d.setFont(&fonts::Font0);
    d.setTextSize(1);
    d.setTextDatum(middle_center);
    d.setTextColor(c565(color::kTextSub), c565(color::kBg));
    d.drawString(freshnessLabel(v.lambdaFr), kCx, kLabelY - 18);
    d.drawString(g_cfg.showAfr ? "AFR" : "LAMBDA", kCx, kLabelY);

    // 主数値（λ / AFR）。Lost では "--"（古い値を出さない。RSK-01）
    if (v.hasLambda) {
        if (g_cfg.showAfr) {
            snprintf(buf, sizeof(buf), "%.1f", lambdaToAfr(v.lambda, g_cfg.stoich));
        } else {
            snprintf(buf, sizeof(buf), "%.3f", v.lambda);
        }
        drawFitted(d, buf, kMainY, rText, true, kMainMaxH,
                   c565(v.lambdaStale ? color::kDisabled : ui::zoneColorHex(v.zone)));
    } else {
        drawFitted(d, "--", kMainY, rText, true, kMainMaxH, c565(color::kDisabled));
    }

    // SYS-22 / SWR-68: λ が無効なときはその理由（停止中 / 加熱中 / 故障 / 通信なし）
    if (!v.hasLambda && v.sensor.state != LambdaSensorState::Ok) {
        formatSensorStatus(buf, sizeof(buf), v.sensor, kDegCAscii);
        drawFitted(d, buf, kSensorY, rText, false, kSensorMaxH, c565(sensorColorHex(v.sensor)));
    }

    // 排気温度
    if (v.hasEgt) {
        snprintf(buf, sizeof(buf), "%dC", static_cast<int>(v.egtC + 0.5f));
        drawFitted(d, buf, kEgtY, rText, false, kEgtMaxH, c565(egtColorHex(v)));
    } else {
        drawFitted(d, "--C", kEgtY, rText, false, kEgtMaxH, c565(color::kDisabled));
    }

    // 診断（下端）
    d.setFont(&fonts::Font0);
    d.setTextSize(1);
    d.setTextDatum(middle_center);
    d.setTextColor(c565(color::kTextSub), c565(color::kBg));
#ifdef CM_ENABLE_CAN_SIM
    const unsigned long framesPerSec = 0;
#else
    const unsigned long framesPerSec = g_can.stats().framesPerSec;
#endif
    snprintf(buf, sizeof(buf), "%lu f/s  %lu fps", framesPerSec,
             static_cast<unsigned long>(g_frameUsTotal ? 1000000UL * g_frameCount / g_frameUsTotal : 0));
    d.drawString(buf, kCx, kStatusY);

    if (g_canvasReady) {
        g_canvas.pushSprite(0, 0);
    }
}

// ---------------------------------------------------------------- シリアル
// tools/meter_log.py / bench_check.py が読む行。LCD-2.1 版と同じ形式にする（SWR-68）
void printBanner() {
    Serial.printf("\ncircle_meter %s  (M5Dial / M5GFX)\n", CM_FW_VERSION);
    Serial.printf("sdk %s / sram %u KB free\n", ESP.getSdkVersion(),
                  static_cast<unsigned>(ESP.getFreeHeap() / 1024));
#ifdef CM_ENABLE_CAN_SIM
    Serial.println("signal source: SIMULATOR (sweep)");
#else
    Serial.printf("signal source: CAN TX=GPIO%d RX=GPIO%d %u kbps base 0x%03X\n", CM_TWAI_TX_GPIO,
                  CM_TWAI_RX_GPIO, static_cast<unsigned>(g_cfg.canBitrateKbps),
                  static_cast<unsigned>(g_cfg.canBaseId));
#endif
    printUserConfig(g_userResult);
}

void printUiReady() {
    Serial.printf("ui ready: sram %u KB free / canvas %s\n", static_cast<unsigned>(ESP.getFreeHeap() / 1024),
                  g_canvasReady ? "ok" : "FAILED");
}

/// SWR-68: 'i' で起動ログを出し直す。M5Dial のネイティブ USB は、PC がポートを開く前の出力を取りこぼす
void reprintBootInfo() {
    printBanner();
    if (g_uiReady) {
        printUiReady();
    }
}

void handleSerialKeys() {
    while (Serial.available() > 0) {
        const int c = Serial.read();
        if (c == 'i') {
            reprintBootInfo();
        } else if (c == 'm') {
            g_cfg.showAfr = !g_cfg.showAfr;
            Serial.printf("display: %s\n", g_cfg.showAfr ? "AFR" : "LAMBDA");
        }
#ifdef CM_ENABLE_CAN_SIM
        else if (c == 'h') {
            g_simHold = !g_simHold.load();
            Serial.printf("sim: %s\n", g_simHold.load() ? "HOLD" : "RUN");
        } else if (c >= '0' && c <= '9') {
            g_simSeek = static_cast<float>(c - '0') / 9.0f;
            g_simHold = true;
            Serial.printf("sim: hold at %c/9\n", c);
        }
#endif
    }
}

void printStatus(uint32_t nowMs, uint32_t frameUs) {
#ifdef CM_ENABLE_CAN_SIM
    Serial.printf("fps=%lu | draw=%luus heap=%u\n", (unsigned long)(frameUs ? 1000000UL / frameUs : 0),
                  (unsigned long)frameUs, static_cast<unsigned>(ESP.getFreeHeap()));
    (void)nowMs;
#else
    // LCD-2.1 版の printCanStatus と同じ形式（tools/bench_check.py / meter_log.py が読む）
    static const char* const kSensorNames[] = {
        "OK",    "NO_SIGNAL", "OFF", "WARMUP", "CHECK", "FAULT_NO_HEAT", "FAULT_OVERHEAT", "FAULT_UNDERHEAT",
        "NO_WBO"};
    const CanStats& st            = g_can.stats();
    const Snapshot snap           = g_store.snapshot(nowMs);
    const LambdaSensorInfo sensor = classifyLambdaSensor(snap);
    float lam = 0.0f, egt = 0.0f;
    const bool hasLam = snap.get(SignalId::WboLambda, lam);  // 表示に使う λ（SYS-03）
    const bool hasEgt = snap.get(SignalId::Egt1, egt);
    Serial.printf(
        "rx=%lu f/s=%lu unk=%lu dlc=%lu ovf=%lu tec=%lu rec=%lu | lam=%s%.3f egt=%s%.0f | "
        "ageL=%lu ageE=%lu snapFail=%lu | draw=%luus (max %lu fps) heap=%u | sensor=%s %d\n",
        (unsigned long)st.rxFrames, (unsigned long)st.framesPerSec, (unsigned long)st.unknownId,
        (unsigned long)st.badDlc, (unsigned long)st.queueOverflow, (unsigned long)st.tec,
        (unsigned long)st.rec, hasLam ? "" : "(none)", lam, hasEgt ? "" : "(none)", egt,
        (unsigned long)snap.ageMs(SignalId::WboLambda), (unsigned long)snap.ageMs(SignalId::Egt1),
        (unsigned long)g_store.snapshotFailures(), (unsigned long)frameUs,
        (unsigned long)(frameUs ? 1000000UL / frameUs : 0), static_cast<unsigned>(ESP.getFreeHeap()),
        kSensorNames[static_cast<int>(sensor.state)],
        sensor.hasTemp ? static_cast<int>(sensor.tempC + 0.5f) : -1);
#endif
}

}  // namespace

void setup() {
    auto cfg = M5.config();
    M5Dial.begin(cfg, /*enableEncoder=*/true, /*enableRFID=*/false);

    M5Dial.Display.setBrightness(0);  // DOC-23 §7: 初回描画までバックライトを落とす
    M5Dial.Display.fillScreen(TFT_BLACK);

    Serial.begin(115200);
    delay(50);

    // SWD-09: 設定 -> CAN 開始 -> 受信タスク -> 画面、の順。
    // SYS-18 によりスプラッシュ表示中も受信していなければならない。
    g_cfg = defaultConfig();
    // SWR-67 / SWD-14: user/user_config.h の値を重ねる（範囲外の組は既定値のまま）。
    // 表示デザイン（USER_DIAL_STYLE）は M5Dial ではリングのみなので使わない（SWR-68）
    g_userResult = applyUserConfig(g_cfg, userConfigFromBuild());
    printBanner();

    // 全画面スプライト 240x240x16bit = 112.5 KB。確保できなければ直接描画へフォールバックする。
    g_canvas.setColorDepth(16);
    g_canvas.setPsram(false);  // DOC-12 §7.1: PSRAM は無い
    g_canvasReady = g_canvas.createSprite(240, 240);

#ifdef CM_ENABLE_CAN_SIM
    xTaskCreatePinnedToCore(simTask, "sim", 4096, nullptr, 10, nullptr, 0);
#else
    if (!g_can.begin(g_cfg)) {
        Serial.println("CAN init FAILED");
    }
    startCanRxTask(g_can, g_store, g_cfg);
    startCanHealthTask(g_can);
#endif

    // オープニング画面（SYS-18）。信号源は既に走っている。
    showSplash(g_cfg.brightness);

    // 最初の本画面を描いてからバックライトを戻す（黒画面の一瞬を見せない）
    drawMainScreen(g_filter.update(g_store.snapshot(millis()), g_cfg));
    fadeBacklight(0, brightnessPwm(g_cfg.brightness), 200);

    g_uiReady = true;
    printUiReady();
}

void loop() {
    static uint32_t nextUiMs  = 0;
    static uint32_t nextLogMs = 0;

    M5Dial.update();
    const uint32_t now = millis();

    if (static_cast<int32_t>(now - nextUiMs) >= 0) {
        nextUiMs          = now + kUiPeriodMs;
        const uint32_t t0 = micros();
        drawMainScreen(g_filter.update(g_store.snapshot(now), g_cfg));
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

    handleSerialKeys();

    if (static_cast<int32_t>(now - nextLogMs) >= 0) {
        nextLogMs = now + 1000;
        printStatus(now, g_frameCount ? g_frameUsTotal / g_frameCount : 0);
    }

    delay(2);
}
