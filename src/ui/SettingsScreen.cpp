#include "SettingsScreen.h"
#include <cstdio>
#include <cstring>
#include "../app/App.h"
#include "HudWidgets.h"
#include "ProgressRing.h"
#include "SevenSegmentRenderer.h"
#include "TextUtil.h"

namespace cd {

namespace {
constexpr float C = LayoutPolicy::kCenter;

void localStamp(App& app, int64_t utc, char* date, size_t dn, char* time, size_t tn) {
    const Civil c = utcToLocal(utc, app.settings().displayTz);
    formatDate(c, date, dn);
    formatTime(c, time, tn);
}
}  // namespace

// ------------------------------------------------------------------ base

Layout SettingsPage::layout() const { return LayoutPolicy::settings(app.caps()); }

void SettingsPage::drawBackground(Painter& p) {
    p.clear(pal::bg);
    hud::grid(p, 0, 0, 466, 466, 22);
    if (layout().round) {
        p.arc(C, C, 200, 240, 0, 360, pal::bg);
        // Settings keep the ring as dark decoration: no fictitious progress.
        ProgressRing::drawRing(p, C, C, LayoutPolicy::kRingMid, LayoutPolicy::kRingWidth,
                               kRingSegments, -1);
        ProgressRing::drawRim(p, C, C, LayoutPolicy::kRimRadius);
    }
    hud::rocket(p, C, 60, 26);
    hud::splitStripes(p, C, 66, 64, 92, 8);
    hud::titlePlate(p, title_, C, 101, 300, 42);
}

void SettingsPage::onBack() { app.back(); }

void SettingsPage::setHit(int i, const Rect& r) {
    if (int(hits_.size()) <= i) hits_.resize(size_t(i) + 1);
    hits_[size_t(i)] = r;
}

void SettingsPage::onInput(const InputEvent& e, float dx, float dy) {
    const int n = focusCount();
    if (n <= 0) return;
    if (focus_ >= n) focus_ = n - 1;
    switch (e.type) {
        case InputType::Tap:
            if (onFreeTap(dx, dy)) break;
            for (int i = 0; i < int(hits_.size()) && i < n; ++i) {
                if (hits_[size_t(i)].w > 0 && hits_[size_t(i)].contains(dx, dy)) {
                    if (editing_ && i != focus_) editing_ = false;
                    focus_ = i;
                    onTap(i);
                    break;
                }
            }
            break;
        case InputType::ShortA:
        case InputType::ShortB: {
            const int dir = e.type == InputType::ShortA ? -1 : +1;
            if (editing_) adjust(focus_, dir);
            else focus_ = (focus_ + dir + n) % n;
            break;
        }
        case InputType::LongB:
            if (editing_) editing_ = false;  // value kept in the page draft
            else if (editable(focus_)) {
                beginEdit(focus_);
                editing_ = true;
            } else activate(focus_);
            break;
        case InputType::LongA:
            if (editing_) {
                cancelEdit(focus_);
                editing_ = false;
            } else onBack();
            break;
        default: break;
    }
    app.requestRedraw();
}

Rect SettingsPage::row(Painter& p, int i, float cy, float w, float h, const char* label,
                       const char* value, bool enabled) {
    const Rect r{C - w / 2, cy - h / 2, w, h};
    const bool f = focused(i);
    hud::frame(p, r, 9, enabled ? (f ? 2 : 1) : 0, false);
    if (f) hud::nubs(p, r, editing_ ? pal::amber : pal::red);
    const Color lc = enabled ? (f && editing_ ? pal::amber : pal::text) : pal::textMuted;
    p.text(label, r.x + 24, cy, middle_left, fontset::body, 0.8f, lc);
    if (value && *value)
        p.text(value, r.x + w - 24, cy, middle_right, fontset::body, 0.8f,
               enabled ? (f && editing_ ? pal::amber : pal::redPale) : pal::textMuted);
    setHit(i, r);
    return r;
}

Rect SettingsPage::btn(Painter& p, int i, const char* caption, float cx, float cy, float w, float h,
                       bool enabled) {
    const Rect r = hud::button(p, caption, cx, cy, w, h, focused(i), enabled);
    setHit(i, r);
    return r;
}

void SettingsPage::hint(Painter& p, float y, const char* touchText, const char* buttonText) {
    const char* t = app.caps().touch ? touchText : buttonText;
    if (!t) return;
    const float w = p.textWidth(t, fontset::small, 0.8f);
    p.text(t, C, y, middle_center, fontset::small, 0.8f, pal::text);
    const float half = LayoutPolicy::safeHalfWidth(y) - 8;
    if (half > w / 2 + 40) {
        hud::stripes(p, C - half, y - 4, half - w / 2 - 10, 8);
        hud::stripes(p, C + w / 2 + 10, y - 4, half - w / 2 - 10, 8);
    }
}

// ------------------------------------------------------------------ menu

void MenuPage::enter(ScreenId from) {
    if (from == ScreenId::Countdown) focus_ = 0;
    editing_ = false;
}

void MenuPage::draw(Painter& p) {
    hud::stripes(p, 76, 132, 314, 8);
    char count[12];
    std::snprintf(count, sizeof count, "%u / 10", unsigned(app.events().events().size()));
    const float y0 = 166, step = 38, w = 300, h = 32;
    row(p, 0, y0, w, h, "EVENT LIST", count);
    row(p, 1, y0 + step, w, h, "SET TIME", clockSourceText(app.clock().source()));
    row(p, 2, y0 + step * 2, w, h, "WEB CONFIG", app.webConfigActive() ? "ON" : "AP");
    row(p, 3, y0 + step * 3, w, h, "DEVICE OPTIONS", "");
    row(p, 4, y0 + step * 4, w, h, "TIME STATUS", app.clock().valid() ? "VALID" : "INVALID");
    hint(p, 352, "TAP TO SELECT", "HOLD B OPEN / HOLD A BACK");
    btn(p, 5, "BACK", C, 384, 150, 32);
}

void MenuPage::activate(int i) {
    static const ScreenId kTargets[] = {ScreenId::EventList, ScreenId::SetTime, ScreenId::WebConfig,
                                        ScreenId::Options, ScreenId::TimeStatus};
    if (i < 5) app.navigate(kTargets[i]);
    else onBack();
}

// ------------------------------------------------------------------ event list

void EventListPage::enter(ScreenId) {
    focus_ = 0;
    first_ = 0;
    editing_ = false;
    const auto& ev = app.events().events();
    for (size_t i = 0; i < ev.size(); ++i)
        if (ev[i].id == app.events().selectedId()) focus_ = int(i);
}

int EventListPage::focusCount() const { return int(app.events().events().size()) + 1; }

void EventListPage::draw(Painter& p) {
    const auto& ev = app.events().events();
    const int n = int(ev.size());
    constexpr int kVisible = 4;
    if (focus_ < n) {
        if (focus_ < first_) first_ = focus_;
        if (focus_ >= first_ + kVisible) first_ = focus_ - kVisible + 1;
    }
    if (first_ > n - kVisible) first_ = n > kVisible ? n - kVisible : 0;
    hits_.assign(size_t(n) + 1, Rect{});

    if (n == 0) {
        p.text("NO EVENTS REGISTERED", C, 220, middle_center, fontset::body, 0.9f, pal::text);
        p.text("Add events in Web Config", C, 250, middle_center, fontset::small, 0.9f, pal::textDim);
        if (!app.configError().empty())
            p.text("STORED EVENTS INVALID - SEE WEB CONFIG", C, 278, middle_center, fontset::small,
                   0.75f, pal::amber);
    }
    const int64_t now = app.clock().valid() ? app.clock().nowUtc() : 0;
    for (int k = 0; k < kVisible && first_ + k < n; ++k) {
        const int i = first_ + k;
        const Event& e = ev[size_t(i)];
        const float cy = 158 + k * 42, w = 320, h = 36;
        const Rect r{C - w / 2, cy - h / 2, w, h};
        const bool f = focused(i), sel = e.id == app.events().selectedId();
        hud::frame(p, r, 10, e.enabled ? (f ? 2 : 1) : 0, sel);
        hud::nubs(p, r, e.enabled ? pal::red : pal::frameDim);
        char idx[4];
        std::snprintf(idx, sizeof idx, "%02d", i + 1);
        SegStyle s;
        s.digitW = 11;
        s.digitH = 20;
        s.glow = false;
        s.showOff = false;
        if (!e.enabled) s.on = pal::frameDim;
        SevenSegmentRenderer::draw(p, idx, r.x + 20, cy - 10, s);
        const std::string name =
            ellipsize(p, fontSafe(e.name, fontset::japan), fontset::japan, 0.8f, 168);
        p.text(name.c_str(), r.x + 62, cy + 1, middle_left, fontset::japan, 0.8f,
               e.enabled ? pal::text : pal::textMuted);
        p.line(r.x + w - 76, cy - 11, r.x + w - 76, cy + 11, 1, pal::frameDim);
        char status[12];
        if (!e.enabled) std::snprintf(status, sizeof status, "OFF");
        else if (sel) std::snprintf(status, sizeof status, "ACTIVE");
        else if (!app.clock().valid()) std::snprintf(status, sizeof status, "--");
        else if (now >= e.targetUtc) std::snprintf(status, sizeof status, "DONE");
        else std::snprintf(status, sizeof status, "%lldD", (long long)((e.targetUtc - now) / 86400));
        HudStyle hs;
        hs.height = 11;
        hs.glow = false;
        hs.body = e.enabled ? pal::red : pal::textMuted;
        hs.core = e.enabled ? pal::core : pal::textMuted;
        HudFont::drawCentered(p, status, r.x + w - 40, cy, hs);
        setHit(i, r);
    }
    if (first_ > 0) p.tri({C - 8, 137}, {C + 8, 137}, {C, 130}, pal::red);
    if (first_ + kVisible < n) p.tri({C - 8, 309}, {C + 8, 309}, {C, 316}, pal::red);

    hud::stripes(p, 96, 320, 274, 7);
    p.text(app.caps().touch ? "TAP TO CHOOSE / KEY A,B ON MAIN" : "HOLD B CHOOSE / HOLD A BACK", C,
           340, middle_center, fontset::small, 0.75f, pal::text);
    p.text("EDIT EVENTS IN WEB CONFIG", C, 358, middle_center, fontset::small, 0.7f, pal::textDim);
    btn(p, n, "BACK", C, 388, 120, 28);
}

void EventListPage::activate(int i) {
    const auto& ev = app.events().events();
    if (i >= int(ev.size())) return onBack();
    const RepoError err = app.selectEvent(ev[size_t(i)].id, false);
    if (err == RepoError::Ok) app.toast("EVENT SELECTED");
    else if (err == RepoError::Disabled) app.toast("DISABLED - ENABLE IN WEB", true);
}

// ------------------------------------------------------------------ set time

void SetTimePage::enter(ScreenId) {
    focus_ = 0;
    editing_ = false;
    Civil c{2026, 1, 1, 0, 0, 0};
    if (app.clock().valid()) c = utcToLocal(app.clock().nowUtc(), app.settings().displayTz);
    editor_.load(c);
}

void SetTimePage::draw(Painter& p) {
    const Tz tz = app.settings().displayTz;
    const Rect sub{C - 140, 134, 280, 26};
    hud::frame(p, sub, 8, 1, false);
    p.text("MANUAL CLOCK ADJUST", sub.x + 22, sub.cy(), middle_left, fontset::small, 0.85f, pal::text);
    HudStyle tzs;
    tzs.height = 12;
    tzs.glow = false;
    HudFont::draw(p, tzShort(tz), sub.x + sub.w - 56, sub.cy(), tzs);

    struct Field { int idx; float cx; const char* label; const char* fmt; float digits; };
    const Field fields[6] = {
        {0, 126, "YEAR", "%04d", 4}, {1, 233, "MONTH", "%02d", 2}, {2, 326, "DAY", "%02d", 2},
        {3, 140, "HOUR", "%02d", 2}, {4, 233, "MIN", "%02d", 2},   {5, 326, "SEC", "%02d", 2},
    };
    const Rect dateRow{70, 166, 326, 58}, timeRow{70, 230, 326, 58};
    hud::frame(p, dateRow, 14, 1, false);
    hud::frame(p, timeRow, 14, 1, false);
    p.text("-", 183, 190, middle_center, fontset::large, 0.8f, pal::red);
    p.text("-", 279, 190, middle_center, fontset::large, 0.8f, pal::red);
    p.text(":", 186, 254, middle_center, fontset::large, 0.8f, pal::red);
    p.text(":", 280, 254, middle_center, fontset::large, 0.8f, pal::red);
    for (const Field& f : fields) {
        const float top = f.idx < 3 ? 172 : 236;
        char v[8];
        std::snprintf(v, sizeof v, f.fmt, editor_.value(f.idx));
        SegStyle s;
        s.digitW = 16;
        s.digitH = 28;
        s.glow = true;
        const bool fo = focused(f.idx);
        if (fo && editing_) s.on = pal::amber;
        const float w = SevenSegmentRenderer::width(v, s);
        const Rect box{f.cx - w / 2 - 8, top - 3, w + 16, 34};
        if (fo) hud::frame(p, box, 6, 2, false, false);
        SevenSegmentRenderer::draw(p, v, f.cx - w / 2, top, s);
        p.text(f.label, f.cx, top + 43, middle_center, fontset::small, 0.7f,
               fo ? pal::redPale : pal::textDim);
        setHit(f.idx, box);
    }
    // +/- drawn as bold bars of equal size, centred in identical buttons.
    const bool canAdjust = focus_ < 6;
    minus_ = hud::button(p, "", 180, 312, 92, 32, false, canAdjust);
    plus_ = hud::button(p, "", 286, 312, 92, 32, false, canAdjust);
    const Color sym = canAdjust ? pal::core : pal::textMuted;
    p.fillRect(180 - 11, 312 - 2.5f, 22, 5, sym);
    p.fillRect(286 - 11, 312 - 2.5f, 22, 5, sym);
    p.fillRect(286 - 2.5f, 312 - 11, 5, 22, sym);
    if (!app.caps().touch)
        p.text(editing_ ? "A-/B+  HOLD B OK  HOLD A UNDO" : "A/B MOVE  HOLD B EDIT", C, 338,
               middle_center, fontset::small, 0.65f, pal::textDim);
    btn(p, 6, "SAVE TIME", 290, 364, 116, 30);
    btn(p, 7, "CANCEL", 176, 364, 104, 30);
}

void SetTimePage::adjust(int i, int dir) {
    if (i < 6) editor_.adjust(i, dir);
}

void SetTimePage::onTap(int i) {
    if (i >= 6) activate(i);  // fields: tap only focuses
}

bool SetTimePage::onFreeTap(float x, float y) {
    if (focus_ >= 6) return false;
    if (minus_.contains(x, y)) { editor_.adjust(focus_, -1); return true; }
    if (plus_.contains(x, y)) { editor_.adjust(focus_, +1); return true; }
    return false;
}

void SetTimePage::activate(int i) {
    if (i == 7) return onBack();
    if (i != 6) return;
    if (!editor_.valid()) return app.toast("INVALID DATE", true);
    const SetResult r = app.setClockLocal(editor_.draft());
    if (r == SetResult::Invalid) return app.toast("INVALID DATE", true);
    app.toast(r == SetResult::Ok ? "TIME SAVED" : "SAVED / RTC WRITE FAILED", r != SetResult::Ok);
    onBack();
}

// ------------------------------------------------------------------ web config

void WebConfigPage::enter(ScreenId) {
    focus_ = 0;
    app.startWebConfig();
}

void WebConfigPage::onInput(const InputEvent& e, float dx, float dy) {
    app.noteSetupActivity();  // device-side operation counts as activity
    SettingsPage::onInput(e, dx, dy);
}

void WebConfigPage::draw(Painter& p) {
    hud::stripes(p, 76, 132, 314, 8);
    if (!app.webConfigActive()) {
        p.text(app.webConfigFailed() ? "AP START FAILED" : "SETUP CLOSED", C, 220, middle_center,
               fontset::body, 0.9f, pal::amber);
        btn(p, 0, "EXIT", C, 384, 120, 30);
        return;
    }
    const Rect conn{C - 150, 146, 300, 28};
    hud::frame(p, conn, 9, 1, false);
    hud::nubs(p, conn, pal::red);
    p.text("CONNECT YOUR PHONE / PC", C, conn.cy(), middle_center, fontset::small, 0.9f, pal::text);

    auto field = [&](const char* label, const std::string& value, float y, float w, Color c) {
        p.text(label, C, y, middle_center, fontset::small, 0.8f, pal::text);
        const Rect r{C - w / 2, y + 10, w, 30};
        hud::frame(p, r, 9, 1, true);
        hud::nubs(p, r, pal::red);
        p.text(value.c_str(), C, r.cy() + 1, middle_center, fontset::body, 0.9f, c);
    };
    field("Wi-Fi SSID", app.net().apSsid(), 188, 290, pal::text);
    field("PASSWORD", app.net().apPassword(), 238, 250, pal::amber);
    field("OPEN IN BROWSER", "http://" + app.net().apIp(), 288, 250, pal::text);

    const int64_t rem = app.apRemainingMs() / 1000;
    char t[40];
    std::snprintf(t, sizeof t, "IDLE TIMEOUT %d:%02d", int(rem / 60), int(rem % 60));
    p.text(t, C, 346, middle_center, fontset::small, 0.8f, rem <= 30 ? pal::amber : pal::textDim);
    hud::stripes(p, 112, 360, 242, 7);
    btn(p, 0, "EXIT", C, 390, 120, 30);
}

void WebConfigPage::activate(int) { onBack(); }

void WebConfigPage::onBack() {
    app.stopWebConfig();  // also leaves this page when setup was active
    if (app.current() == ScreenId::WebConfig) app.back();
}

// ------------------------------------------------------------------ device options

void OptionsPage::enter(ScreenId from) {
    if (from != ScreenId::RtcMode) {
        draft_ = app.settings();
        focus_ = 0;
    }
    editing_ = false;
}

bool OptionsPage::editable(int i) const {
    if (i == 2) return app.caps().speaker;
    if (i == 3) return app.caps().vibration;
    return i < 2;
}

void OptionsPage::draw(Painter& p) {
    hud::stripes(p, 76, 132, 314, 8);
    char b[8], d[8];
    std::snprintf(b, sizeof b, "%u%%", draft_.brightness);
    if (draft_.dimAfterSec == 0) std::snprintf(d, sizeof d, "NEVER");
    else if (draft_.dimAfterSec < 60) std::snprintf(d, sizeof d, "%uS", draft_.dimAfterSec);
    else std::snprintf(d, sizeof d, "%uM", draft_.dimAfterSec / 60);
    const float y0 = 162, step = 37, w = 300, h = 31;
    row(p, 0, y0, w, h, "BRIGHTNESS", b);
    row(p, 1, y0 + step, w, h, "DIM AFTER", d);
    row(p, 2, y0 + step * 2, w, h, "SOUND", app.caps().speaker ? (draft_.sound ? "ON" : "OFF") : "N/A",
        app.caps().speaker);
    row(p, 3, y0 + step * 3, w, h, "VIBRATION",
        app.caps().vibration ? (draft_.vibration ? "ON" : "OFF") : "N/A", app.caps().vibration);
    row(p, 4, y0 + step * 4, w, h, "RTC MODE", rtcModeText(app.settings().rtcMode));
    hud::stripes(p, 100, 334, 266, 7);
    btn(p, 5, "BACK", 178, 368, 104, 30);
    btn(p, 6, "SAVE", 288, 368, 104, 30);
}

void OptionsPage::adjust(int i, int dir) {
    switch (i) {
        case 0:
            draft_.brightness = cycleStep(kBrightnessSteps, draft_.brightness, dir);
            app.previewBrightness(draft_.brightness);
            break;
        case 1: draft_.dimAfterSec = cycleStep(kDimSteps, draft_.dimAfterSec, dir); break;
        case 2: if (app.caps().speaker) draft_.sound = !draft_.sound; break;
        case 3: if (app.caps().vibration) draft_.vibration = !draft_.vibration; break;
    }
}

void OptionsPage::cancelEdit(int) {
    draft_ = backup_;
    app.previewBrightness(draft_.brightness);
}

void OptionsPage::onTap(int i) {
    if (i < 4) {
        if (editable(i)) adjust(i, +1);
        else app.toast("NOT AVAILABLE ON THIS DEVICE", true);
    } else activate(i);
}

void OptionsPage::activate(int i) {
    if (i < 4) {
        if (!editable(i)) app.toast("NOT AVAILABLE ON THIS DEVICE", true);
        return;
    }
    if (i == 4) return app.navigate(ScreenId::RtcMode);
    if (i == 5) return onBack();
    if (app.saveSettings(draft_)) {
        app.toast("OPTIONS SAVED");
        app.back();
    } else {
        app.toast("SAVE FAILED", true);
    }
}

void OptionsPage::onBack() {
    app.restoreBrightness();  // unsaved preview is discarded
    app.back();
}

// ------------------------------------------------------------------ time status

void TimeStatusPage::draw(Painter& p) {
    ClockService& clk = app.clock();
    const Rect src{C - 158, 130, 316, 28};
    hud::frame(p, src, 9, 1, false);
    p.text("CLOCK SOURCE", src.x + 18, src.cy(), middle_left, fontset::small, 0.85f, pal::text);
    char s[32];
    std::snprintf(s, sizeof s, "%s / %s", clockSourceText(clk.source()), clk.valid() ? "VALID" : "INVALID");
    p.text(s, src.x + src.w - 18, src.cy(), middle_right, fontset::body, 0.85f,
           clk.valid() ? pal::text : pal::amber);

    char date[16] = "----------", time[12] = "--:--:--";
    if (clk.valid()) localStamp(app, clk.nowUtc(), date, sizeof date, time, sizeof time);
    SegStyle ds;
    ds.digitW = 13;
    ds.digitH = 22;
    ds.on = pal::redPale;
    SevenSegmentRenderer::drawCentered(p, date, C, 168, ds);
    SegStyle ts;
    ts.digitW = 30;
    ts.digitH = 46;
    SevenSegmentRenderer::drawCentered(p, time, C, 198, ts);

    const Rect info{C - 160, 254, 320, 84};
    hud::frame(p, info, 14, 1, false);
    const Tz tz = app.settings().displayTz;
    char l1[48], l2[48], l3[56], l4[64];
    std::snprintf(l1, sizeof l1, "TIMEZONE: %s (UTC%s)", tzShort(tz), tz == Tz::AsiaTokyo ? "+09" : "+00");
    std::snprintf(l2, sizeof l2, "RTC: %s / %s / WRITE %s", rtcModeText(clk.rtcMode()),
                  !clk.rtcAvailable() ? "NONE" : clk.rtcValid() ? "VALID" : "INVALID",
                  rtcWriteStatusText(clk.rtcWriteStatus()));
    std::snprintf(l3, sizeof l3, "WI-FI: %s", staStateText(app.net().staState()));
    if (app.lastNtpSuccessUtc() > 0) {
        char d2[16], t2[12];
        localStamp(app, app.lastNtpSuccessUtc(), d2, sizeof d2, t2, sizeof t2);
        std::snprintf(l4, sizeof l4, "NTP: %s  %s %.5s", ntpStateText(app.ntpState()), d2 + 5, t2);
    } else {
        std::snprintf(l4, sizeof l4, "NTP: %s  NEVER", ntpStateText(app.ntpState()));
    }
    const char* lines[4] = {l1, l2, l3, l4};
    for (int i = 0; i < 4; ++i)
        p.text(lines[i], info.x + 22, info.y + 14 + i * 19, middle_left, fontset::small, 0.75f,
               i == 0 ? pal::text : pal::textDim);
    hud::stripes(p, 104, 346, 258, 7);
    btn(p, 0, "SYNC NTP", 176, 376, 106, 30);
    btn(p, 1, "BACK", 290, 376, 106, 30);
}

void TimeStatusPage::activate(int i) {
    if (i == 1) return onBack();
    switch (app.requestNtpSync()) {
        case NtpScheduler::Manual::Started: app.toast("NTP SYNC STARTED"); break;
        case NtpScheduler::Manual::Scheduled: app.toast("NTP SYNC SCHEDULED"); break;
        case NtpScheduler::Manual::Disabled: app.toast("NTP DISABLED (WEB SETTINGS)", true); break;
        case NtpScheduler::Manual::Offline: app.toast("WI-FI OFFLINE", true); break;
    }
}

// ------------------------------------------------------------------ RTC mode

void RtcModePage::enter(ScreenId) {
    draft_ = app.settings().rtcMode;
    focus_ = draft_ == RtcMode::Utc ? 0 : 1;
    editing_ = false;
}

void RtcModePage::draw(Painter& p) {
    const Rect sub{C - 92, 132, 184, 24};
    hud::frame(p, sub, 8, 1, false);
    p.text("HOW RTC STORES TIME", C, sub.cy(), middle_center, fontset::small, 0.8f, pal::text);
    hud::stripes(p, 82, 140, 54, 8);
    hud::stripes(p, 330, 140, 54, 8);

    auto option = [&](int i, RtcMode m, const char* title, const char* sub, float cy) {
        const Rect r{C - 150, cy - 21, 300, 42};
        const bool chosen = draft_ == m;
        hud::frame(p, r, 12, chosen ? 2 : (focused(i) ? 1 : 0), true);
        if (chosen || focused(i)) hud::nubs(p, r, pal::red);
        HudStyle hs;
        hs.height = 15;
        hs.body = chosen ? pal::red : pal::textDim;
        hs.core = chosen ? pal::coreHot : pal::text;
        hs.glow = chosen;
        HudFont::draw(p, title, r.x + 26, cy - 7, hs);
        p.text(sub, r.x + 26, cy + 11, middle_left, fontset::small, 0.7f, pal::textDim);
        if (focused(i) && !chosen) p.text("TAP / HOLD B", r.x + r.w - 22, cy, middle_right, fontset::small, 0.6f, pal::textMuted);
        setHit(i, r);
    };
    option(0, RtcMode::Utc, "UTC", "RECOMMENDED", 186);
    option(1, RtcMode::LocalJst, "LOCAL JST", "FOR COMPATIBILITY", 236);

    char hdr[32];
    std::snprintf(hdr, sizeof hdr, "PREVIEW / %s", tzShort(app.settings().displayTz));
    p.text(hdr, C, 272, middle_center, fontset::small, 0.8f, pal::redPale);
    char nowD[16] = "--", nowT[12] = "", aftD[16] = "UNAVAILABLE", aftT[12] = "";
    if (app.clock().valid()) localStamp(app, app.clock().nowUtc(), nowD, sizeof nowD, nowT, sizeof nowT);
    int64_t after;
    if (app.clock().previewRtc(draft_, after)) localStamp(app, after, aftD, sizeof aftD, aftT, sizeof aftT);
    char l1[48], l2[48];
    std::snprintf(l1, sizeof l1, "NOW    %s %s", nowD, nowT);
    std::snprintf(l2, sizeof l2, "AFTER  %s %s", aftD, aftT);
    p.text(l1, C, 292, middle_center, fontset::body, 0.75f, pal::textDim);
    p.text(l2, C, 312, middle_center, fontset::body, 0.75f, pal::red);
    p.text("VERIFY AFTER LAUNCHER RETURN", C, 334, middle_center, fontset::small, 0.75f, pal::amber);
    btn(p, 2, "BACK", 180, 368, 100, 30);
    btn(p, 3, "SAVE RTC MODE", 292, 368, 124, 30);
}

void RtcModePage::activate(int i) {
    if (i == 0) draft_ = RtcMode::Utc;
    else if (i == 1) draft_ = RtcMode::LocalJst;
    else if (i == 2) onBack();
    else {
        if (draft_ == app.settings().rtcMode) {
            app.toast("NO CHANGE");
            return app.back();
        }
        if (app.saveRtcMode(draft_)) {
            app.toast("RTC MODE SAVED");
            app.back();
        } else {
            app.toast("SAVE FAILED - MODE UNCHANGED", true);
        }
    }
}

}  // namespace cd
