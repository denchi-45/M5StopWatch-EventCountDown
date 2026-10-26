#pragma once
// Seven-segment digits built from hexagonal polygons (spec 1.2): slight right
// slant, red body, light core, dark unlit segments and a 2-step outer glow.
#include "Painter.h"

namespace cd {

struct SegStyle {
    float digitW = 50, digitH = 90;
    float thickness = 0;  // 0 = digitW * 0.18
    float gap = 0;        // spacing between digits, 0 = digitW * 0.28
    float slant = 0.12f;
    Color on = pal::red;
    Color core = pal::core;
    Color off = pal::segOff;
    bool glow = true;
    bool showOff = true;
};

class SevenSegmentRenderer {
public:
    // text: digits, ':' and '-', ' ' (blank cell). Returns the drawn width.
    static float width(const char* text, const SegStyle& st);
    static void draw(Painter& p, const char* text, float x, float y, const SegStyle& st);
    static void drawCentered(Painter& p, const char* text, float cx, float y, const SegStyle& st) {
        draw(p, text, cx - width(text, st) / 2, y, st);
    }
};

}  // namespace cd
