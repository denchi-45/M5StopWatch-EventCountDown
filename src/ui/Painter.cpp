#include "Painter.h"
#include <algorithm>

namespace cd {

namespace fontset {
const lgfx::IFont* small = &fonts::FreeSansBold9pt7b;
const lgfx::IFont* body = &fonts::FreeSansBold12pt7b;
const lgfx::IFont* large = &fonts::FreeSansBold18pt7b;
const lgfx::IFont* japan = &fonts::lgfxJapanGothicP_24;
}  // namespace fontset

void Painter::fillRect(float x, float y, float w, float h, Color c) {
    const int x0 = X(x), y0 = Y(y), x1 = X(x + w), y1 = Y(y + h);
    if (x1 > x0 && y1 > y0) g.fillRect(x0, y0, x1 - x0, y1 - y0, c);
}

namespace {
// LovyanGFX's drawWideLine() (draw_gradient_wedgeline, M5GFX 0.2.28) with the
// same scan and the same coverage, so the pixels match it, but bounded by the
// current clip rectangle. The original replaces the clip with its bounding box
// passed as (x0, y0, x1, y1) where width and height are expected, then clears
// it: drawing after it ignores the caller's clip, and a box starting at x or y
// 0 loses its last column or row. App's partial frames depend on the clip.
void wideLine(LovyanGFX& g, float ax, float ay, float bx, float by, float r, Color color) {
    constexpr float kLo = 1.0f / 32.0f, kHi = 1.0f - kLo;
    if (std::fabs(ax - bx) < 0.01f && std::fabs(ay - by) < 0.01f) bx += 0.01f;
    int32_t cx, cy, cw, ch;
    g.getClipRect(&cx, &cy, &cw, &ch);
    const int32_t x0 = std::max(cx, int32_t(std::floor(std::min(ax - r, bx - r))));
    const int32_t x1 = std::min(cx + cw - 1, int32_t(std::ceil(std::max(ax + r, bx + r))));
    const int32_t y0 = std::max(cy, int32_t(std::floor(std::min(ay - r, by - r))));
    const int32_t y1 = std::min(cy + ch - 1, int32_t(std::ceil(std::max(ay + r, by + r))));
    if (x0 > x1 || y0 > y1) return;
    const lgfx::rgb888_t fg = lgfx::convert_to_rgb888(color);
    const uint32_t solid = lgfx::color888(fg.r, fg.g, fg.b);
    // Start on the row of the leftmost end; the left edge moves right in both
    // directions away from it, which the start column (xs) relies on.
    const int32_t ys = (ax - r) > (bx - r) ? int32_t(by) : int32_t(ay);
    const float ar = r + 0.5f, bax = bx - ax, bay = by - ay;
    auto distance = [&](float px, float py) {
        const float d = (px * bax + py * bay) / (bax * bax + bay * bay);
        const float h = d < 0.0f ? 0.0f : d > 1.0f ? 1.0f : d;
        const float dx = px - bax * h, dy = py - bay * h;
        return std::sqrt(dx * dx + dy * dy);
    };
    g.startWrite();
    float alpha = 1.0f;
    auto row = [&](int32_t yp, int32_t& xs) {
        bool endX = false;
        for (int32_t xp = xs; xp <= x1; ++xp) {
            if (endX && alpha <= kLo) break;  // past the right side
            alpha = ar - distance(float(xp) - ax, float(yp) - ay);
            if (alpha <= kLo) continue;
            if (!endX) {
                endX = true;
                xs = xp;
            }
            if (alpha > kHi) g.drawPixel(xp, yp, solid);
            else g.fillRectAlpha(xp, yp, 1, 1, uint8_t(alpha * 255.0f), fg);
        }
    };
    int32_t xs = x0;
    for (int32_t yp = std::max(ys, y0); yp <= y1; ++yp) row(yp, xs);
    xs = x0;
    for (int32_t yp = std::min(ys - 1, y1); yp >= y0; --yp) row(yp, xs);
    g.endWrite();
}
}  // namespace

void Painter::line(float x0, float y0, float x1, float y1, float width, Color c) {
    const float r = std::max(0.5f, S(width) / 2);
    if (r <= 0.75f) g.drawLine(X(x0), Y(y0), X(x1), Y(y1), c);
    else wideLine(g, float(X(x0)), float(Y(y0)), float(X(x1)), float(Y(y1)), r, c);
}

void Painter::hline(float x0, float x1, float y, Color c) {
    g.drawFastHLine(X(x0), Y(y), X(x1) - X(x0), c);
}

void Painter::tri(Pt a, Pt b, Pt c, Color col) {
    g.fillTriangle(X(a.x), Y(a.y), X(b.x), Y(b.y), X(c.x), Y(c.y), col);
}

void Painter::poly(const Pt* p, int n, Color c) {
    for (int i = 1; i + 1 < n; ++i) tri(p[0], p[i], p[i + 1], c);
}

void Painter::polyline(const Pt* p, int n, float width, Color c, bool closed) {
    for (int i = 0; i + 1 < n; ++i) line(p[i].x, p[i].y, p[i + 1].x, p[i + 1].y, width, c);
    if (closed && n > 2) line(p[n - 1].x, p[n - 1].y, p[0].x, p[0].y, width, c);
}

void Painter::arc(float cx, float cy, float r0, float r1, float a0, float a1, Color c) {
    g.fillArc(X(cx), Y(cy), int(std::lround(S(r0))), int(std::lround(S(r1))), a0, a1, c);
}

void Painter::circle(float cx, float cy, float r, Color c) {
    g.drawCircle(X(cx), Y(cy), int(std::lround(S(r))), c);
}

void Painter::fillCircle(float cx, float cy, float r, Color c) {
    g.fillCircle(X(cx), Y(cy), int(std::lround(S(r))), c);
}

void Painter::text(const char* str, float x, float y, textdatum_t datum, const lgfx::IFont* font,
                   float size, Color c) {
    g.setFont(font);
    g.setTextSize(std::max(0.25f, S(size)));
    g.setTextDatum(datum);
    g.setTextColor(c);
    g.drawString(str, X(x), Y(y));
}

float Painter::textWidth(const char* str, const lgfx::IFont* font, float size) {
    g.setFont(font);
    g.setTextSize(std::max(0.25f, S(size)));
    return g.textWidth(str) / s;
}

float Painter::fontHeight(const lgfx::IFont* font, float size) {
    g.setFont(font);
    g.setTextSize(std::max(0.25f, S(size)));
    return g.fontHeight() / s;
}

}  // namespace cd
