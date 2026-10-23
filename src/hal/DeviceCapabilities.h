#pragma once
// What the running board can do (spec 2.12, 8). Decided from the board id that
// M5Unified detected, not from the screen aspect ratio alone.
#include <cstdint>

namespace cd {

enum class ScreenShape : uint8_t { Round, Rect };

struct DeviceCapabilities {
    ScreenShape shape = ScreenShape::Rect;
    int16_t width = 0, height = 0;
    bool touch = false;
    uint8_t buttons = 0;  // programmable buttons usable as KEY A / KEY B
    bool rtc = false;
    bool speaker = false;
    bool vibration = false;
    bool wifi = true;
    bool compact = false;  // small screen: digits first, fewer decorations
    const char* boardName = "unknown";

    // Full on-device editing needs touch or at least two buttons (spec 4).
    bool fullDeviceEditing() const { return touch || buttons >= 2; }

    static DeviceCapabilities detect();  // after M5.begin()
};

}  // namespace cd
