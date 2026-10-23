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

void Painter::line(float x0, float y0, float x1, float y1, float width, Color c) {
    const float r = std::max(0.5f, S(width) / 2);
    if (r <= 0.75f) g.drawLine(X(x0), Y(y0), X(x1), Y(y1), c);
    else g.drawWideLine(X(x0), Y(y0), X(x1), Y(y1), r, c);
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
