#pragma once
// Maps design spaces onto the real panel (spec 2.12, 8).
//  Round: 466x466 design space, centre (233,233), content within r195.
//  Rect : 320x240 design space for the countdown; settings pages reuse the
//         round 466 layout scaled into the shorter side.
#include "../hal/DeviceCapabilities.h"

namespace cd {

struct Layout {
    bool round = true;
    float designW = 466, designH = 466;
    float scale = 1, ox = 0, oy = 0;
};

class LayoutPolicy {
public:
    static constexpr float kRoundSize = 466;
    static constexpr float kCenter = 233;
    static constexpr float kRingMid = 215, kRingWidth = 20, kRimRadius = 226;
    static constexpr float kSafeRadius = 195;

    static Layout countdown(const DeviceCapabilities& caps);
    static Layout settings(const DeviceCapabilities& caps);
    // Half width available at design y inside the safe circle (round layout).
    static float safeHalfWidth(float y);
};

}  // namespace cd
