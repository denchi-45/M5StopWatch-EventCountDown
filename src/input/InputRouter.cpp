#include "InputRouter.h"
#include <cstdlib>

namespace cd {

InputEvent InputRouter::update(bool a, bool b, bool touch, int16_t tx, int16_t ty,
                               int64_t nowMs) {
    InputEvent ev;

    // Track press edges first so the combo check sees the current state.
    const bool aPressed = a && !a_.down, bPressed = b && !b_.down;
    const bool aReleased = !a && a_.down, bReleased = !b && b_.down;
    if (aPressed) a_ = {true, false, nowMs};
    if (bPressed) b_ = {true, false, nowMs};

    if (a && b) combo_ = true;
    if (combo_) {
        if (aReleased) a_.down = false;
        if (bReleased) b_.down = false;
        if (!a_.down && !b_.down) combo_ = false;
        // Touch held during the combo is swallowed as well.
        touchDown_ = touch;
        touchLongFired_ = true;
        return ev;
    }

    auto handle = [&](Btn& s, bool released, InputType shortT, InputType longT) {
        if (released) {
            const bool wasLong = s.longFired;
            s.down = false;
            if (!wasLong && ev.type == InputType::None) ev.type = shortT;
        } else if (s.down && !s.longFired && nowMs - s.since >= kLongMs &&
                   ev.type == InputType::None) {
            s.longFired = true;
            ev.type = longT;
        }
    };
    handle(a_, aReleased, InputType::ShortA, InputType::LongA);
    handle(b_, bReleased, InputType::ShortB, InputType::LongB);
    if (ev.type != InputType::None) return ev;

    if (touch && !touchDown_) {
        touchDown_ = true;
        touchLongFired_ = false;
        touchMoved_ = false;
        touchSince_ = nowMs;
        tx0_ = txl_ = tx;
        ty0_ = tyl_ = ty;
    } else if (touch && touchDown_) {
        txl_ = tx;
        tyl_ = ty;
        if (std::abs(tx - tx0_) > kTapSlopPx || std::abs(ty - ty0_) > kTapSlopPx)
            touchMoved_ = true;
        if (!touchLongFired_ && !touchMoved_ && nowMs - touchSince_ >= kLongMs) {
            touchLongFired_ = true;
            ev = {InputType::LongTouch, tx0_, ty0_};
        }
    } else if (!touch && touchDown_) {
        touchDown_ = false;
        if (!touchLongFired_ && !touchMoved_) ev = {InputType::Tap, tx0_, ty0_};
    }
    return ev;
}

}  // namespace cd
