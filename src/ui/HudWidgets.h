#pragma once
// Shared HUD decorations from the concept art (spec 1.4): red grid, frames with
// cut corners (double line for important ones), hazard stripes, rocket.
#include "HudFont.h"
#include "Painter.h"

namespace cd {

namespace hud {

void grid(Painter& p, float x, float y, float w, float h, float pitch);
// Chamfered frame. emphasis: 0 = dim outline, 1 = normal, 2 = focused (glow).
void frame(Painter& p, const Rect& r, float cut, int emphasis, bool doubleLine, bool fill = true);
// Small arrow nubs on the left/right edges (the "<  >" side markers).
void nubs(Painter& p, const Rect& r, Color c);
void stripes(Painter& p, float x, float y, float w, float h, Color c = pal::frame);
// Stripe bands either side of a centre gap (around the rocket).
void splitStripes(Painter& p, float cx, float y, float gap, float len, float h);
void rocket(Painter& p, float cx, float cy, float size);
// Headline plate: double chamfered frame with HUD lettering.
void titlePlate(Painter& p, const char* title, float cx, float cy, float maxW, float h);
// Button with HUD caption. Returns its rect.
Rect button(Painter& p, const char* caption, float cx, float cy, float w, float h, bool focused,
            bool enabled = true);

}  // namespace hud

}  // namespace cd
