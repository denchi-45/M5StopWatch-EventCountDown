#include "CountdownScreen.h"
#include <cstdio>
#include "../app/App.h"
#include "HudWidgets.h"
#include "ProgressRing.h"
#include "SevenSegmentRenderer.h"
#include "TextUtil.h"

namespace cd {

namespace {
constexpr float C = LayoutPolicy::kCenter;

const char* titleFor(CountdownState s) {
    switch (s) {
        case CountdownState::NoEvents: return "NO EVENTS";
        case CountdownState::TimeNotSet: return "TIME NOT SET";
        default: return "HYPE ALERT";
    }
}

void daysText(int64_t days, char* buf, size_t n) {
    if (days > 99999) days = 99999;
    std::snprintf(buf, n, days < 1000 ? "%03lld" : "%lld", (long long)days);
}

std::string eventName(App& app, Painter& p, float maxW, float size) {
    const Event* e = app.events().selected();
    if (!e) return "";
    return fittedName(p, e->name, fontset::japan, size, maxW);
}

void roundCommonBackground(Painter& p, const char* title) {
    p.clear(pal::bg);
    hud::grid(p, 0, 0, 466, 466, 22);
    p.arc(C, C, 200, 240, 0, 360, pal::bg);  // keep the grid inside the ring
    ProgressRing::drawRim(p, C, C, LayoutPolicy::kRimRadius);
    hud::rocket(p, C, 60, 26);
    hud::splitStripes(p, C, 66, 64, 92, 8);
    hud::titlePlate(p, title, C, 101, 270, 40);
}
}  // namespace

Layout CountdownScreen::layout() const { return LayoutPolicy::countdown(app.caps()); }

uint32_t CountdownScreen::backgroundKey() const {
    return uint32_t(const_cast<App&>(app).countdown().state == CountdownState::NoEvents) |
           uint32_t(const_cast<App&>(app).countdown().state == CountdownState::TimeNotSet) << 1;
}

void CountdownScreen::drawBackground(Painter& p) {
    const CountdownState s = app.countdown().state;
    if (layout().round) {
        roundCommonBackground(p, titleFor(s));
        return;
    }
    p.clear(pal::bg);
    hud::grid(p, 0, 0, 320, 240, 16);
    // Outer chamfered bezel of the rectangular layout.
    hud::frame(p, {3, 3, 314, 234}, 14, 1, true, false);
}

void CountdownScreen::draw(Painter& p) {
    const CountdownState s = app.countdown().state;
    if (!layout().round) {
        if (s == CountdownState::NoEvents || s == CountdownState::TimeNotSet) drawRectMessage(p);
        else drawRectCountdown(p);
        return;
    }
    if (s == CountdownState::NoEvents) drawRoundNoEvents(p);
    else if (s == CountdownState::TimeNotSet) drawRoundTimeNotSet(p);
    else drawRoundCountdown(p);
}

void CountdownScreen::drawRoundCountdown(Painter& p) {
    const CountdownView v = app.countdown();
    ProgressRing::drawRing(p, C, C, LayoutPolicy::kRingMid, LayoutPolicy::kRingWidth, kRingSegments,
                           v.litSegments);

    // EVENT : name
    const Rect ev{74, 133, 318, 28};
    hud::frame(p, ev, 8, 1, false);
    p.text("EVENT :", ev.x + 14, ev.cy(), middle_left, fontset::small, 1, pal::textDim);
    const std::string name = eventName(app, p, ev.w - 100, 1);
    p.text(name.c_str(), ev.x + 92, ev.cy() + 1, middle_left, fontset::japan, 1, pal::text);

    hud::stripes(p, 66, 170, 334, 8);

    HudStyle tm;
    tm.height = 13;
    tm.body = pal::redPale;
    tm.core = pal::text;
    tm.glow = false;
    tm.tracking = 3.5f;
    HudFont::drawCentered(p, "T - MINUS", C, 193, tm);

    // Days.
    char days[8];
    daysText(v.days, days, sizeof days);
    SegStyle big;
    big.digitW = 50;
    big.digitH = 64;
    const int nd = int(std::strlen(days));
    if (nd > 3) {  // shrink to keep the group inside the circle
        big.digitW = 50.0f * 3 / nd;
        big.digitH = 64.0f * (0.6f + 0.4f * 3 / nd);
    }
    HudStyle dl;
    dl.height = 18;
    const float dw = SevenSegmentRenderer::width(days, big), lw = HudFont::width("DAYS", dl);
    const float gx = C - (dw + 10 + lw) / 2;
    SevenSegmentRenderer::draw(p, days, gx, 206, big);
    HudFont::draw(p, "DAYS", gx + dw + 10, 206 + big.digitH - dl.height / 2, dl);

    // HH:MM:SS
    const Rect hms{80, 284, 306, 66};
    hud::frame(p, hms, 14, 1, false);
    char t[12];
    std::snprintf(t, sizeof t, "%02d:%02d:%02d", v.hours, v.minutes, v.seconds);
    SegStyle mid;
    mid.digitW = 27;
    mid.digitH = 38;
    SevenSegmentRenderer::drawCentered(p, t, C, 290, mid);
    const float w = SevenSegmentRenderer::width(t, mid), x0 = C - w / 2;
    const float unit = mid.digitW + mid.digitW * 0.28f;
    p.text("HRS", x0 + unit * 1 - 2, 340, middle_center, fontset::small, 0.8f, pal::textDim);
    p.text("MIN", C, 340, middle_center, fontset::small, 0.8f, pal::textDim);
    p.text("SEC", x0 + w - unit + 2, 340, middle_center, fontset::small, 0.8f, pal::textDim);

    hud::stripes(p, 102, 358, 262, 7);

    // State plate (no flashing; EVENT STARTED / FINAL COUNTDOWN get a steady glow).
    const bool emph = v.state != CountdownState::Active;
    const Rect sp{C - 100, 371, 200, 28};
    hud::frame(p, sp, 9, emph ? 2 : 1, true);
    HudStyle sl;
    sl.height = 12;
    sl.tracking = 1.8f;
    sl.core = emph ? pal::coreHot : pal::core;
    HudFont::drawCentered(p, stateLabel(v.state), C, sp.cy(), sl);
}

void CountdownScreen::drawRoundNoEvents(Painter& p) {
    ProgressRing::drawRing(p, C, C, LayoutPolicy::kRingMid, LayoutPolicy::kRingWidth, kRingSegments, -1);
    hud::stripes(p, 72, 132, 322, 8);
    p.text("YOUR COUNTDOWN STARTS HERE", C, 168, middle_center, fontset::small, 1, pal::text);
    p.line(68, 190, 210, 190, 1.5f, pal::frame);
    p.line(256, 190, 398, 190, 1.5f, pal::frame);
    const Pt notch[4] = {{210, 190}, {256, 190}, {248, 196}, {218, 196}};
    p.polyline(notch, 4, 1.5f, pal::frame, false);
    p.text("NO EVENTS REGISTERED", C, 222, middle_center, fontset::body, 1, pal::text);
    p.text("Create an event via Web Config", C, 248, middle_center, fontset::small, 0.9f, pal::textDim);
    const Rect keys{C - 96, 270, 192, 32};
    hud::frame(p, keys, 10, 1, false);
    hud::nubs(p, keys, pal::red);
    HudStyle k;
    k.height = 13;
    HudFont::drawCentered(p, "KEY A / KEY B", C, keys.cy(), k);
    p.text("No switching until created", C, 320, middle_center, fontset::small, 0.9f, pal::textDim);
    hud::stripes(p, 104, 338, 258, 7);
    openBtn_ = hud::button(p, "OPEN SETTINGS", C, 372, 196, 34, true);
    if (!app.caps().touch)
        p.text("HOLD B: SETTINGS", C, 404, middle_center, fontset::small, 0.75f, pal::textDim);
}

void CountdownScreen::drawRoundTimeNotSet(Painter& p) {
    ProgressRing::drawRing(p, C, C, LayoutPolicy::kRingMid, LayoutPolicy::kRingWidth, kRingSegments, -1);
    const Rect src{78, 132, 310, 28};
    hud::frame(p, src, 9, 1, false);
    p.text("CLOCK SOURCE :", src.x + 36, src.cy(), middle_left, fontset::small, 1, pal::text);
    p.text("INVALID", src.x + 196, src.cy(), middle_left, fontset::small, 1, pal::amber);
    hud::stripes(p, 72, 170, 322, 8);
    setTimeBtn_ = hud::button(p, "SET TIME", C, 200, 168, 28, false);
    p.text("Set on this device", C, 224, middle_center, fontset::small, 0.85f, pal::textDim);
    webBtn_ = hud::button(p, "WEB CONFIG", C, 254, 168, 28, false);
    p.text("Sync from your phone", C, 278, middle_center, fontset::small, 0.85f, pal::textDim);
    hud::button(p, "RTC / NTP", C, 308, 168, 28, false, false);
    p.text("Will update when available", C, 332, middle_center, fontset::small, 0.85f, pal::textDim);
    hud::stripes(p, 104, 348, 258, 7);
    openBtn_ = hud::button(p, "OPEN SET TIME", C, 378, 190, 32, true);
}

void CountdownScreen::drawRectCountdown(Painter& p) {
    const CountdownView v = app.countdown();
    const bool compact = app.caps().compact;
    // HYPE ALERT + stripes.
    hud::titlePlate(p, "HYPE ALERT", 116, 33, 200, 28);
    hud::stripes(p, 228, 28, 76, 10);
    // EVENT
    const Rect ev{18, 52, 284, 20};
    hud::frame(p, ev, 6, 1, false);
    p.text("EVENT :", ev.x + 10, ev.cy(), middle_left, fontset::small, 0.75f, pal::textDim);
    const std::string name = eventName(app, p, ev.w - 80, 0.7f);
    p.text(name.c_str(), ev.x + 70, ev.cy() + 1, middle_left, fontset::japan, 0.7f, pal::text);
    // Horizontal progress bar.
    hud::frame(p, {20, 77, 280, 15}, 5, 0, false);
    ProgressRing::drawBar(p, 25, 80, 270, 9, kRingSegments / 2, v.litSegments / 2);
    hud::stripes(p, 20, 97, 280, 6);
    // State / T-MINUS.
    HudStyle tm;
    tm.height = 10;
    tm.glow = false;
    tm.body = pal::redPale;
    tm.core = pal::text;
    tm.tracking = 2.5f;
    HudFont::draw(p, v.state == CountdownState::Active ? "T - MINUS" : stateLabel(v.state), 24, 114, tm);
    // Days.
    char days[8];
    daysText(v.days, days, sizeof days);
    SegStyle big;
    big.digitW = std::strlen(days) > 3 ? 26 : 34;
    big.digitH = 46;
    big.glow = !compact;
    const float dw = SevenSegmentRenderer::width(days, big);
    SevenSegmentRenderer::draw(p, days, 160 - dw / 2, 122, big);
    HudStyle dl;
    dl.height = 12;
    dl.glow = !compact;
    HudFont::draw(p, "DAYS", 160 + dw / 2 + 8, 162, dl);
    hud::stripes(p, 30, 174, 260, 6);
    // HH:MM:SS
    const Rect hms{70, 184, 180, 34};
    hud::frame(p, hms, 9, 1, false);
    char t[12];
    std::snprintf(t, sizeof t, "%02d:%02d:%02d", v.hours, v.minutes, v.seconds);
    SegStyle mid;
    mid.digitW = 15;
    mid.digitH = 24;
    mid.glow = !compact;
    SevenSegmentRenderer::drawCentered(p, t, 160, 189, mid);
    if (!compact) {
        p.text("A < PREV", 14, 226, middle_left, fontset::small, 0.6f, pal::text);
        p.text("NEXT > B", 306, 226, middle_right, fontset::small, 0.6f, pal::text);
    }
}

void CountdownScreen::drawRectMessage(Painter& p) {
    const CountdownState s = app.countdown().state;
    hud::titlePlate(p, titleFor(s), 160, 40, 260, 34);
    hud::stripes(p, 30, 66, 260, 7);
    if (s == CountdownState::NoEvents) {
        p.text("NO EVENTS REGISTERED", 160, 100, middle_center, fontset::body, 0.8f, pal::text);
        p.text("Create an event via Web Config", 160, 126, middle_center, fontset::small, 0.75f, pal::textDim);
        openBtn_ = hud::button(p, "OPEN SETTINGS", 160, 178, 180, 32, true);
    } else {
        p.text("CLOCK SOURCE : INVALID", 160, 100, middle_center, fontset::small, 0.85f, pal::amber);
        p.text("Set time on device or from your phone", 160, 126, middle_center, fontset::small, 0.7f, pal::textDim);
        openBtn_ = hud::button(p, "OPEN SET TIME", 160, 178, 180, 32, true);
    }
    if (!app.caps().touch)
        p.text("HOLD B: SETTINGS", 160, 214, middle_center, fontset::small, 0.65f, pal::textDim);
}

void CountdownScreen::onInput(const InputEvent& e, float dx, float dy) {
    const CountdownState s = app.countdown().state;
    switch (e.type) {
        case InputType::ShortA:
            if (app.selectPrev()) app.requestRedraw();
            break;
        case InputType::ShortB:
            if (app.selectNext()) app.requestRedraw();
            break;
        case InputType::LongB:
            // Touchless boards open Settings with KEY B hold (spec 4).
            if (!app.caps().touch) app.navigate(ScreenId::Menu);
            break;
        case InputType::LongTouch: {
            const float r2 = (dx - C) * (dx - C) + (dy - C) * (dy - C);
            const bool central = !layout().round || r2 < 175.0f * 175.0f;
            if (central) app.navigate(ScreenId::Menu);
            break;
        }
        case InputType::Tap:
            if (s == CountdownState::NoEvents && openBtn_.contains(dx, dy)) app.navigate(ScreenId::Menu);
            else if (s == CountdownState::TimeNotSet) {
                if (openBtn_.contains(dx, dy) || setTimeBtn_.contains(dx, dy)) app.navigate(ScreenId::SetTime);
                else if (webBtn_.contains(dx, dy)) app.navigate(ScreenId::WebConfig);
            }
            break;
        default: break;
    }
}

}  // namespace cd
