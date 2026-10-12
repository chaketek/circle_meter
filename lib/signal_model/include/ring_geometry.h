// SWD-08: λ リングの幾何計算（HW / LVGL 非依存）
//
// リングを「前回との差分の角度範囲だけ」再描画するために、環状扇形の外接矩形を求める。
// 480x480 の RGB パネルは全面再描画で 16 fps しか出ない（OPN-12 の実測）ので、
// 定常時の無効領域を値の変化分に絞ることが SYS-12（30 fps）の前提になる（DEC-05）。
//
// 角度は LVGL 準拠（0° = 3 時方向、時計回り、Y 軸は下向き）。
#pragma once

#include <cmath>
#include <cstdint>
#include <initializer_list>

namespace cm {

/// 端点を含む矩形（LVGL の lv_area_t と同じ意味）。
struct IntRect {
    int16_t x1 = 0;
    int16_t y1 = 0;
    int16_t x2 = 0;
    int16_t y2 = 0;

    constexpr int32_t width() const { return x2 - x1 + 1; }
    constexpr int32_t height() const { return y2 - y1 + 1; }
    constexpr int32_t pixels() const { return width() * height(); }
};

namespace ring_geometry {

constexpr float kPi = 3.14159265358979f;

/// 比率 0.0-1.0 を角度（度）にする。範囲外はクランプする（SWD-08 の契約）。
constexpr float angleOfRatio(float ratio, float startDeg, float sweepDeg) {
    const float r = (ratio < 0.0f) ? 0.0f : (ratio > 1.0f ? 1.0f : ratio);
    return startDeg + sweepDeg * r;
}

/// 環状扇形 [startDeg, startDeg + sweepDeg]（半径 rInner - rOuter）の外接矩形。
/// 円弧の端点と、扇形が跨ぐ 0/90/180/270° の外周点から求める。anti-alias の滲みのため
/// marginPx だけ広げる。sweepDeg >= 360 のときは円全体になる。
inline IntRect annularSectorBounds(int cx, int cy, int rInner, int rOuter, float startDeg, float sweepDeg,
                                   int marginPx = 2) {
    if (sweepDeg < 0.0f) {
        startDeg += sweepDeg;
        sweepDeg = -sweepDeg;
    }
    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    auto take = [&](float deg, int r) {
        const float rad = deg * kPi / 180.0f;
        const float x   = static_cast<float>(cx) + static_cast<float>(r) * std::cos(rad);
        const float y   = static_cast<float>(cy) + static_cast<float>(r) * std::sin(rad);
        minX            = (x < minX) ? x : minX;
        maxX            = (x > maxX) ? x : maxX;
        minY            = (y < minY) ? y : minY;
        maxY            = (y > maxY) ? y : maxY;
    };

    if (sweepDeg >= 360.0f) {
        take(0.0f, rOuter);
        take(90.0f, rOuter);
        take(180.0f, rOuter);
        take(270.0f, rOuter);
    } else {
        take(startDeg, rInner);
        take(startDeg, rOuter);
        take(startDeg + sweepDeg, rInner);
        take(startDeg + sweepDeg, rOuter);
        // 扇形が 0/90/180/270° を跨ぐなら、その方向の外周点が外接矩形を決める
        const int first = static_cast<int>(std::ceil(startDeg / 90.0f));
        for (int k = first; static_cast<float>(k) * 90.0f <= startDeg + sweepDeg; ++k) {
            take(static_cast<float>(k) * 90.0f, rOuter);
        }
    }

    IntRect out;
    out.x1 = static_cast<int16_t>(std::floor(minX)) - marginPx;
    out.y1 = static_cast<int16_t>(std::floor(minY)) - marginPx;
    out.x2 = static_cast<int16_t>(std::ceil(maxX)) + marginPx;
    out.y2 = static_cast<int16_t>(std::ceil(maxY)) + marginPx;
    return out;
}

/// 浮かせた針（二等辺三角形）を長さ方向に segCount 等分したときの、segIndex 番目の区間の外接矩形（DEC-10）。
/// 針は半径 rBase で幅 wBase、半径 rTip で幅 0（尖る）。斜めの針の外接矩形は大きいので、
/// 区間ごとに分けて無効化すると描き直す面積が数分の 1 になる（UT-20）。
inline IntRect needleSegmentBounds(int cx, int cy, float angleDeg, float rBase, float rTip, float wBase,
                                   int segIndex, int segCount, int marginPx = 2) {
    const float rad = angleDeg * kPi / 180.0f;
    const float dx = std::cos(rad), dy = std::sin(rad);
    const float nx = -dy, ny = dx;
    const float t0 = static_cast<float>(segIndex) / static_cast<float>(segCount);
    const float t1 = static_cast<float>(segIndex + 1) / static_cast<float>(segCount);
    float minX = 1e9f, minY = 1e9f, maxX = -1e9f, maxY = -1e9f;
    for (float t : {t0, t1}) {
        const float r  = rBase + (rTip - rBase) * t;
        const float hw = 0.5f * wBase * (1.0f - t);
        for (float s : {-1.0f, 1.0f}) {
            const float x = static_cast<float>(cx) + r * dx + s * hw * nx;
            const float y = static_cast<float>(cy) + r * dy + s * hw * ny;
            minX          = (x < minX) ? x : minX;
            maxX          = (x > maxX) ? x : maxX;
            minY          = (y < minY) ? y : minY;
            maxY          = (y > maxY) ? y : maxY;
        }
    }
    IntRect out;
    out.x1 = static_cast<int16_t>(std::floor(minX)) - marginPx;
    out.y1 = static_cast<int16_t>(std::floor(minY)) - marginPx;
    out.x2 = static_cast<int16_t>(std::ceil(maxX)) + marginPx;
    out.y2 = static_cast<int16_t>(std::ceil(maxY)) + marginPx;
    return out;
}

/// 2 つの矩形の和（外接矩形）。
constexpr IntRect unionOf(const IntRect& a, const IntRect& b) {
    IntRect o;
    o.x1 = (a.x1 < b.x1) ? a.x1 : b.x1;
    o.y1 = (a.y1 < b.y1) ? a.y1 : b.y1;
    o.x2 = (a.x2 > b.x2) ? a.x2 : b.x2;
    o.y2 = (a.y2 > b.y2) ? a.y2 : b.y2;
    return o;
}

}  // namespace ring_geometry
}  // namespace cm
