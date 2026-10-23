#pragma once
// Drawing in design units. Round screens use a 466x466 design space, rectangular
// ones a 320x240 space; LayoutPolicy chooses scale/offset for the real panel.
#include <M5GFX.h>
#include <cmath>
#include <cstdint>

namespace cd {

using Color = uint16_t;  // RGB565
constexpr Color rgb(uint8_t r, uint8_t g, uint8_t b) {
    return Color(((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3));
}

namespace pal {
constexpr Color bg = rgb(0x05, 0x07, 0x0A);
constexpr Color grid = rgb(0x2C, 0x07, 0x09);
constexpr Color red = rgb(0xFF, 0x41, 0x45);
constexpr Color redDeep = rgb(0xA8, 0x16, 0x1B);
constexpr Color redDark = rgb(0x52, 0x0A, 0x0D);
constexpr Color glow1 = rgb(0x3A, 0x06, 0x08);
constexpr Color glow2 = rgb(0x7A, 0x0E, 0x12);
constexpr Color core = rgb(0xFF, 0xD2, 0xC8);
constexpr Color coreHot = rgb(0xFF, 0xF0, 0xE8);
constexpr Color frame = rgb(0xFF, 0x28, 0x2D);
constexpr Color frameDim = rgb(0x8A, 0x14, 0x18);
constexpr Color redPale = rgb(0xFF, 0x87, 0x86);
constexpr Color segOff = rgb(0x24, 0x07, 0x09);
constexpr Color panel = rgb(0x0C, 0x05, 0x06);
constexpr Color panelHi = rgb(0x30, 0x08, 0x0B);
constexpr Color graphite = rgb(0x35, 0x37, 0x40);
constexpr Color graphiteHi = rgb(0x52, 0x55, 0x60);
constexpr Color graphiteLo = rgb(0x1E, 0x1F, 0x25);
constexpr Color text = rgb(0xF4, 0xF7, 0xFA);
constexpr Color textDim = rgb(0xA4, 0xA9, 0xB2);
constexpr Color textMuted = rgb(0x5E, 0x62, 0x6A);
constexpr Color amber = rgb(0xFF, 0xB5, 0x47);
constexpr Color green = rgb(0x5E, 0xE0, 0x9A);
}  // namespace pal

struct Pt {
    float x, y;
};

struct Rect {
    float x = 0, y = 0, w = 0, h = 0;
    bool contains(float px, float py) const { return px >= x && px < x + w && py >= y && py < y + h; }
    float cx() const { return x + w / 2; }
    float cy() const { return y + h / 2; }
};

class Painter {
public:
    Painter(LovyanGFX& g, float scale, float ox, float oy) : g(g), s(scale), ox(ox), oy(oy) {}

    LovyanGFX& g;
    float s, ox, oy;

    int X(float x) const { return int(std::lround(ox + x * s)); }
    int Y(float y) const { return int(std::lround(oy + y * s)); }
    float S(float v) const { return v * s; }
    // Design coordinates of a panel point (touch).
    float toDesignX(int px) const { return (px - ox) / s; }
    float toDesignY(int py) const { return (py - oy) / s; }

    void clear(Color c) { g.fillScreen(c); }
    void fillRect(float x, float y, float w, float h, Color c);
    void line(float x0, float y0, float x1, float y1, float width, Color c);
    void hline(float x0, float x1, float y, Color c);
    void tri(Pt a, Pt b, Pt c, Color col);
    void poly(const Pt* pts, int n, Color c);  // convex, filled
    void polyline(const Pt* pts, int n, float width, Color c, bool closed);
    void arc(float cx, float cy, float r0, float r1, float a0, float a1, Color c);
    void circle(float cx, float cy, float r, Color c);
    void fillCircle(float cx, float cy, float r, Color c);

    // Bitmap/GFX fonts. datum: lgfx textdatum (e.g. middle_center).
    void text(const char* str, float x, float y, textdatum_t datum, const lgfx::IFont* font,
              float size, Color c);
    float textWidth(const char* str, const lgfx::IFont* font, float size);
    float fontHeight(const lgfx::IFont* font, float size);
};

namespace fontset {
extern const lgfx::IFont* small;   // ~13 px labels
extern const lgfx::IFont* body;    // ~17 px rows
extern const lgfx::IFont* large;   // ~25 px values
extern const lgfx::IFont* japan;   // event names (JIS gothic, proportional)
}  // namespace fontset

}  // namespace cd
