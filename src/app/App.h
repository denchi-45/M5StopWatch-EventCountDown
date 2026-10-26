#pragma once
// Application controller: owns the services, applies every state change in one
// place (clock, events, settings, network) and drives the screens.
// Everything runs on the UI task (main.cpp) except the NTP worker and the web
// server, which take the UI lock (hal/UiTask.h) before calling in.
#include <M5Unified.h>
#include <memory>
#include <string>
#include "../core/ClockService.h"
#include "../core/CountdownEngine.h"
#include "../core/DeviceSettings.h"
#include "../core/EventRepository.h"
#include "../core/SyncScheduler.h"
#include "../hal/DeviceCapabilities.h"
#include "../hal/RtcHal.h"
#include "../input/InputRouter.h"
#include "../services/NetworkService.h"
#include "../services/NtpSync.h"
#include "../services/SettingsStore.h"
#include "../ui/Screen.h"

namespace cd {

class WebConfigServer;

class App {
public:
    static constexpr int64_t kApIdleMs = 5 * 60 * 1000;
    static constexpr int64_t kSelectionDebounceMs = 2000;

    // framebuffer: the display keeps its own frame in memory and transfers only
    // what changed, so frames are drawn on it directly (no canvas of our own).
    // inputWake: presses end a wait, so idleMs() need not poll for them.
    void setup(bool framebuffer, bool inputWake);
    void loop();
    // How long the UI task may wait before the next loop() (its earliest deadline).
    uint32_t idleMs();
    // Automatic light sleep may run during that wait (dimmed, on battery, radio
    // off, no sound or vibration in progress).
    bool lightSleepAllowed() const;

    // ---- read access for screens and the web API
    const DeviceCapabilities& caps() const { return caps_; }
    const EventRepository& events() const { return repo_; }
    ClockService& clock() { return clock_; }
    const DeviceSettings& settings() const { return settings_; }
    NetworkService& net() { return net_; }
    NtpState ntpState() const { return ntpSched_.state(); }
    int64_t lastNtpSuccessUtc() const { return lastNtpSuccessUtc_; }
    const std::string& lastNtpError() const { return lastNtpError_; }
    const std::string& configError() const { return configError_; }
    CountdownView countdown();
    int64_t monoNow() const { return monoMs(); }

    // ---- actions (validate -> persist -> apply). False/err means nothing changed.
    bool selectPrev();
    bool selectNext();
    RepoError selectEvent(const std::string& id, bool persistNow, bool* saveFailed = nullptr);
    RepoError addEvent(const EventInput& in, std::string* id, std::string* detail, bool& saveFailed);
    RepoError updateEvent(const std::string& id, const EventInput& in, std::string* detail,
                          bool& saveFailed);
    RepoError deleteEvent(const std::string& id, bool& saveFailed);
    RepoError reorderEvents(const std::vector<std::string>& ids, bool& saveFailed);

    SetResult setClockUtcMs(int64_t utcMs, ClockSource src);
    SetResult setClockLocal(const Civil& local);  // display TZ, MANUAL
    // Saves everything except the RTC mode (that has its own confirmed flow).
    bool saveSettings(const DeviceSettings& s);
    // clockFollowed: the RTC passed the trust checks under the new mode and the
    // clock now follows it (false = the running clock was kept).
    bool saveRtcMode(RtcMode mode, bool* clockFollowed = nullptr);
    NtpScheduler::Manual requestNtpSync();
    bool saveWifi(const std::string& ssid, const std::string* password);

    void previewBrightness(uint8_t percent);
    void restoreBrightness();

    // ---- setup web mode (SoftAP + server)
    bool startWebConfig();
    void stopWebConfig();
    bool webConfigActive() const;
    void noteSetupActivity();  // real user action: extends the AP idle deadline
    int64_t apRemainingMs() const;
    bool webConfigFailed() const { return webFailed_; }

    // ---- navigation
    void navigate(ScreenId id);
    void back();
    ScreenId current() const { return currentId_; }
    void requestRedraw() { dirty_ = true; }
    void toast(const char* msg, bool error = false);
    const char* toastText() const;
    bool toastError() const { return toastError_; }

private:
    DeviceCapabilities caps_;
    RtcHal rtcHal_;
    ClockService clock_;
    SettingsStore store_;
    EventRepository repo_;
    DeviceSettings settings_;
    NetworkService net_;
    NtpSync ntp_;
    NtpScheduler ntpSched_;
    InputRouter input_;
    std::unique_ptr<WebConfigServer> web_;

    std::unique_ptr<Screen> screens_[8];
    Screen* screen_ = nullptr;
    ScreenId currentId_ = ScreenId::Countdown;
    ScreenId stack_[8];
    int depth_ = 0;

    // Rendering. frame_ is the double buffer for panels without a framebuffer
    // of their own; bg_ caches the static background.
    M5Canvas* frame_ = nullptr;
    M5Canvas* bg_ = nullptr;
    M5Canvas* live_ = nullptr;  // partial frames (Screen::liveRect)
    uint32_t bgKey_ = 0xFFFFFFFF;
    ScreenId bgScreen_ = ScreenId::Countdown;
    bool bgValid_ = false;
    uint32_t contentKey_ = 0;
    bool toastShown_ = false;
    bool dirty_ = true;  // something other than the passing of time changed
    int64_t lastDrawnSecond_ = -1;
    int64_t lastFrameMs_ = 0;
    bool inputWake_ = false;

    // Clock tick / arrivals
    bool haveTick_ = false;
    int64_t lastTickUtc_ = 0;
    uint32_t lastTickGen_ = 0;
    bool eventsSaveRetry_ = false;
    int64_t eventsRetryAt_ = 0;
    // An arrival whose record could not be stored yet; notified once the retry
    // succeeds within kArrivalNotifyWindowMs (0 = none pending).
    int64_t pendingNotifyUntil_ = 0;
    // Stored events were unreadable and could not be backed up: never overwrite them.
    bool eventsReadOnly_ = false;
    int64_t selectionSaveAt_ = 0;  // 0 = nothing pending

    // NTP
    uint32_t ntpGen_ = 0;
    int64_t lastNtpSuccessUtc_ = 0;
    std::string lastNtpError_;

    // Setup web mode
    bool webActive_ = false;
    bool webFailed_ = false;
    int64_t apDeadline_ = 0;

    // Power / notification
    int64_t lastInputMs_ = 0;
    int64_t fastPollUntil_ = 0;  // keep sampling input this long after activity
    bool usbPower_ = false;      // cached: reading it is an I2C transfer
    int64_t usbReadAt_ = 0;
    int statusLed_ = -1;         // unknown until the first USB reading sets it
    bool dimmed_ = false;
    bool swallowNext_ = false;  // the press that woke a dimmed screen
    int64_t vibrateUntil_ = 0;
    int64_t secondBeepAt_ = 0;

    std::string configError_;
    std::string toast_;
    bool toastError_ = false;
    int64_t toastUntil_ = 0;

    void loadPersisted();
    bool persistEvents();
    void onEventsPersisted(int64_t now);
    template <typename Op>
    RepoError mutateEvents(Op op, bool& saveFailed);
    void tickClock();
    void tickNetwork(int64_t now);
    void tickPower(int64_t now, bool activity);
    void notifyArrival();
    void render();
    int64_t msToNextSecond() const;
    bool liveCanvas(int w, int h);
    void applyBrightness();
    void readUsb();
    void afterClockSet(SetResult r, ClockSource src);
    Screen* screenFor(ScreenId id);
    static void launcherShutdown(void* ctx);
};

App& app();

}  // namespace cd
