#pragma once
// Base class of device screens. A screen draws a cached background (grid,
// ring frame, title...) and a per-frame foreground; both are vector-drawn.
#include <cstdint>
#include "../input/InputRouter.h"
#include "LayoutPolicy.h"
#include "Painter.h"

namespace cd {

class App;

enum class ScreenId : uint8_t {
    Countdown, Menu, EventList, SetTime, WebConfig, Options, TimeStatus, RtcMode,
};

class Screen {
public:
    explicit Screen(App& app) : app(app) {}
    virtual ~Screen() = default;

    virtual void enter(ScreenId from) { (void)from; }
    virtual void leave() {}
    virtual Layout layout() const = 0;
    // The background is re-rendered whenever this key changes.
    virtual uint32_t backgroundKey() const { return 0; }
    virtual void drawBackground(Painter& p) = 0;
    virtual void draw(Painter& p) = 0;
    // What may change on screen with the passing of time alone (App::render).
    // ticks(): the screen shows the time at all; false = no frame per second.
    virtual bool ticks() const { return true; }
    // The part of draw() that changes from second to second (design units).
    // When nothing else changed, only this rectangle is redrawn and sent.
    // false = every second is a full frame.
    virtual bool liveRect(Rect& r) const {
        (void)r;
        return false;
    }
    // A summary of everything draw() shows outside liveRect(); when it changes,
    // the frame is a full one.
    virtual uint32_t contentKey() const { return 0; }
    // Touch coordinates are already in design units.
    virtual void onInput(const InputEvent& e, float dx, float dy) = 0;

protected:
    App& app;
};

}  // namespace cd
