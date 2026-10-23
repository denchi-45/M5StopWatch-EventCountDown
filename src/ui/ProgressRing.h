#pragma once
// Segmented progress: 52-segment ring (round) or horizontal bar (rect).
// Lit = elapsed, from 12 o'clock clockwise (spec 2.1, 5).
#include "Painter.h"

namespace cd {

class ProgressRing {
public:
    // lit < 0: all segments in decorative graphite (settings / invalid states).
    static void drawRing(Painter& p, float cx, float cy, float rMid, float width, int segments,
                         int lit);
    // Metal rim just outside the ring.
    static void drawRim(Painter& p, float cx, float cy, float r);
    static void drawBar(Painter& p, float x, float y, float w, float h, int segments, int lit);
};

}  // namespace cd
