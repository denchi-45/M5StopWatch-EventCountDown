#pragma once
// Turns raw button/touch levels into gestures (spec 4).
//  - Short press fires on release; once a long press (1.5 s) fired, no short.
//  - As soon as A and B are held together, every button/touch gesture is
//    suppressed until both are released: A+B belongs to the Launcher return.
// Pure logic; the caller samples hardware and passes monotonic milliseconds.
#include <cstdint>

namespace cd {

enum class InputType : uint8_t { None, ShortA, ShortB, LongA, LongB, Tap, LongTouch };

struct InputEvent {
    InputType type = InputType::None;
    int16_t x = 0, y = 0;  // touch only
};

class InputRouter {
public:
    static constexpr int64_t kLongMs = 1500;
    static constexpr int kTapSlopPx = 24;

    // Returns at most one event per call (call every loop).
    InputEvent update(bool a, bool b, bool touch, int16_t tx, int16_t ty, int64_t nowMs);
    bool comboActive() const { return combo_; }
    // True while anything is held (used to keep the display awake).
    bool anyHeld() const { return a_.down || b_.down || touchDown_; }

private:
    struct Btn {
        bool down = false;
        bool longFired = false;
        int64_t since = 0;
    };
    Btn a_, b_;
    bool combo_ = false;
    bool touchDown_ = false;
    bool touchLongFired_ = false;
    bool touchMoved_ = false;
    int64_t touchSince_ = 0;
    int16_t tx0_ = 0, ty0_ = 0, txl_ = 0, tyl_ = 0;
};

}  // namespace cd
