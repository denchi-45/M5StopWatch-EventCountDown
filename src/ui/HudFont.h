#pragma once
// Angular, right-slanted stroke font for headlines and button captions
// (spec 1.2/1.4: no plain bold gothic for the big titles). Glyphs are polylines
// on a 4x6 grid with chamfered corners, drawn with a layered glow.
#include "Painter.h"

namespace cd {

struct HudStyle {
    float height = 40;     // cap height in design px
    float stroke = 0.0f;   // 0 = height * 0.11
    float slant = 0.22f;   // x shift per unit of height
    Color body = pal::red;
    Color core = pal::core;
    bool glow = true;
    float tracking = 1.4f; // gap between glyphs in grid units
};

class HudFont {
public:
    static float width(const char* s, const HudStyle& st);
    // Draws centred on (cx, cy).
    static void drawCentered(Painter& p, const char* s, float cx, float cy, const HudStyle& st);
    static void draw(Painter& p, const char* s, float x, float cy, const HudStyle& st);
};

}  // namespace cd
