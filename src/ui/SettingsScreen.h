#pragma once
// Device settings pages (spec 2.7-2.11, 2.13, 2.14) with a shared input model:
//   touch : tap selects / activates
//   A/B   : move focus (wraps); in value-edit mode decrease / increase
//   hold B: open / execute, or confirm the edited value
//   hold A: back (discarding unsaved changes), or cancel the edited value
#include <vector>
#include "../core/DateEditor.h"
#include "../core/DeviceSettings.h"
#include "Screen.h"

namespace cd {

class SettingsPage : public Screen {
public:
    SettingsPage(App& app, const char* title) : Screen(app), title_(title) {}
    Layout layout() const override;
    void drawBackground(Painter& p) override;
    void onInput(const InputEvent& e, float dx, float dy) override;

protected:
    const char* title_;
    int focus_ = 0;
    bool editing_ = false;
    std::vector<Rect> hits_;  // index == focus index, refreshed on every draw

    virtual int focusCount() const = 0;
    virtual void activate(int i) = 0;
    virtual void onTap(int i) { activate(i); }
    virtual bool editable(int) const { return false; }
    virtual void beginEdit(int) {}
    virtual void adjust(int, int) {}
    virtual void cancelEdit(int) {}
    virtual void onBack();
    // Taps outside focusable rects (e.g. +/- buttons). True if consumed.
    virtual bool onFreeTap(float, float) { return false; }

    void setHit(int i, const Rect& r);
    bool focused(int i) const { return focus_ == i; }
    // Standard row: chamfered frame, label left, value right.
    Rect row(Painter& p, int i, float cy, float w, float h, const char* label, const char* value,
             bool enabled = true);
    Rect btn(Painter& p, int i, const char* caption, float cx, float cy, float w, float h,
             bool enabled = true);
    void hint(Painter& p, float y, const char* touchText, const char* buttonText);
};

class MenuPage : public SettingsPage {
public:
    explicit MenuPage(App& app) : SettingsPage(app, "DEVICE SETTINGS") {}
    void enter(ScreenId from) override;
    void draw(Painter& p) override;

protected:
    int focusCount() const override { return 6; }
    void activate(int i) override;
};

class EventListPage : public SettingsPage {
public:
    explicit EventListPage(App& app) : SettingsPage(app, "EVENT LIST") {}
    void enter(ScreenId from) override;
    void draw(Painter& p) override;

protected:
    int first_ = 0;
    int focusCount() const override;
    void activate(int i) override;
};

class SetTimePage : public SettingsPage {
public:
    explicit SetTimePage(App& app) : SettingsPage(app, "SET TIME") {}
    void enter(ScreenId from) override;
    void draw(Painter& p) override;

protected:
    DateEditor editor_;
    Civil backup_;
    Rect minus_, plus_;
    int focusCount() const override { return 8; }
    void activate(int i) override;
    void onTap(int i) override;
    bool editable(int i) const override { return i < 6; }
    void beginEdit(int) override { backup_ = editor_.draft(); }
    void adjust(int i, int dir) override;
    void cancelEdit(int) override { editor_.load(backup_); }
    bool onFreeTap(float x, float y) override;
};

class WebConfigPage : public SettingsPage {
public:
    explicit WebConfigPage(App& app) : SettingsPage(app, "WEB CONFIG") {}
    void enter(ScreenId from) override;
    void draw(Painter& p) override;
    void onInput(const InputEvent& e, float dx, float dy) override;

protected:
    int focusCount() const override { return 1; }
    void activate(int i) override;
    void onBack() override;
};

class OptionsPage : public SettingsPage {
public:
    explicit OptionsPage(App& app) : SettingsPage(app, "DEVICE OPTIONS") {}
    void enter(ScreenId from) override;
    void draw(Painter& p) override;

protected:
    DeviceSettings draft_;
    DeviceSettings backup_;
    int focusCount() const override { return 7; }
    void activate(int i) override;
    void onTap(int i) override;
    bool editable(int i) const override;
    void beginEdit(int) override { backup_ = draft_; }
    void adjust(int i, int dir) override;
    void cancelEdit(int) override;
    void onBack() override;
};

class TimeStatusPage : public SettingsPage {
public:
    explicit TimeStatusPage(App& app) : SettingsPage(app, "TIME STATUS") {}
    void draw(Painter& p) override;

protected:
    int focusCount() const override { return 2; }
    void activate(int i) override;
};

class RtcModePage : public SettingsPage {
public:
    explicit RtcModePage(App& app) : SettingsPage(app, "RTC MODE") {}
    void enter(ScreenId from) override;
    void draw(Painter& p) override;

protected:
    RtcMode draft_ = RtcMode::Utc;
    // Cached RTC preview (previewFresh_ is cleared on enter).
    bool previewFresh_ = false;
    bool previewOk_ = false;
    RtcMode previewMode_ = RtcMode::Utc;
    int64_t previewAtMs_ = 0;
    int64_t previewUtc_ = 0;
    int focusCount() const override { return 4; }
    void activate(int i) override;
};

}  // namespace cd
