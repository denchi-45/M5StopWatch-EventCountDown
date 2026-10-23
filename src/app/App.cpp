#include "App.h"
#include <esp_random.h>
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
constexpr uint8_t kDimPercent = 12;
constexpr int64_t kEventsRetryMs = 10000;
// A delayed arrival notification is still given if the record is stored this soon.
constexpr int64_t kArrivalNotifyWindowMs = 60000;
}  // namespace

App& app() { return g_app; }

// ------------------------------------------------------------------ setup

void App::setup() {
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

    // Full-screen double buffer (PSRAM when present). Without memory for the
    // background cache, the background is simply redrawn every frame.
    frame_ = new M5Canvas(&M5.Display);
    frame_->setColorDepth(16);
    frame_->setPsram(true);
    if (!frame_->createSprite(caps_.width, caps_.height)) {
        delete frame_;
        frame_ = nullptr;
    }
    if (frame_) {
        bg_ = new M5Canvas(&M5.Display);
        bg_->setColorDepth(16);
        bg_->setPsram(true);
        if (!bg_->createSprite(caps_.width, caps_.height)) {
            delete bg_;
            bg_ = nullptr;
        }
    }
    lastInputMs_ = now;
    applyBrightness();
    Serial.printf("[App] board=%s %dx%d touch=%d rtc=%d clock=%s events=%u\n", caps_.boardName,
                  caps_.width, caps_.height, caps_.touch, caps_.rtc, clockSourceText(clock_.source()),
                  unsigned(repo_.events().size()));
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
    haveTick_ = true;
    lastTickUtc_ = now;
    lastTickGen_ = gen;
    dirty_ = true;
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
    bool wantDim = settings_.dimAfterSec != 0 && now - lastInputMs_ >= int64_t(settings_.dimAfterSec) * 1000;
    if (settings_.keepAwakeOnUsb && onExternalPower()) wantDim = false;
    if (webActive_) wantDim = false;  // credentials must stay readable
    if (wantDim != dimmed_) {
        if (!wantDim && activity) swallowNext_ = true;
        dimmed_ = wantDim;
        applyBrightness();
    }
}

bool App::onExternalPower() {
    // charge_unknown (no charger status) does not count as USB power.
    if (M5.Power.isCharging() == m5::Power_Class::is_charging) return true;
    return M5.Power.getVBUSVoltage() > 4300;  // negative when not measurable
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

void App::render() {
    const int64_t now = monoMs();
    const int64_t sec = clock_.valid() ? clock_.nowUtc() : now / 1000;
    if (sec != lastDrawnSecond_) dirty_ = true;
    if (!dirty_ || now - lastFrameMs_ < kFrameMinMs) return;
    dirty_ = false;
    lastDrawnSecond_ = sec;
    lastFrameMs_ = now;

    const Layout l = screen_->layout();
    const uint32_t key = screen_->backgroundKey();
    LovyanGFX* target = frame_ ? static_cast<LovyanGFX*>(frame_) : static_cast<LovyanGFX*>(&M5.Display);

    if (bg_) {
        if (!bgValid_ || key != bgKey_ || bgScreen_ != currentId_) {
            Painter bp(*bg_, l.scale, l.ox, l.oy);
            screen_->drawBackground(bp);
            bgValid_ = true;
            bgKey_ = key;
            bgScreen_ = currentId_;
        }
        std::memcpy(frame_->getBuffer(), bg_->getBuffer(),
                    size_t(frame_->width()) * frame_->height() * 2);
    } else {
        if (!frame_) M5.Display.startWrite();
        Painter bp(*target, l.scale, l.ox, l.oy);
        screen_->drawBackground(bp);
    }
    Painter p(*target, l.scale, l.ox, l.oy);
    screen_->draw(p);

    if (const char* t = toastText()) {
        // Centre overlay; static (no flashing), dismissed after ~2 s.
        const float cx = l.designW / 2, cy = l.designH / 2;
        const float w = std::min(l.designW - 60, p.textWidth(t, fontset::small, 1) + 60);
        const Rect r{cx - w / 2, cy - 22, w, 44};
        hud::frame(p, r, 12, 2, true);
        p.text(t, cx, cy, middle_center, fontset::small, 1, toastError_ ? pal::amber : pal::text);
    }

    if (frame_) {
        M5.Display.startWrite();
        frame_->pushSprite(0, 0);
        M5.Display.endWrite();
    } else {
        M5.Display.endWrite();
    }
}

}  // namespace cd
