#pragma once
// Main countdown (spec 2.1-2.6, 2.12): COUNTDOWN ACTIVE / FINAL COUNTDOWN /
// EVENT STARTED, plus the NO EVENTS and TIME NOT SET states.
#include "Screen.h"

namespace cd {

class CountdownScreen : public Screen {
public:
    using Screen::Screen;
    Layout layout() const override;
    uint32_t backgroundKey() const override;
    void drawBackground(Painter& p) override;
    void draw(Painter& p) override;
    // Per second only the HH:MM:SS digits change; days, state, name and the
    // progress (contentKey) change rarely and then cause a full frame.
    bool ticks() const override;
    bool liveRect(Rect& r) const override;
    uint32_t contentKey() const override;
    void onInput(const InputEvent& e, float dx, float dy) override;

private:
    Rect openBtn_, setTimeBtn_, webBtn_;
    int ringLit() const;  // -1 = dark ring (no countdown)
    void drawRoundCountdown(Painter& p);
    void drawRoundNoEvents(Painter& p);
    void drawRoundTimeNotSet(Painter& p);
    void drawRectCountdown(Painter& p);
    void drawRectMessage(Painter& p);
};

}  // namespace cd
