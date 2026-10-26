#include "App.h"
#include <esp_random.h>
#include <cstdio>
#include "../hal/LauncherBridge.h"
#include "../services/JsonCodec.h"
#include "../services/WebConfigServer.h"
#include "../ui/CountdownScreen.h"
#include "../ui/HudWidgets.h"
#include "../ui/SettingsScreen.h"

namespace cd {

namespace {
App g_app;
constexpr int64_t kFrameMinMs = 40;
// Input is sampled this often while anything is held, and for a while after an
// interrupt or a release (debouncing, long presses, the Launcher's A+B hold).
constexpr uint32_t kInputPollMs = 10;
constexpr int64_t kInputFollowMs = 150;
// Without input interrupts presses are only seen by polling.
constexpr uint32_t kNoWakePollMs = 20;
// Nothing waits longer than this; with the radio on, its timers and the NTP
// results are polled more often.
constexpr uint32_t kMaxIdleMs = 1000;
constexpr uint32_t kRadioIdleMs = 100;
// USB power is an I2C read, so it is sampled rather than read every loop.
constexpr int64_t kUsbReadMs = 2000;
constexpr uint8_t kDimPercent = 12;
constexpr int64_t kEventsRetryMs = 10000;
// A delayed arrival notification is still given if the record is stored this soon.
constexpr int64_t kArrivalNotifyWindowMs = 60000;
}  // namespace

App& app() { return g_app; }

// ------------------------------------------------------------------ setup

void App::setup(bool framebuffer, bool inputWake) {
    inputWake_ = inputWake;
    caps_ = DeviceCapabilities::detect();
    if (caps_.shape == ScreenShape::Rect && caps_.height > caps_.width) {
        M5.Display.setRotation(M5.Display.getRotation() ^ 1);  // landscape
        caps_ = DeviceCapabilities::detect();
    }
    store_.begin();
    loadPersisted();

    clock_.begin(&rtcHal_, settings_.rtcMode, store_.lastKnownGoodUtc());
    lastNtpSuccessUtc_ = store_.lastNtpSuccessUtc();
    const int64_t now = monoMs();
    ntpSched_.setEnabled(settings_.ntpEnabled, now);
    ntp_.begin();
    net_.begin(&store_, now);
    web_.reset(new WebConfigServer(*this));

    screens_[int(ScreenId::Countdown)].reset(new CountdownScreen(*this));
    screens_[int(ScreenId::Menu)].reset(new MenuPage(*this));
    screens_[int(ScreenId::EventList)].reset(new EventListPage(*this));
    screens_[int(ScreenId::SetTime)].reset(new SetTimePage(*this));
    screens_[int(ScreenId::WebConfig)].reset(new WebConfigPage(*this));
    screens_[int(ScreenId::Options)].reset(new OptionsPage(*this));
    screens_[int(ScreenId::TimeStatus)].reset(new TimeStatusPage(*this));
    screens_[int(ScreenId::RtcMode)].reset(new RtcModePage(*this));
    screen_ = screens_[0].get();

    // A panel without a framebuffer of its own gets a full-screen double buffer
    // (PSRAM when present), so a frame never shows half drawn. Without memory
    // for the background cache, the background is redrawn with every frame.
    auto canvas = [this]() -> M5Canvas* {
        auto* c = new M5Canvas(&M5.Display);
        c->setColorDepth(16);
        c->setPsram(true);
        if (c->createSprite(caps_.width, caps_.height)) return c;
        delete c;
        return nullptr;
    };
    if (!framebuffer) frame_ = canvas();
    bg_ = canvas();
    lastInputMs_ = now;
    readUsb();
    usbReadAt_ = now;
    applyBrightness();
    std::printf("[App] board=%s %dx%d touch=%d rtc=%d clock=%s events=%u direct=%d bg_cache=%d\n",
                caps_.boardName, caps_.width, caps_.height, caps_.touch, caps_.rtc,
                clockSourceText(clock_.source()), unsigned(repo_.events().size()), int(framebuffer),
                int(bg_ != nullptr));
}

void App::loadPersisted() {
    std::string json;
    if (store_.loadSettings(json) && !decodeSettings(json, settings_)) {
        settings_ = DeviceSettings();
        configError_ = "settings were unreadable; defaults in use";
    }
    std::vector<Event> evs;
    std::string sel, err;
    if (!store_.loadEvents(json)) json.clear();
    switch (decodeEvents(json, evs, sel, err)) {
        case LoadResult::Ok:
            repo_.assign(std::move(evs), sel);
            break;
        case LoadResult::Empty:
            break;
        case LoadResult::Migrated:
            repo_.assign(std::move(evs), sel);
            if (!store_.saveEvents(encodeEvents(repo_)))
                configError_ = "migrated events could not be saved as v3";
            break;
        case LoadResult::Corrupt:
            // Keep the unreadable data aside; the slot is reused only after the
            // copy verified. Without a backup, event changes are refused.
            if (store_.backupCorruptEvents(json)) {
                configError_ = "stored events invalid (" + err + "); kept as backup";
            } else {
                eventsReadOnly_ = true;
                configError_ = "stored events invalid (" + err + "); backup failed, events read-only";
            }
            break;
    }
}

bool App::persistEvents() { return !eventsReadOnly_ && store_.saveEvents(encodeEvents(repo_)); }

void App::onEventsPersisted(int64_t now) {
    eventsSaveRetry_ = false;
    if (pendingNotifyUntil_) {
        if (now <= pendingNotifyUntil_) notifyArrival();
        pendingNotifyUntil_ = 0;
    }
}

// ------------------------------------------------------------------ loop

void App::loop() {
    const int64_t now = monoMs();
    const bool a = M5.BtnA.isPressed(), b = M5.BtnB.isPressed();
    bool touch = false;
    int16_t tx = 0, ty = 0;
    if (caps_.touch && M5.Touch.getCount() > 0) {
        const auto d = M5.Touch.getDetail(0);
        touch = d.isPressed();
        tx = d.x;
        ty = d.y;
    }
    // Launcher return has priority; InputRouter suppresses A+B for the app.
    launcher::poll(a, b, touch, uint32_t(now), &App::launcherShutdown, this);

    const InputEvent ev = input_.update(a, b, touch, tx, ty, now);
    const bool activity = ev.type != InputType::None || input_.anyHeld();
    if (a || b || touch || activity) fastPollUntil_ = now + kInputFollowMs;
    tickPower(now, activity);
    const bool isShort = ev.type == InputType::ShortA || ev.type == InputType::ShortB ||
                         ev.type == InputType::Tap;
    bool deliver = ev.type != InputType::None;
    if (swallowNext_ && isShort) deliver = false;  // a short press on a dimmed screen only wakes it
    if (swallowNext_ && (ev.type != InputType::None || !input_.anyHeld())) swallowNext_ = false;
    if (deliver) {
        const Layout l = screen_->layout();
        const float dx = (ev.x - l.ox) / l.scale, dy = (ev.y - l.oy) / l.scale;
        screen_->onInput(ev, dx, dy);
        dirty_ = true;
    }

    tickClock();
    tickNetwork(now);

    if (web_) {
        web_->loop();
        if (web_->exitRequested() && now >= web_->exitAt()) stopWebConfig();
    }
    if (webActive_ && now >= apDeadline_) {
        stopWebConfig();
        toast("SETUP CLOSED (IDLE 5 MIN)");
    }

    // Pending selection save (2 s debounce).
    if (selectionSaveAt_ && now >= selectionSaveAt_) {
        selectionSaveAt_ = persistEvents() ? 0 : now + 5000;
    }
    if (eventsSaveRetry_ && now >= eventsRetryAt_) {
        if (persistEvents()) onEventsPersisted(now);
        else eventsRetryAt_ = now + kEventsRetryMs;
    }

    // Notification output timing (max 1 s).
    if (vibrateUntil_ && now >= vibrateUntil_) {
        M5.Power.setVibration(0);
        vibrateUntil_ = 0;
    }
    if (secondBeepAt_ && now >= secondBeepAt_) {
        M5.Speaker.tone(2400, 180);
        secondBeepAt_ = 0;
    }
    if (toastUntil_ && now >= toastUntil_) {
        toastUntil_ = 0;
        dirty_ = true;
    }

    render();
}

void App::tickClock() {
    if (!clock_.valid()) {
        haveTick_ = false;
        return;
    }
    const int64_t now = clock_.nowUtc();
    const uint32_t gen = clock_.generation();
    if (haveTick_ && now == lastTickUtc_ && gen == lastTickGen_) return;
    // Normal progress = same clock generation and moving forward. A first
    // reading, a correction or a jump never notifies (spec 5.2).
    const bool normal = haveTick_ && gen == lastTickGen_ && now > lastTickUtc_;
    bool changed = false;
    const bool notify = repo_.processArrivals(lastTickUtc_, now, normal, changed);
    if (changed) {
        // Record first; output only when the record is safely stored. The
        // arrival is already marked handled in memory, so a failed save keeps
        // the notification pending for the retry.
        const int64_t t = monoMs();
        if (notify) pendingNotifyUntil_ = t + kArrivalNotifyWindowMs;
        if (persistEvents()) {
            onEventsPersisted(t);
        } else {
            eventsSaveRetry_ = true;
            eventsRetryAt_ = t + kEventsRetryMs;
        }
    }
    // A new second alone is drawn by render() (usually as a partial frame); a
    // new clock generation may change anything on screen.
    if (!haveTick_ || gen != lastTickGen_ || changed) dirty_ = true;
    haveTick_ = true;
    lastTickUtc_ = now;
    lastTickGen_ = gen;
}

void App::notifyArrival() {
    const int64_t now = monoMs();
    if (settings_.vibration && caps_.vibration) {
        M5.Power.setVibration(200);
        vibrateUntil_ = now + 800;
    }
    if (settings_.sound && caps_.speaker) {
        M5.Speaker.tone(2400, 180);
        secondBeepAt_ = now + 400;
    }
    lastInputMs_ = now;  // wake the screen
}

void App::launcherShutdown(void*) {
    M5.Speaker.stop();
    M5.Speaker.end();
    M5.Power.setVibration(0);
}

void App::tickNetwork(int64_t now) {
    const int edge = net_.loop(now);
    if (edge != 0 && ntpSched_.setConnected(edge > 0, now)) ++ntpGen_;
    if (edge != 0) dirty_ = true;

    // A cancelled request may still occupy the worker (e.g. a slow DNS lookup);
    // wait for it instead of starting and counting a failure.
    const bool workerBusy = !ntpSched_.inFlight() && ntp_.busy();
    switch (workerBusy ? NtpScheduler::Tick::None : ntpSched_.poll(now)) {
        case NtpScheduler::Tick::Start:
            ++ntpGen_;
            if (!ntp_.request(ntpGen_, settings_.ntpServer)) {
                ntpSched_.onFailure(now);
                lastNtpError_ = "previous request still running";
            }
            break;
        case NtpScheduler::Tick::TimedOut:
            ++ntpGen_;  // a late answer must not be applied
            lastNtpError_ = "timeout";
            break;
        default: break;
    }
    NtpResult r;
    while (ntp_.poll(r)) {
        if (r.generation != ntpGen_ || !ntpSched_.inFlight()) continue;  // stale
        const int64_t t = monoMs();
        if (r.ok) {
            const SetResult sr = clock_.setUtcMs(r.utcMs + (t - r.monoMsAtRx), ClockSource::Ntp);
            if (sr == SetResult::Invalid) {
                ntpSched_.onFailure(t);
                lastNtpError_ = "invalid server time";
            } else {
                afterClockSet(sr, ClockSource::Ntp);
                ntpSched_.onSuccess(t);
                lastNtpError_.clear();
                lastNtpSuccessUtc_ = clock_.nowUtc();
                store_.saveLastNtpSuccessUtc(lastNtpSuccessUtc_);
            }
        } else {
            ntpSched_.onFailure(t);
            lastNtpError_ = r.error;
        }
        dirty_ = true;
    }
}

void App::tickPower(int64_t now, bool activity) {
    if (activity) lastInputMs_ = now;
    if (now - usbReadAt_ >= kUsbReadMs) {
        readUsb();
        usbReadAt_ = now;
    }
    bool wantDim = settings_.dimAfterSec != 0 && now - lastInputMs_ >= int64_t(settings_.dimAfterSec) * 1000;
    if (settings_.keepAwakeOnUsb && usbPower_) wantDim = false;
    if (webActive_) wantDim = false;  // credentials must stay readable
    if (wantDim != dimmed_) {
        if (!wantDim && activity) swallowNext_ = true;
        dimmed_ = wantDim;
        applyBrightness();
    }
}

void App::readUsb() {
    // charge_unknown (no charger status) does not count as USB power.
    const bool charging = M5.Power.isCharging() == m5::Power_Class::is_charging;
    const bool vbus = M5.Power.getVBUSVoltage() > 4300;  // negative when not measurable
    usbPower_ = charging || vbus;
    // The green LED means charging and nothing else, as in wararyoLauncher:
    // M5PM1 lights it at power-on whatever the state. PWR_CFG (0x06) bit 4 is
    // LED_EN; the same register switches the rails, so read-modify-write. A
    // failed write is sent again at the next reading.
    if (M5.getBoard() != m5::board_t::board_M5StopWatch) return;
    const bool led = charging && vbus;
    if (int(led) == statusLed_) return;
    constexpr uint8_t kPm1 = 0x6e, kPwrCfg = 0x06, kLedEn = 0x10;
    const bool ok = led ? M5.In_I2C.bitOn(kPm1, kPwrCfg, kLedEn, 100000)
                        : M5.In_I2C.bitOff(kPm1, kPwrCfg, kLedEn, 100000);
    if (ok) statusLed_ = led;
}

void App::applyBrightness() {
    const uint8_t pct = dimmed_ ? std::min<uint8_t>(kDimPercent, settings_.brightness) : settings_.brightness;
    M5.Display.setBrightness(uint8_t(pct * 255 / 100));
}

void App::previewBrightness(uint8_t percent) { M5.Display.setBrightness(uint8_t(percent * 255 / 100)); }
void App::restoreBrightness() { applyBrightness(); }

// ------------------------------------------------------------------ countdown

CountdownView App::countdown() {
    const Event* e = repo_.selected();
    if (!e) return computeCountdown(false, clock_.valid(), 0, 0, 1);
    return computeCountdown(true, clock_.valid(), clock_.valid() ? clock_.nowUtc() : 0, e->startUtc,
                            e->targetUtc);
}

// ------------------------------------------------------------------ actions

bool App::selectPrev() {
    if (!repo_.selectPrev()) return false;
    selectionSaveAt_ = monoMs() + kSelectionDebounceMs;
    return true;
}
bool App::selectNext() {
    if (!repo_.selectNext()) return false;
    selectionSaveAt_ = monoMs() + kSelectionDebounceMs;
    return true;
}

RepoError App::selectEvent(const std::string& id, bool persistNow, bool* saveFailed) {
    if (saveFailed) *saveFailed = false;
    const std::string before = repo_.selectedId();
    const RepoError r = repo_.select(id);
    if (r != RepoError::Ok) return r;
    if (persistNow) {
        if (!persistEvents()) {
            repo_.select(before);
            if (saveFailed) *saveFailed = true;
            return r;
        }
        selectionSaveAt_ = 0;
    } else {
        selectionSaveAt_ = monoMs() + kSelectionDebounceMs;
    }
    dirty_ = true;
    return RepoError::Ok;
}

template <typename Op>
RepoError App::mutateEvents(Op op, bool& saveFailed) {
    saveFailed = false;
    const EventRepository snapshot = repo_;
    const RepoError r = op(repo_);
    if (r != RepoError::Ok) return r;
    if (!persistEvents()) {
        repo_ = snapshot;
        saveFailed = true;
        return r;
    }
    selectionSaveAt_ = 0;
    dirty_ = true;
    return r;
}

RepoError App::addEvent(const EventInput& in, std::string* id, std::string* detail, bool& saveFailed) {
    return mutateEvents(
        [&](EventRepository& r) { return r.add(in, [] { return esp_random(); }, id, detail); },
        saveFailed);
}

RepoError App::updateEvent(const std::string& id, const EventInput& in, std::string* detail,
                           bool& saveFailed) {
    return mutateEvents([&](EventRepository& r) { return r.update(id, in, detail); }, saveFailed);
}

RepoError App::deleteEvent(const std::string& id, bool& saveFailed) {
    // remove() includes the selection fallback.
    return mutateEvents([&](EventRepository& r) { return r.remove(id); }, saveFailed);
}

RepoError App::reorderEvents(const std::vector<std::string>& ids, bool& saveFailed) {
    return mutateEvents([&](EventRepository& r) { return r.reorder(ids); }, saveFailed);
}

void App::afterClockSet(SetResult r, ClockSource src) {
    if (r == SetResult::Invalid) return;
    store_.saveLastKnownGoodUtc(clock_.lastEstablishedUtc());
    if (src == ClockSource::Manual || src == ClockSource::Browser) {
        // Pending NTP answers are older than this explicit setting.
        if (ntpSched_.clockOverridden(monoMs())) ++ntpGen_;
    }
    dirty_ = true;
}

SetResult App::setClockUtcMs(int64_t utcMs, ClockSource src) {
    const SetResult r = clock_.setUtcMs(utcMs, src);
    afterClockSet(r, src);
    return r;
}

SetResult App::setClockLocal(const Civil& local) {
    int64_t utc;
    if (!localToUtc(local, settings_.displayTz, utc)) return SetResult::Invalid;
    return setClockUtcMs(utc * 1000, ClockSource::Manual);
}

bool App::saveSettings(const DeviceSettings& in) {
    DeviceSettings s = in;
    s.rtcMode = settings_.rtcMode;  // changed only through saveRtcMode()
    if (!caps_.speaker) s.sound = false;
    if (!caps_.vibration) s.vibration = false;
    if (!validNtpServer(s.ntpServer)) return false;
    if (!store_.saveSettings(encodeSettings(s))) return false;
    const DeviceSettings old = settings_;
    settings_ = s;
    const int64_t now = monoMs();
    if (old.ntpEnabled != s.ntpEnabled) {
        ntpSched_.setEnabled(s.ntpEnabled, now);
        ++ntpGen_;
    } else if (std::strcmp(old.ntpServer, s.ntpServer) != 0) {
        if (ntpSched_.configChanged(now)) ++ntpGen_;
    }
    applyBrightness();
    dirty_ = true;
    return true;
}

bool App::saveRtcMode(RtcMode mode, bool* clockFollowed) {
    DeviceSettings s = settings_;
    s.rtcMode = mode;
    // Persist first; on failure neither the mode nor the clock changes.
    if (!store_.saveSettings(encodeSettings(s))) return false;
    settings_ = s;
    const bool followed = clock_.applyRtcMode(mode);
    if (clockFollowed) *clockFollowed = followed;
    if (ntpSched_.clockOverridden(monoMs())) ++ntpGen_;
    if (clock_.valid()) store_.saveLastKnownGoodUtc(clock_.lastEstablishedUtc());
    dirty_ = true;
    return true;
}

NtpScheduler::Manual App::requestNtpSync() { return ntpSched_.requestManual(monoMs()); }

bool App::saveWifi(const std::string& ssid, const std::string* password) {
    if (!store_.saveWifi(ssid, password)) return false;
    net_.credentialsChanged(monoMs());
    if (ntpSched_.setConnected(false, monoMs())) ++ntpGen_;
    return true;
}

// ------------------------------------------------------------------ web setup

bool App::startWebConfig() {
    const int64_t now = monoMs();
    webFailed_ = false;
    if (!webActive_) {
        if (!net_.startAp(now)) {
            webFailed_ = true;
            return false;
        }
        web_->start();
        webActive_ = true;
    }
    apDeadline_ = now + kApIdleMs;
    dirty_ = true;
    return true;
}

void App::stopWebConfig() {
    if (!webActive_) return;
    web_->stop();
    net_.stopAp();  // STA link and clock keep running
    webActive_ = false;
    if (currentId_ == ScreenId::WebConfig) back();
    dirty_ = true;
}

bool App::webConfigActive() const { return webActive_; }

void App::noteSetupActivity() {
    if (webActive_) apDeadline_ = monoMs() + kApIdleMs;
}

int64_t App::apRemainingMs() const {
    if (!webActive_) return 0;
    const int64_t r = apDeadline_ - monoMs();
    return r > 0 ? r : 0;
}

// ------------------------------------------------------------------ navigation

Screen* App::screenFor(ScreenId id) { return screens_[int(id)].get(); }

void App::navigate(ScreenId id) {
    if (id == currentId_) return;
    if (depth_ < int(sizeof stack_ / sizeof stack_[0])) stack_[depth_++] = currentId_;
    const ScreenId from = currentId_;
    screen_->leave();
    currentId_ = id;
    screen_ = screenFor(id);
    screen_->enter(from);
    dirty_ = true;
}

void App::back() {
    const ScreenId from = currentId_;
    const ScreenId to = depth_ > 0 ? stack_[--depth_] : ScreenId::Countdown;
    screen_->leave();
    currentId_ = to;
    screen_ = screenFor(to);
    screen_->enter(from);
    dirty_ = true;
}

void App::toast(const char* msg, bool error) {
    toast_ = msg;
    toastError_ = error;
    toastUntil_ = monoMs() + 2200;
    dirty_ = true;
}

const char* App::toastText() const { return toastUntil_ ? toast_.c_str() : nullptr; }

// ------------------------------------------------------------------ rendering

int64_t App::msToNextSecond() const {
    const int64_t ms = clock_.valid() ? clock_.nowUtcMs() : monoMs();
    return 1000 - ((ms % 1000) + 1000) % 1000;
}

// Frames come in two kinds. A full frame redraws everything; it follows any
// change other than time (input, navigation, toasts, data, a new background
// or content key). When only the second changed, the screen's liveRect() is
// redrawn alone (background and the whole draw() into a canvas of that area,
// so it matches a full frame pixel for pixel) and only that area is
// transferred. Screens that do not change with time get no frame.
// tools/preview checks the partial frames against full ones.
void App::render() {
    const int64_t now = monoMs();
    const int64_t sec = clock_.valid() ? clock_.nowUtc() : now / 1000;
    const bool tick = sec != lastDrawnSecond_;
    if ((!dirty_ && !tick) || now - lastFrameMs_ < kFrameMinMs) return;

    const Layout l = screen_->layout();
    const uint32_t key = screen_->backgroundKey(), content = screen_->contentKey();
    const bool bgChanged = !bgValid_ || key != bgKey_ || bgScreen_ != currentId_;
    const char* toastMsg = toastText();
    bool full = dirty_ || bgChanged || content != contentKey_ || (toastMsg != nullptr) != toastShown_;
    Rect live;
    if (!full) {
        if (!screen_->ticks()) {
            lastDrawnSecond_ = sec;
            return;
        }
        full = toastMsg || !screen_->liveRect(live);
    }
    dirty_ = false;
    lastDrawnSecond_ = sec;
    lastFrameMs_ = now;
    contentKey_ = content;
    toastShown_ = toastMsg != nullptr;

    if (bg_ && bgChanged) {
        Painter bp(*bg_, l.scale, l.ox, l.oy);
        screen_->drawBackground(bp);
        bgValid_ = true;
        bgKey_ = key;
        bgScreen_ = currentId_;
    }

    LovyanGFX* target = frame_ ? static_cast<LovyanGFX*>(frame_) : static_cast<LovyanGFX*>(&M5.Display);
    int cx = 0, cy = 0;
    if (!full) {
        // The live area in panel pixels, with a pixel of margin for rounding.
        const Painter m(*target, l.scale, l.ox, l.oy);
        cx = m.X(live.x) - 1;
        cy = m.Y(live.y) - 1;
        full = !liveCanvas(m.X(live.x + live.w) - cx + 2, m.Y(live.y + live.h) - cy + 2);
    }

    M5.Display.startWrite();
    if (full) {
        if (bg_) {
            bg_->pushSprite(target, 0, 0);
        } else {
            Painter bp(*target, l.scale, l.ox, l.oy);
            screen_->drawBackground(bp);
        }
        Painter p(*target, l.scale, l.ox, l.oy);
        screen_->draw(p);
        if (toastMsg) {
            // Centre overlay; static (no flashing), dismissed after ~2 s.
            const float tx = l.designW / 2, ty = l.designH / 2;
            const float w = std::min(l.designW - 60, p.textWidth(toastMsg, fontset::small, 1) + 60);
            const Rect r{tx - w / 2, ty - 22, w, 44};
            hud::frame(p, r, 12, 2, true);
            p.text(toastMsg, tx, ty, middle_center, fontset::small, 1, toastError_ ? pal::amber : pal::text);
        }
        if (frame_) frame_->pushSprite(&M5.Display, 0, 0);
    } else {
        // Drawn into a canvas of exactly the live area, not through a clip
        // rectangle: M5GFX's drawWideLine() replaces the clip with its own
        // bounds and clears it afterwards, so later drawing would leak out.
        if (bg_) {
            bg_->pushSprite(live_, -cx, -cy);
        } else {
            Painter bp(*live_, l.scale, l.ox - cx, l.oy - cy);
            screen_->drawBackground(bp);
        }
        Painter p(*live_, l.scale, l.ox - cx, l.oy - cy);
        screen_->draw(p);
        live_->pushSprite(target, cx, cy);
        if (frame_) live_->pushSprite(&M5.Display, cx, cy);
    }
    // A framebuffer panel transfers only the area written since startWrite().
    M5.Display.endWrite();
}

bool App::liveCanvas(int w, int h) {
    if (live_ && live_->width() == w && live_->height() == h) return true;
    if (!live_) {
        live_ = new M5Canvas(&M5.Display);
        live_->setColorDepth(16);
    }
    live_->deleteSprite();
    // Small enough for internal RAM, which draws faster than PSRAM.
    live_->setPsram(false);
    if (live_->createSprite(w, h)) return true;
    live_->setPsram(true);
    return live_->createSprite(w, h) != nullptr;
}

// ------------------------------------------------------------------ waiting

uint32_t App::idleMs() {
    const int64_t now = monoMs();
    if (!inputWake_) return kNoWakePollMs;
    if (now < fastPollUntil_ || input_.anyHeld()) return kInputPollMs;
    int64_t wait = webActive_ || net_.radioOn() ? kRadioIdleMs : kMaxIdleMs;
    auto until = [&](int64_t deadline) {
        if (deadline) wait = std::min(wait, deadline - now);
    };
    if (dirty_) until(lastFrameMs_ + kFrameMinMs);
    // The next second, when the screen shows time (or may change state).
    wait = std::min(wait, msToNextSecond() + 1);
    until(toastUntil_);
    until(vibrateUntil_);
    until(secondBeepAt_);
    until(selectionSaveAt_);
    if (eventsSaveRetry_) until(eventsRetryAt_);
    if (webActive_) until(apDeadline_);
    if (web_ && web_->exitRequested()) until(web_->exitAt());
    if (!dimmed_ && settings_.dimAfterSec != 0)
        until(lastInputMs_ + int64_t(settings_.dimAfterSec) * 1000);
    return uint32_t(std::max<int64_t>(1, wait));
}

bool App::lightSleepAllowed() const {
    // Only where the wake-up is certain (input interrupts) and nothing needs
    // the CPU or a peripheral between frames: the USB console needs the bus,
    // the radio its own timing, the speaker and the vibration motor their output.
    return inputWake_ && dimmed_ && !usbPower_ && !webActive_ && !net_.radioOn() && !vibrateUntil_ &&
           !secondBeepAt_ && !M5.Speaker.isPlaying() && !input_.anyHeld() && monoMs() >= fastPollUntil_;
}

}  // namespace cd
