#include "LayoutPolicy.h"
#include <algorithm>
#include <cmath>

namespace cd {

static Layout fit(float dw, float dh, int w, int h, bool round) {
    Layout l;
    l.round = round;
    l.designW = dw;
    l.designH = dh;
    l.scale = std::min(w / dw, h / dh);
    l.ox = (w - dw * l.scale) / 2;
    l.oy = (h - dh * l.scale) / 2;
    return l;
}

Layout LayoutPolicy::countdown(const DeviceCapabilities& caps) {
    if (caps.shape == ScreenShape::Round) return fit(kRoundSize, kRoundSize, caps.width, caps.height, true);
    return fit(320, 240, caps.width, caps.height, false);
}

Layout LayoutPolicy::settings(const DeviceCapabilities& caps) {
    Layout l = fit(kRoundSize, kRoundSize, caps.width, caps.height, true);
    l.round = caps.shape == ScreenShape::Round;
    return l;
}

float LayoutPolicy::safeHalfWidth(float y) {
    const float dy = y - kCenter;
    return dy * dy >= kSafeRadius * kSafeRadius ? 0 : std::sqrt(kSafeRadius * kSafeRadius - dy * dy);
}

}  // namespace cd
