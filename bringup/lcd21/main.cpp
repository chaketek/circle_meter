// Waveshare ESP32-S3-Touch-LCD-2.1 ブリングアップ（計測専用・使い捨て）
//
// 本番コードではない。次の事実を集めるためのスケッチ:
//   OPN-12  480x480 RGB パネルで CPU がフレームバッファへ描ける速度（fps）
//   OPN-15  pioarduino (Arduino-ESP32 3.3.12 / IDF 5.5.5) + ESP32_Display_Panel が動くか
//   RSK-14  Flash 書き込み (NVS) 中に画面が乱れるか
//
// 画面にはテキストを出さない（描画ライブラリを入れていないため）。代わりに
//   - 画面上端中央の 24x24 のマーカー色でフェーズを示す
//   - 計測値は UART0 (CH343P) へ出す
// 画面は外付け UVC カメラで撮影して確認する（docs/40_SUP8 §2.4.1）。
#include <Arduino.h>
#include <Preferences.h>
#include <esp_heap_caps.h>

#include <esp_display_panel.hpp>

using namespace esp_panel::board;
using namespace esp_panel::drivers;

namespace {

constexpr int kW = 480;
constexpr int kH = 480;
constexpr size_t kCanvasBytes = static_cast<size_t>(kW) * kH * sizeof(uint16_t);

Board* g_board     = nullptr;
LCD* g_lcd         = nullptr;
uint16_t* g_canvas = nullptr;  // PSRAM 450 KB。CPU はここへ描き、drawBitmap でフレームバッファへ送る
uint16_t g_pattern[kW * 2];    // 内蔵 SRAM。水平にスクロールする虹色の 1 行パターン（2 周分）

// パネルが 1 フレームを送り終えるたびに ISR から呼ばれる。リフレッシュ周波数の実測に使う。
DRAM_ATTR volatile uint32_t g_refreshCount = 0;
IRAM_ATTR bool onRefreshFinish(void*) {
    g_refreshCount = g_refreshCount + 1;
    return false;
}

constexpr uint16_t rgb565(uint8_t r, uint8_t g, uint8_t b) {
    return static_cast<uint16_t>(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

void buildPattern() {
    for (int i = 0; i < kW * 2; ++i) {
        const int x = i % kW;
        const int seg = x * 6 / kW;                  // 0..5
        const int f   = ((x * 6) % kW) * 255 / kW;  // 0..255
        uint8_t r = 0, g = 0, b = 0;
        switch (seg) {
            case 0: r = 255;     g = f;       b = 0;       break;
            case 1: r = 255 - f; g = 255;     b = 0;       break;
            case 2: r = 0;       g = 255;     b = f;       break;
            case 3: r = 0;       g = 255 - f; b = 255;     break;
            case 4: r = f;       g = 0;       b = 255;     break;
            default: r = 255;    g = 0;       b = 255 - f; break;
        }
        g_pattern[i] = rgb565(r, g, b);
    }
}

// 行 y に、パターンを (shift + 2*y) だけずらして複製する。
// ピクセル単位の演算をせず memcpy で埋めるので、CPU の「PSRAM への書き込み帯域」を測る形になる。
void fillRows(int y0, int y1, int shift) {
    for (int y = y0; y < y1; ++y) {
        const int s = (shift + 2 * y) % kW;
        memcpy(g_canvas + static_cast<size_t>(y) * kW, g_pattern + s, kW * sizeof(uint16_t));
    }
}

void drawMarker(uint16_t color) {
    for (int y = 8; y < 32; ++y) {
        uint16_t* row = g_canvas + static_cast<size_t>(y) * kW;
        for (int x = 228; x < 252; ++x) {
            row[x] = color;
        }
    }
}

void printBanner() {
    Serial.println();
    Serial.println("==== lcd21 bring-up ====");
    Serial.printf("build        : %s %s\n", __DATE__, __TIME__);
    Serial.printf("chip         : %s rev %d, %d cores, %u MHz\n", ESP.getChipModel(), ESP.getChipRevision(),
                  ESP.getChipCores(), ESP.getCpuFreqMHz());
    Serial.printf("sdk          : %s\n", ESP.getSdkVersion());
    Serial.printf("flash        : %u MB\n", ESP.getFlashChipSize() / (1024 * 1024));
    Serial.printf("psram        : total %u KB / free %u KB\n", ESP.getPsramSize() / 1024,
                  ESP.getFreePsram() / 1024);
    Serial.printf("heap(SRAM)   : free %u KB / largest block %u KB\n", ESP.getFreeHeap() / 1024,
                  heap_caps_get_largest_free_block(MALLOC_CAP_INTERNAL) / 1024);
}

// 1 フェーズ分を実行する。1 秒ごとに計測値を出す。
//   bandY0/bandRows : 毎フレーム更新する領域（全画面なら 0 / 480）
//   nvsStress       : true なら 250 ms ごとに NVS へ書き込む（Flash アクセスを発生させる = RSK-14）
void runPhase(const char* name, uint16_t marker, uint32_t durationMs, int bandY0, int bandRows,
              bool nvsStress) {
    Preferences prefs;
    if (nvsStress) {
        prefs.begin("bench", false);
    }

    // 背景を黒にして一度全面を送っておく（帯だけ更新するフェーズでも残りが黒になる）
    memset(g_canvas, 0, kCanvasBytes);
    drawMarker(marker);
    g_lcd->drawBitmap(0, 0, kW, kH, reinterpret_cast<const uint8_t*>(g_canvas));

    Serial.printf("--- phase %-10s band y=%d rows=%d nvs=%d (%u ms)\n", name, bandY0, bandRows, nvsStress,
                  durationMs);

    const uint32_t phaseStart = millis();
    uint32_t winStart         = phaseStart;
    uint32_t refreshAtWin     = g_refreshCount;
    uint32_t winFrames = 0, winFillUs = 0, winDrawUs = 0, nvsMaxUs = 0, nvsCount = 0;
    uint32_t nextNvs = phaseStart + 250;
    int shift        = 0;

    while (millis() - phaseStart < durationMs) {
        const uint32_t t0 = micros();
        fillRows(bandY0, bandY0 + bandRows, shift);
        if (bandY0 == 0) {
            drawMarker(marker);
        }
        shift = (shift + 7) % kW;
        const uint32_t t1 = micros();
        g_lcd->drawBitmap(0, bandY0, kW, bandRows,
                          reinterpret_cast<const uint8_t*>(g_canvas + static_cast<size_t>(bandY0) * kW));
        const uint32_t t2 = micros();

        winFillUs += t1 - t0;
        winDrawUs += t2 - t1;
        ++winFrames;

        if (nvsStress && millis() >= nextNvs) {
            const uint32_t n0 = micros();
            prefs.putUInt("n", nvsCount++);
            const uint32_t dt = micros() - n0;
            if (dt > nvsMaxUs) {
                nvsMaxUs = dt;
            }
            nextNvs += 250;
        }

        const uint32_t now = millis();
        if (now - winStart >= 1000) {
            const uint32_t elapsed = now - winStart;
            const uint32_t refresh = g_refreshCount - refreshAtWin;
            Serial.printf(
                "[%-10s] fps=%3u fill=%6uus draw=%6uus | panel=%3u Hz | nvs_max=%6uus | psram_free=%uKB\n", name,
                winFrames * 1000 / elapsed, winFillUs / (winFrames ? winFrames : 1),
                winDrawUs / (winFrames ? winFrames : 1), refresh * 1000 / elapsed, nvsMaxUs,
                ESP.getFreePsram() / 1024);
            winStart      = now;
            refreshAtWin  = g_refreshCount;
            winFrames = winFillUs = winDrawUs = 0;
        }
    }
    if (nvsStress) {
        prefs.end();
    }
}

}  // namespace

void setup() {
    Serial.begin(115200);
    delay(300);
    printBanner();

    buildPattern();

    g_canvas = static_cast<uint16_t*>(heap_caps_malloc(kCanvasBytes, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT));
    Serial.printf("canvas       : %s (%u bytes in PSRAM)\n", g_canvas ? "ok" : "ALLOC FAILED", (unsigned)kCanvasBytes);
    if (g_canvas == nullptr) {
        return;
    }

    Serial.println("board        : init ...");
    g_board = new Board();
    const bool ok = g_board->begin();
    Serial.printf("board        : begin() = %s\n", ok ? "OK" : "FAILED");
    if (!ok) {
        return;
    }

    g_lcd = g_board->getLCD();
    if (g_lcd == nullptr) {
        Serial.println("lcd          : NOT AVAILABLE");
        return;
    }
    g_lcd->attachRefreshFinishCallback(onRefreshFinish);

    if (auto* bl = g_board->getBacklight()) {
        bl->setBrightness(100);
        Serial.println("backlight    : 100 %");
    } else {
        Serial.println("backlight    : not available");
    }

    Serial.printf("after init   : psram free %u KB / heap free %u KB\n", ESP.getFreePsram() / 1024,
                  ESP.getFreeHeap() / 1024);
}

void loop() {
    static uint32_t cycle = 0;
    if (g_lcd == nullptr) {
        delay(1000);
        return;
    }

    Serial.printf("=== cycle %u ===\n", ++cycle);

    Serial.println("--- phase colorbar (3000 ms)");
    g_lcd->colorBarTest(kW, kH);
    delay(3000);

    runPhase("FULL", rgb565(255, 0, 0), 8000, 0, kH, false);       // 赤マーカー: 全画面を毎フレーム更新
    runPhase("BAND", rgb565(0, 255, 0), 8000, 180, 120, false);    // 緑マーカー: 480x120 の帯だけ更新
    runPhase("FULL+NVS", rgb565(0, 0, 255), 8000, 0, kH, true);    // 青マーカー: 全画面更新 + NVS 書き込み
}
