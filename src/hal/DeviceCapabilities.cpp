#include "DeviceCapabilities.h"
#include <M5Unified.h>

namespace cd {

DeviceCapabilities DeviceCapabilities::detect() {
    DeviceCapabilities c;
    c.width = int16_t(M5.Display.width());
    c.height = int16_t(M5.Display.height());
    c.touch = M5.Touch.isEnabled();
    c.rtc = M5.Rtc.isEnabled();  // chip present; validity is ClockService's job
    c.speaker = M5.Speaker.isEnabled();
    c.compact = c.width < 240 || c.height < 200;

    using m5::board_t;
    switch (M5.getBoard()) {
        case board_t::board_M5StopWatch:
            c.shape = ScreenShape::Round;
            c.buttons = 2;
            c.vibration = true;
            c.boardName = "M5StopWatch";
            break;
        case board_t::board_M5Dial:
            c.shape = ScreenShape::Round;
            c.buttons = 1;
            c.boardName = "M5Dial";
            break;
        case board_t::board_M5StackCore2:
            c.buttons = 3;  // touch buttons A/B/C below the screen
            c.vibration = true;
            c.boardName = "M5Stack Core2";
            break;
        case board_t::board_M5StackCoreS3:
            c.buttons = 0;
            c.boardName = "M5Stack CoreS3";
            break;
        case board_t::board_M5StickCPlus:
        case board_t::board_M5StickCPlus2:
        case board_t::board_M5StickC:
            c.buttons = 2;
            c.boardName = "M5StickC";
            break;
        default:
            c.buttons = 2;  // unknown board: assume BtnA/BtnB are wired
            break;
    }
    return c;
}

}  // namespace cd
