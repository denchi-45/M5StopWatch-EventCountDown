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
    // Touch coordinates are already in design units.
    virtual void onInput(const InputEvent& e, float dx, float dy) = 0;

protected:
    App& app;
};

}  // namespace cd
