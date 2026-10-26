#include "ProgressRing.h"
#include <algorithm>
#include <cmath>

namespace cd {

namespace {
constexpr Color litBody = rgb(0xF0, 0x3A, 0x3F);
constexpr Color litHi = rgb(0xFF, 0x96, 0x90);
constexpr Color litLo = rgb(0x9C, 0x1A, 0x1F);
constexpr Color metal[] = {rgb(0x26, 0x28, 0x2D), rgb(0x5C, 0x60, 0x68), rgb(0xA6, 0xAA, 0xB1),
                           rgb(0x74, 0x78, 0x80), rgb(0x3A, 0x3C, 0x42), rgb(0x1A, 0x1B, 0x1F)};

// Short annular sector as triangles. LovyanGFX fillArc tests every pixel of the
// arc's quadrant box, so ~200 calls took ~0.5 s per ring; at 2-degree steps the
// chord error is under 0.05 px at this radius.
void band(Painter& p, float cx, float cy, float r0, float r1, float a0, float a1, Color c) {
    constexpr float kDeg = 3.14159265f / 180.0f, kStep = 2.0f;
    const int n = std::max(1, int(std::ceil((a1 - a0) / kStep)));    float ca = std::cos(a0 * kDeg), sa = std::sin(a0 * kDeg);
    for (int i = 1; i <= n; ++i) {
        const float b = (a0 + (a1 - a0) * i / n) * kDeg, cb = std::cos(b), sb = std::sin(b);
        const Pt q[4] = {{cx + r0 * ca, cy + r0 * sa}, {cx + r1 * ca, cy + r1 * sa},
                         {cx + r1 * cb, cy + r1 * sb}, {cx + r0 * cb, cy + r0 * sb}};
        p.poly(q, 4, c);
        ca = cb;
        sa = sb;
    }
}
}  // namespace

void ProgressRing::drawRing(Painter& p, float cx, float cy, float rMid, float width, int segments,
                            int lit) {
    const float r0 = rMid - width / 2, r1 = rMid + width / 2;
    const float step = 360.0f / segments, gap = step * 0.2f;
    // Faint red halo under the lit part keeps the "glow" of the concept.
    for (int i = 0; i < segments; ++i) {
        const bool on = lit >= 0 && i < lit;
        float a0 = std::fmod(270.0f + i * step + gap / 2, 360.0f);
        float a1 = a0 + step - gap;
        if (on) band(p, cx, cy, r0 - 2, r1 + 2, a0 - 0.6f, a1 + 0.6f, pal::glow1);
        const Color body = on ? litBody : pal::graphite;
        const Color hi = on ? litHi : pal::graphiteHi;
        const Color lo = on ? litLo : pal::graphiteLo;
        band(p, cx, cy, r0, r1, a0, a1, body);
        // Bevel: bright outer lip, dark inner lip.
        band(p, cx, cy, r1 - width * 0.22f, r1, a0, a1, hi);
        band(p, cx, cy, r0, r0 + width * 0.18f, a0, a1, lo);
    }
}

void ProgressRing::drawRim(Painter& p, float cx, float cy, float r) {
    for (int i = 0; i < 6; ++i) p.arc(cx, cy, r + i, r + i + 1.2f, 0, 360, metal[i]);
}

void ProgressRing::drawBar(Painter& p, float x, float y, float w, float h, int segments, int lit) {
    const float step = w / segments, gap = step * 0.18f;
    for (int i = 0; i < segments; ++i) {
        const bool on = lit >= 0 && i < lit;
        const float sx = x + i * step;
        p.fillRect(sx, y, step - gap, h, on ? litBody : pal::graphite);
        p.fillRect(sx, y, step - gap, h * 0.25f, on ? litHi : pal::graphiteHi);
        p.fillRect(sx, y + h * 0.8f, step - gap, h * 0.2f, on ? litLo : pal::graphiteLo);
    }
}

}  // namespace cd
