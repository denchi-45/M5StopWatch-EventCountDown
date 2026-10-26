#include "HudWidgets.h"
#include <algorithm>

namespace cd {
namespace hud {

void grid(Painter& p, float x, float y, float w, float h, float pitch) {
    const int y0 = p.Y(y), y1 = p.Y(y + h), x0 = p.X(x), x1 = p.X(x + w);
    for (float gx = x; gx <= x + w; gx += pitch) p.g.drawFastVLine(p.X(gx), y0, y1 - y0, pal::grid);
    for (float gy = y; gy <= y + h; gy += pitch) p.g.drawFastHLine(x0, p.Y(gy), x1 - x0, pal::grid);
}

static void chamferPts(const Rect& r, float c, Pt out[8]) {
    out[0] = {r.x + c, r.y};
    out[1] = {r.x + r.w - c, r.y};
    out[2] = {r.x + r.w, r.y + c};
    out[3] = {r.x + r.w, r.y + r.h - c};
    out[4] = {r.x + r.w - c, r.y + r.h};
    out[5] = {r.x + c, r.y + r.h};
    out[6] = {r.x, r.y + r.h - c};
    out[7] = {r.x, r.y + c};
}

void frame(Painter& p, const Rect& r, float cut, int emphasis, bool doubleLine, bool fill) {
    Pt o[8];
    chamferPts(r, cut, o);
    if (fill) p.poly(o, 8, emphasis == 2 ? pal::panelHi : pal::panel);
    const Color c = emphasis == 0 ? pal::frameDim : pal::frame;
    if (emphasis >= 1) p.polyline(o, 8, emphasis == 2 ? 6.0f : 4.0f, pal::glow1, true);
    if (emphasis == 2) p.polyline(o, 8, 3.2f, pal::glow2, true);
    p.polyline(o, 8, 2.0f, c, true);
    if (emphasis == 2) p.polyline(o, 8, 0.8f, pal::core, true);
    if (doubleLine) {
        const float in = 5;
        Rect ri{r.x + in, r.y + in, r.w - 2 * in, r.h - 2 * in};
        if (ri.w > 2 * cut && ri.h > 2 * cut) {
            Pt i[8];
            chamferPts(ri, std::max(2.0f, cut - in * 0.6f), i);
            p.polyline(i, 8, 1.0f, emphasis == 0 ? pal::glow2 : pal::redDeep, true);
        }
    }
}

void nubs(Painter& p, const Rect& r, Color c) {
    const float h = std::min(16.0f, r.h * 0.4f), w = h * 0.5f, cy = r.cy();
    p.tri({r.x + 3, cy}, {r.x + 3 + w, cy - h / 2}, {r.x + 3 + w, cy + h / 2}, c);
    const float rx = r.x + r.w - 3;
    p.tri({rx, cy}, {rx - w, cy - h / 2}, {rx - w, cy + h / 2}, c);
}

void stripes(Painter& p, float x, float y, float w, float h, Color c) {
    const float period = h * 1.9f, bar = h * 1.1f, lean = h * 0.7f;
    for (float sx = x; sx + bar + lean <= x + w + 0.01f; sx += period) {
        const Pt q[4] = {{sx + lean, y}, {sx + lean + bar, y}, {sx + bar, y + h}, {sx, y + h}};
        p.poly(q, 4, c);
    }
}

void splitStripes(Painter& p, float cx, float y, float gap, float len, float h) {
    stripes(p, cx - gap / 2 - len, y, len, h);
    stripes(p, cx + gap / 2, y, len, h);
}

void rocket(Painter& p, float cx, float cy, float s) {
    // Drawn pointing up-right like the concept icon: build upright, then rotate 45 deg.
    auto rot = [&](float x, float y) {
        const float k = 0.7071f;
        return Pt{cx + (x - y) * k, cy + (x + y) * k};
    };
    const float u = s / 10;
    const Pt body[6] = {rot(0, -5 * u), rot(1.6f * u, -2.5f * u), rot(1.6f * u, 2.5f * u),
                        rot(-1.6f * u, 2.5f * u), rot(-1.6f * u, -2.5f * u), rot(0, -5 * u)};
    const Pt finL[3] = {rot(-1.6f * u, 0.5f * u), rot(-3.2f * u, 3.4f * u), rot(-1.6f * u, 2.5f * u)};
    const Pt finR[3] = {rot(1.6f * u, 0.5f * u), rot(3.2f * u, 3.4f * u), rot(1.6f * u, 2.5f * u)};
    const Pt flame[3] = {rot(-1.0f * u, 2.9f * u), rot(1.0f * u, 2.9f * u), rot(0, 5.4f * u)};
    p.poly(flame, 3, pal::red);
    p.poly(finL, 3, pal::red);
    p.poly(finR, 3, pal::red);
    p.poly(body, 6, pal::redPale);
    const Pt w = rot(0, -1.4f * u);
    p.fillCircle(w.x, w.y, 0.9f * u, pal::redDeep);
}

void titlePlate(Painter& p, const char* title, float cx, float cy, float maxW, float h) {
    HudStyle st;
    st.height = h * 0.52f;
    float tw = HudFont::width(title, st);
    const float limit = maxW - h * 0.9f;
    if (tw > limit) {  // shrink long titles to fit
        st.height *= limit / tw;
        tw = HudFont::width(title, st);
    }
    const float w = std::min(maxW, tw + h * 0.9f);
    const Rect r{cx - w / 2, cy - h / 2, w, h};
    frame(p, r, h * 0.28f, 1, true);
    nubs(p, r, pal::red);
    HudFont::drawCentered(p, title, cx, cy, st);
}

Rect button(Painter& p, const char* caption, float cx, float cy, float w, float h, bool focused,
            bool enabled) {
    const Rect r{cx - w / 2, cy - h / 2, w, h};
    frame(p, r, h * 0.3f, enabled ? (focused ? 2 : 1) : 0, true);
    nubs(p, r, enabled ? pal::red : pal::frameDim);
    HudStyle st;
    st.height = h * 0.42f;
    const float tw = HudFont::width(caption, st), limit = w - h * 0.9f;
    if (tw > limit) st.height *= limit / tw;
    st.body = enabled ? pal::red : pal::textMuted;
    st.core = enabled ? (focused ? pal::coreHot : pal::core) : pal::textMuted;
    st.glow = enabled;
    HudFont::drawCentered(p, caption, cx, cy, st);
    return r;
}

}  // namespace hud
}  // namespace cd
