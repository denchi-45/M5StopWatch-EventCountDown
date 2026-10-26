#include "SevenSegmentRenderer.h"

namespace cd {

namespace {
// Bits: a b c d e f g (a = top, then clockwise, g = middle).
uint8_t segmentsFor(char c) {
    switch (c) {
        case '0': return 0x3F;
        case '1': return 0x06;
        case '2': return 0x5B;
        case '3': return 0x4F;
        case '4': return 0x66;
        case '5': return 0x6D;
        case '6': return 0x7D;
        case '7': return 0x07;
        case '8': return 0x7F;
        case '9': return 0x6F;
        case '-': return 0x40;
        default: return 0;
    }
}

float colonW(const SegStyle& st) { return st.digitW * 0.42f; }
float thick(const SegStyle& st) { return st.thickness > 0 ? st.thickness : st.digitW * 0.18f; }
float gapW(const SegStyle& st) { return st.gap > 0 ? st.gap : st.digitW * 0.28f; }

// Elongated hexagon along (x0,y0)-(x1,y1), half thickness t, sheared by slant
// around the digit baseline `base`.
void segment(Painter& p, float x0, float y0, float x1, float y1, float t, float slant, float base,
             Color c) {
    Pt pts[6];
    const bool horiz = y0 == y1;
    if (horiz) {
        pts[0] = {x0, y0};
        pts[1] = {x0 + t, y0 - t};
        pts[2] = {x1 - t, y0 - t};
        pts[3] = {x1, y0};
        pts[4] = {x1 - t, y0 + t};
        pts[5] = {x0 + t, y0 + t};
    } else {
        pts[0] = {x0, y0};
        pts[1] = {x0 + t, y0 + t};
        pts[2] = {x0 + t, y1 - t};
        pts[3] = {x0, y1};
        pts[4] = {x0 - t, y1 - t};
        pts[5] = {x0 - t, y0 + t};
    }
    for (auto& q : pts) q.x += (base - q.y) * slant;
    p.poly(pts, 6, c);
}

enum Pass { GlowOuter, GlowInner, Body, Core };

void digit(Painter& p, uint8_t bits, float x, float y, const SegStyle& st, Pass pass) {
    const float w = st.digitW, h = st.digitH, t = thick(st) / 2, g = t * 0.45f;
    const float xl = x + t, xr = x + w - t, yt = y + t, ym = y + h / 2, yb = y + h - t;
    struct S { float x0, y0, x1, y1; };
    const S seg[7] = {
        {xl + g, yt, xr - g, yt},  // a
        {xr, yt + g, xr, ym - g},  // b
        {xr, ym + g, xr, yb - g},  // c
        {xl + g, yb, xr - g, yb},  // d
        {xl, ym + g, xl, yb - g},  // e
        {xl, yt + g, xl, ym - g},  // f
        {xl + g, ym, xr - g, ym},  // g
    };
    const float base = y + h;
    for (int i = 0; i < 7; ++i) {
        const bool on = bits & (1 << i);
        const S& s = seg[i];
        switch (pass) {
            case GlowOuter:
                if (on && st.glow) segment(p, s.x0, s.y0, s.x1, s.y1, t + w * 0.07f, st.slant, base, pal::glow1);
                break;
            case GlowInner:
                if (on && st.glow) segment(p, s.x0, s.y0, s.x1, s.y1, t + w * 0.035f, st.slant, base, pal::glow2);
                break;
            case Body:
                if (on) segment(p, s.x0, s.y0, s.x1, s.y1, t, st.slant, base, st.on);
                else if (st.showOff) segment(p, s.x0, s.y0, s.x1, s.y1, t, st.slant, base, st.off);
                break;
            case Core:
                if (on) segment(p, s.x0, s.y0, s.x1, s.y1, t * 0.36f, st.slant, base, st.core);
                break;
        }
    }
}

void colon(Painter& p, float x, float y, const SegStyle& st, Pass pass) {
    const float r = thick(st) * 0.55f, cx = x + colonW(st) / 2, base = y + st.digitH;
    for (float fy : {0.32f, 0.70f}) {
        const float cy = y + st.digitH * fy;
        const float sx = cx + (base - cy) * st.slant;
        float rr = r;
        Color c = st.on;
        if (pass == GlowOuter) { if (!st.glow) continue; rr = r + st.digitW * 0.07f; c = pal::glow1; }
        if (pass == GlowInner) { if (!st.glow) continue; rr = r + st.digitW * 0.035f; c = pal::glow2; }
        if (pass == Core) { rr = r * 0.4f; c = st.core; }
        const Pt d[4] = {{sx, cy - rr}, {sx + rr, cy}, {sx, cy + rr}, {sx - rr, cy}};
        p.poly(d, 4, c);
    }
}
}  // namespace

float SevenSegmentRenderer::width(const char* text, const SegStyle& st) {
    float w = 0;
    int n = 0;
    for (const char* c = text; *c; ++c, ++n) w += (*c == ':' ? colonW(st) : st.digitW);
    if (n > 1) w += gapW(st) * (n - 1);
    return w;
}

void SevenSegmentRenderer::draw(Painter& p, const char* text, float x, float y, const SegStyle& st) {
    for (Pass pass : {GlowOuter, GlowInner, Body, Core}) {
        float cx = x;
        for (const char* c = text; *c; ++c) {
            if (*c == ':') {
                colon(p, cx, y, st, pass);
                cx += colonW(st) + gapW(st);
            } else {
                if (*c != ' ') digit(p, segmentsFor(*c), cx, y, st, pass);
                cx += st.digitW + gapW(st);
            }
        }
    }
}

}  // namespace cd
