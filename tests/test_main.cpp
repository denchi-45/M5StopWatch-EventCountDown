// Host unit tests for the hardware-independent core (spec chapter 9).
// Build & run: tests/run_tests.sh (g++ >= 9, C++17).
#include <cmath>
#include <cstdio>
#include <string>
#include <vector>
#include "../src/core/ClockService.h"
#include "../src/core/CountdownEngine.h"
#include "../src/core/DateEditor.h"
#include "../src/core/DeviceSettings.h"
#include "../src/core/EventRepository.h"
#include "../src/core/SyncScheduler.h"
#include "../src/core/TimeMath.h"
#include "../src/core/Utf8.h"
#include "../src/input/InputRouter.h"
#include "../src/services/JsonCodec.h"

using namespace cd;

static int g_fail = 0, g_pass = 0;
#define CHECK(c)                                                                  \
    do {                                                                          \
        if (c) ++g_pass;                                                          \
        else { ++g_fail; std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #c); } \
    } while (0)

static int64_t jst(int y, int mo, int d, int h = 0, int mi = 0, int s = 0) {
    int64_t t = 0;
    localToUtc(Civil{y, mo, d, h, mi, s}, Tz::AsiaTokyo, t);
    return t;
}

static uint32_t g_rnd = 1;
static uint32_t rnd() { return g_rnd = g_rnd * 1103515245u + 12345u; }

static EventInput input(const char* name, int64_t s, int64_t t, bool enabled = true) {
    EventInput in;
    in.name = name;
    in.startUtc = s;
    in.targetUtc = t;
    in.enabled = enabled;
    return in;
}

// ---------------------------------------------------------------- time math
static void testTimeMath() {
    CHECK(civilToEpoch({2000, 1, 1, 0, 0, 0}) == kMinUtc);
    CHECK(civilToEpoch({2099, 12, 31, 23, 59, 59}) == kMaxUtc);
    CHECK(epochToCivil(kMaxUtc) == (Civil{2099, 12, 31, 23, 59, 59}));
    // Sample config: 2026-10-01T00:00 JST == 1790780400, 2026-11-15T10:00 JST == 1794704400.
    CHECK(jst(2026, 10, 1) == 1790780400LL);
    CHECK(jst(2026, 11, 15, 10) == 1794704400LL);
    CHECK(jst(2026, 10, 23) == 1792681200LL);
    CHECK(jst(2026, 12, 13, 10) == 1797123600LL);
    // JST and UTC inputs of the same instant give the same seconds.
    int64_t u;
    CHECK(localToUtc({2026, 11, 15, 1, 0, 0}, Tz::Utc, u) && u == jst(2026, 11, 15, 10));
    // Leap years and month ends.
    CHECK(daysInMonth(2024, 2) == 29 && daysInMonth(2026, 2) == 28 && daysInMonth(2000, 2) == 29);
    CHECK(daysInMonth(2100, 2) == 28);
    CHECK(!validCivil({2026, 2, 29, 0, 0, 0}) && validCivil({2028, 2, 29, 0, 0, 0}));
    CHECK(!validCivil({2026, 4, 31, 0, 0, 0}) && !validCivil({2026, 1, 1, 24, 0, 0}));
    CHECK(!validCivil({1999, 12, 31, 0, 0, 0}) && !validCivil({2100, 1, 1, 0, 0, 0}));
    Civil c;
    CHECK(parseLocalIso("2026-11-15T10:00:00", c) && c == (Civil{2026, 11, 15, 10, 0, 0}));
    CHECK(parseLocalIso("2026-11-15T10:00", c) && c.second == 0);
    CHECK(!parseLocalIso("2026-02-30T10:00:00", c));
    CHECK(!parseLocalIso("2026-1-15T10:00:00", c));
    CHECK(!parseLocalIso("2026-11-15 10:00:00", c));
    CHECK(weekdayFromDays(daysFromCivil(2026, 10, 23)) == 5);  // Friday
}

// ---------------------------------------------------------------- countdown
static void testNtpEra() {
    // 2026-10-23T00:00:00Z, era 0.
    CHECK(ntpToUnixMs(uint32_t(1792713600LL + 2208988800LL), 0) == 1792713600000LL);
    CHECK(ntpToUnixMs(uint32_t(946684800LL + 2208988800LL), 0x80000000UL) == 946684800500LL);
    // The 32-bit seconds wrap on 2036-02-07T06:28:16Z.
    CHECK(ntpToUnixMs(0xFFFFFFFFUL, 0) == 2085978495000LL);
    CHECK(ntpToUnixMs(0, 0) == 2085978496000LL);
    // 2038-02-01T00:00:00Z (era 1) and 2099-12-31T23:59:59Z stay in range.
    CHECK(ntpToUnixMs(uint32_t(2148595200LL + 2208988800LL - 0x100000000LL), 0) == 2148595200000LL);
    CHECK(inUtcRange(ntpToUnixMs(uint32_t(kMaxUtc + 2208988800LL - 0x100000000LL), 0) / 1000));
}

static void testCountdown() {
    const int64_t s = 1000000000, t = s + 100 * 86400;
    auto v = computeCountdown(false, true, s, s, t);
    CHECK(v.state == CountdownState::NoEvents);
    v = computeCountdown(true, false, s, s, t);
    CHECK(v.state == CountdownState::TimeNotSet && v.remaining == 0);
    // Before start: 0 %, 0 segments.
    v = computeCountdown(true, true, s - 50, s, t);
    CHECK(v.progress == 0.0 && v.litSegments == 0 && v.state == CountdownState::Active);
    v = computeCountdown(true, true, s, s, t);
    CHECK(v.litSegments == 0);
    // 50 %: 26 segments (no integer-division truncation).
    v = computeCountdown(true, true, s + 50 * 86400, s, t);
    CHECK(std::fabs(v.progress - 0.5) < 1e-12 && v.litSegments == 26);
    // Mid-way values are not truncated to 0.
    v = computeCountdown(true, true, s + 1, s, t);
    CHECK(v.progress > 0.0);
    // Days / h:m:s split.
    const int64_t now = t - (8 * 86400 + 12 * 3600 + 34 * 60 + 56);
    v = computeCountdown(true, true, now, s, t);
    CHECK(v.days == 8 && v.hours == 12 && v.minutes == 34 && v.seconds == 56);
    // Final hour.
    v = computeCountdown(true, true, t - 3599, s, t);
    CHECK(v.state == CountdownState::FinalCountdown);
    v = computeCountdown(true, true, t - 3600, s, t);
    CHECK(v.state == CountdownState::Active);
    // Just before target: ring may be full, state is not "started".
    v = computeCountdown(true, true, t - 1, s, t);
    CHECK(v.litSegments == 52 && v.state == CountdownState::FinalCountdown && v.remaining == 1);
    // Reached and after: 0, 100 %.
    v = computeCountdown(true, true, t, s, t);
    CHECK(v.state == CountdownState::EventStarted && v.remaining == 0 && v.litSegments == 52);
    v = computeCountdown(true, true, t + 99999, s, t);
    CHECK(v.remaining == 0 && v.days == 0 && v.hours == 0 && v.progress == 1.0);
    // Clock corrected backwards: countdown resumes.
    v = computeCountdown(true, true, t - 10, s, t);
    CHECK(v.state == CountdownState::FinalCountdown && v.remaining == 10);
    // Four-digit days.
    v = computeCountdown(true, true, kMinUtc, kMinUtc, kMinUtc + 1500LL * 86400);
    CHECK(v.days == 1500);
}

// ---------------------------------------------------------------- utf8 / names
static void testUtf8() {
    std::string out;
    CHECK(validateEventName("3DPの集い 2026", out) == NameError::Ok && out == "3DPの集い 2026");
    CHECK(validateEventName("  HoriLab 出店  ", out) == NameError::Ok && out == "HoriLab 出店");
    CHECK(validateEventName("   ", out) == NameError::Empty);
    CHECK(validateEventName("", out) == NameError::Empty);
    CHECK(validateEventName(std::string("\xC0\xAF"), out) == NameError::InvalidUtf8);   // overlong
    CHECK(validateEventName(std::string("\xED\xA0\x80"), out) == NameError::InvalidUtf8);  // surrogate
    CHECK(validateEventName(std::string("abc\xE3\x81"), out) == NameError::InvalidUtf8);  // truncated
    CHECK(validateEventName(std::string("a\x01" "b"), out) == NameError::ControlChar);
    std::string kana48, kana49;
    for (int i = 0; i < 48; ++i) kana48 += "あ";  // 48 cp, 144 bytes
    kana49 = kana48 + "あ";
    CHECK(validateEventName(kana48, out) == NameError::Ok);
    CHECK(validateEventName(kana49, out) == NameError::TooLong);
    std::string emoji48;
    for (int i = 0; i < 48; ++i) emoji48 += "\xF0\x9F\x9A\x80";  // 192 bytes
    CHECK(validateEventName(emoji48, out) == NameError::Ok);
}

// ---------------------------------------------------------------- repository
static void testRepository() {
    EventRepository r;
    const int64_t s = jst(2026, 10, 1), t = jst(2026, 11, 15, 10);
    std::string id;
    CHECK(r.add(input("A", t, s), rnd, &id) == RepoError::StartNotBeforeTarget);
    CHECK(r.add(input("A", s, s), rnd, &id) == RepoError::StartNotBeforeTarget);
    CHECK(r.add(input("A", 0, s), rnd, &id) == RepoError::OutOfRange);
    CHECK(r.add(input(" ", s, t), rnd, &id) == RepoError::BadName);
    CHECK(r.events().empty() && r.selectedId().empty());

    // One event: selection stays put.
    std::string a;
    CHECK(r.add(input("A", s, t), rnd, &a) == RepoError::Ok);
    CHECK(r.selectedId() == a);
    CHECK(!r.selectNext() && !r.selectPrev() && r.selectedId() == a);

    // Ten events, cycling both ways with wrap-around.
    std::vector<std::string> ids{a};
    for (int i = 1; i < 10; ++i) {
        std::string x;
        CHECK(r.add(input("E", s, t + i), rnd, &x) == RepoError::Ok);
        ids.push_back(x);
    }
    CHECK(r.add(input("X", s, t), rnd, &id) == RepoError::Full);
    for (int i = 1; i <= 10; ++i) {
        r.selectNext();
        CHECK(r.selectedId() == ids[size_t(i % 10)]);
    }
    r.selectPrev();
    CHECK(r.selectedId() == ids[9]);
    r.selectNext();
    CHECK(r.selectedId() == ids[0]);

    // Disabled events are skipped.
    EventInput dis = input("E", s, t + 1, false);
    CHECK(r.update(ids[1], dis) == RepoError::Ok);
    r.selectNext();
    CHECK(r.selectedId() == ids[2]);
    r.selectPrev();
    CHECK(r.selectedId() == ids[0]);
    CHECK(r.select(ids[1]) == RepoError::Disabled);

    // Deleting the selected event falls back to the first enabled one.
    r.select(ids[5]);
    CHECK(r.remove(ids[5]) == RepoError::Ok && r.selectedId() == ids[0]);
    // Disabling the selected one: fallback as well.
    r.select(ids[0]);
    CHECK(r.update(ids[0], input("A", s, t, false)) == RepoError::Ok);
    CHECK(r.selectedId() == ids[2]);

    // Reorder must be an exact permutation.
    std::vector<std::string> order;
    for (const auto& e : r.events()) order.insert(order.begin(), e.id);
    CHECK(r.reorder(order) == RepoError::Ok && r.events().front().id == order.front());
    auto dup = order;
    dup[1] = dup[0];
    CHECK(r.reorder(dup) == RepoError::BadOrder);
    order.pop_back();
    CHECK(r.reorder(order) == RepoError::BadOrder);

    // All disabled: no selection (NO EVENTS).
    EventRepository r2;
    std::string b;
    r2.add(input("B", s, t, false), rnd, &b);
    CHECK(r2.selectedId().empty() && r2.selected() == nullptr && r2.enabledCount() == 0);

    // Corrupt stored selection falls back on assign().
    std::vector<Event> evs = r.events();
    r2.assign(evs, "evt-missing");
    CHECK(r2.selected() != nullptr);
}

// ---------------------------------------------------------------- notification
static void testArrival() {
    EventRepository r;
    const int64_t s = jst(2026, 10, 1), t = jst(2026, 11, 15, 10);
    std::string a, b;
    r.add(input("A", s, t), rnd, &a);
    r.add(input("B", s, t + 5), rnd, &b);
    bool changed;
    // Normal progress across the target: notify once.
    CHECK(!r.processArrivals(t - 2, t - 1, true, changed) && !changed);
    CHECK(r.processArrivals(t - 1, t, true, changed) && changed);
    CHECK(!r.processArrivals(t, t + 1, true, changed) && !changed);
    // Clock back before the target and forward again: no re-notification.
    CHECK(!r.processArrivals(t + 1, t - 100, false, changed));
    CHECK(!r.processArrivals(t - 1, t, true, changed));
    // Non-selected event reached: marked silently.
    CHECK(!r.processArrivals(t + 4, t + 5, true, changed) && changed);
    CHECK(r.find(b)->handled());
    // Editing the target resets the record; editing the name does not.
    EventInput in = input("A2", s, t);
    r.update(a, in);
    CHECK(r.find(a)->handled());
    in.targetUtc = t + 100;
    r.update(a, in);
    CHECK(!r.find(a)->handled());
    CHECK(r.processArrivals(t + 99, t + 100, true, changed));
    // Edited to a past target: marked without notification.
    in.targetUtc = t + 50;
    r.update(a, in);
    CHECK(!r.processArrivals(t + 100, t + 101, true, changed) && changed && r.find(a)->handled());

    // Start-up / NTP jump past the target: no notification.
    EventRepository r2;
    std::string c;
    r2.add(input("C", s, t), rnd, &c);
    CHECK(!r2.processArrivals(t - 10, t + 10, false, changed) && changed);
    CHECK(r2.find(c)->handled());
    // Disabled events are marked silently too.
    EventRepository r3;
    std::string d;
    r3.add(input("D", s, t, false), rnd, &d);
    CHECK(!r3.processArrivals(t - 1, t, true, changed) && r3.find(d)->handled());
}

// ---------------------------------------------------------------- clock / RTC
struct FakeHal : IClockHal {
    bool present = true, writeOk = true;
    Civil rtc{2000, 1, 1, 0, 0, 0};
    int64_t sysUs = 0;
    int writes = 0;
    bool rtcPresent() override { return present; }
    bool readRtc(Civil& out) override { out = rtc; return present; }
    bool writeRtc(const Civil& c) override {
        ++writes;
        if (writeOk) rtc = c;
        return writeOk;
    }
    void setSystemUtcUs(int64_t us) override { sysUs = us; }
    int64_t systemUtcUs() override { return sysUs; }
};

static void testClock() {
    const int64_t instant = jst(2026, 10, 23, 5, 18, 0);
    // Cleared RTC: invalid, no fictitious countdown.
    {
        FakeHal h;
        ClockService c;
        c.begin(&h, RtcMode::Utc, 0);
        CHECK(!c.valid() && c.source() == ClockSource::Invalid && c.rtcAvailable() && !c.rtcValid());
    }
    // Both modes: set via each source, reboot, same instant, no double +9 h.
    for (RtcMode mode : {RtcMode::Utc, RtcMode::LocalJst}) {
        for (ClockSource src : {ClockSource::Ntp, ClockSource::Manual, ClockSource::Browser}) {
            FakeHal h;
            ClockService c;
            c.begin(&h, mode, 0);
            CHECK(c.setUtcMs(instant * 1000 + 250, src) == SetResult::Ok);
            CHECK(c.valid() && c.nowUtc() == instant && c.source() == src);
            CHECK(c.rtcWriteStatus() == RtcWriteStatus::Ok);
            const Civil expect = mode == RtcMode::Utc ? Civil{2026, 10, 22, 20, 18, 0}
                                                      : Civil{2026, 10, 23, 5, 18, 0};
            CHECK(h.rtc == expect);
            ClockService reboot;
            h.sysUs = 0;
            reboot.begin(&h, mode, c.lastEstablishedUtc());
            CHECK(reboot.valid() && reboot.nowUtc() == instant && reboot.source() == ClockSource::Rtc);
            // Booting again (e.g. after a Launcher round trip) does not drift.
            ClockService again;
            again.begin(&h, mode, 0);
            CHECK(again.nowUtc() == instant);
        }
    }
    // Launcher stores UTC: reading it as UTC matches.
    {
        FakeHal h;
        h.rtc = {2026, 10, 22, 20, 18, 0};
        ClockService c;
        c.begin(&h, RtcMode::Utc, 0);
        CHECK(c.valid() && c.nowUtc() == instant);
    }
    // RTC write failure: internal clock kept, failure reported.
    {
        FakeHal h;
        h.writeOk = false;
        ClockService c;
        c.begin(&h, RtcMode::Utc, 0);
        CHECK(c.setUtcMs(instant * 1000, ClockSource::Manual) == SetResult::RtcWriteFailed);
        CHECK(c.valid() && c.nowUtc() == instant && c.rtcWriteStatus() == RtcWriteStatus::Failed);
    }
    // No RTC: clock still works for this run.
    {
        FakeHal h;
        h.present = false;
        ClockService c;
        c.begin(&h, RtcMode::Utc, 0);
        CHECK(!c.valid() && !c.rtcAvailable());
        CHECK(c.setUtcMs(instant * 1000, ClockSource::Browser) == SetResult::RtcWriteFailed);
        CHECK(c.valid() && c.rtcWriteStatus() == RtcWriteStatus::Unavailable);
    }
    // RTC older than last known good: untrusted.
    {
        FakeHal h;
        h.rtc = {2024, 6, 1, 0, 0, 0};
        ClockService c;
        c.begin(&h, RtcMode::Utc, instant);
        CHECK(!c.valid());
    }
    // Out-of-range sets are rejected; generation bumps on every set.
    {
        FakeHal h;
        ClockService c;
        c.begin(&h, RtcMode::Utc, 0);
        const uint32_t g = c.generation();
        CHECK(c.setUtcMs((kMaxUtc + 1) * 1000, ClockSource::Manual) == SetResult::Invalid);
        CHECK(c.setUtcMs(-5, ClockSource::Manual) == SetResult::Invalid);
        CHECK(c.generation() == g);
        c.setUtcMs(instant * 1000, ClockSource::Manual);
        CHECK(c.generation() == g + 1);
        // LOCAL JST cannot write a calendar year 2100.
        ClockService j;
        j.begin(&h, RtcMode::LocalJst, 0);
        CHECK(j.setUtcMs(kMaxUtc * 1000, ClockSource::Manual) == SetResult::Invalid);
    }
    // RTC mode change re-interprets the calendar, never rewrites it.
    {
        FakeHal h;
        h.rtc = {2026, 10, 23, 5, 18, 0};
        ClockService c;
        c.begin(&h, RtcMode::Utc, 0);
        CHECK(c.nowUtc() == instant + 9 * 3600);
        int64_t pv;
        CHECK(c.previewRtc(RtcMode::LocalJst, pv) && pv == instant);
        const int w = h.writes;
        c.applyRtcMode(RtcMode::LocalJst);
        CHECK(c.nowUtc() == instant && h.writes == w && c.rtcMode() == RtcMode::LocalJst);
        // Without RTC the mode change keeps a valid clock.
        FakeHal n;
        n.present = false;
        ClockService d;
        d.begin(&n, RtcMode::Utc, 0);
        d.setUtcMs(instant * 1000, ClockSource::Manual);
        CHECK(!d.applyRtcMode(RtcMode::LocalJst));
        CHECK(d.valid() && d.nowUtc() == instant && !d.previewRtc(RtcMode::Utc, pv));
    }
    // A cleared RTC (2000-01-01) never replaces a good clock on a mode change.
    {
        FakeHal h;
        ClockService c;
        c.begin(&h, RtcMode::Utc, 0);
        h.writeOk = false;  // the NTP time never reached the RTC
        c.setUtcMs(instant * 1000, ClockSource::Ntp);
        CHECK(!c.applyRtcMode(RtcMode::LocalJst));
        CHECK(c.valid() && c.nowUtc() == instant && c.source() == ClockSource::Ntp && !c.rtcValid());
        CHECK(c.lastEstablishedUtc() == instant && c.rtcMode() == RtcMode::LocalJst);
    }
    // A stale RTC (well before the last established instant) is also rejected.
    {
        FakeHal h;
        ClockService c;
        c.begin(&h, RtcMode::Utc, 0);
        c.setUtcMs(instant * 1000, ClockSource::Manual);
        h.rtc = {2025, 1, 1, 0, 0, 0};
        CHECK(!c.applyRtcMode(RtcMode::LocalJst) && c.nowUtc() == instant);
        // Within the slack (the 9 h reinterpretation itself) it is followed.
        h.rtc = {2026, 10, 22, 20, 18, 0};
        CHECK(c.applyRtcMode(RtcMode::Utc) && c.nowUtc() == instant && c.source() == ClockSource::Rtc);
    }
}

// ---------------------------------------------------------------- schedulers
static void testNtpScheduler() {
    NtpScheduler n;
    int64_t now = 1000;
    CHECK(n.state() == NtpState::Offline);
    CHECK(n.poll(now) == NtpScheduler::Tick::None);  // not connected (SoftAP clients do not count)
    n.setConnected(true, now);
    CHECK(n.poll(now) == NtpScheduler::Tick::Start && n.state() == NtpState::Syncing);
    CHECK(n.poll(now + 1) == NtpScheduler::Tick::None);  // no duplicate request
    // 10 s timeout => failure, retry 60 s, 300 s, 900 s.
    CHECK(n.poll(now + 9999) == NtpScheduler::Tick::None);
    CHECK(n.poll(now + 10000) == NtpScheduler::Tick::TimedOut && n.state() == NtpState::Failed);
    now += 10000;
    CHECK(n.poll(now + 59999) == NtpScheduler::Tick::None);
    CHECK(n.poll(now + 60000) == NtpScheduler::Tick::Start);
    now += 60000;
    n.onFailure(now);
    CHECK(n.poll(now + 299999) == NtpScheduler::Tick::None);
    CHECK(n.poll(now + 300000) == NtpScheduler::Tick::Start);
    now += 300000;
    n.onFailure(now);
    CHECK(n.poll(now + 899999) == NtpScheduler::Tick::None);
    CHECK(n.poll(now + 900000) == NtpScheduler::Tick::Start);
    now += 900000;
    n.onFailure(now);
    CHECK(n.poll(now + 900000) == NtpScheduler::Tick::Start);
    now += 900000;
    // Success => 6 h periodic.
    n.onSuccess(now);
    CHECK(n.state() == NtpState::Synced);
    CHECK(n.poll(now + NtpScheduler::kResyncMs - 1) == NtpScheduler::Tick::None);
    CHECK(n.poll(now + NtpScheduler::kResyncMs) == NtpScheduler::Tick::Start);
    now += NtpScheduler::kResyncMs;
    // Disconnect cancels in-flight; reconnect within 60 s waits the remainder.
    CHECK(n.setConnected(false, now + 5) && n.state() == NtpState::Offline);
    n.setConnected(true, now + 10);
    CHECK(n.poll(now + 10) == NtpScheduler::Tick::None);
    CHECK(n.poll(now + 60000) == NtpScheduler::Tick::Start);
    now += 60000;
    n.onSuccess(now);
    // Manual request inside 60 s: scheduled, not duplicated.
    CHECK(n.requestManual(now + 1000) == NtpScheduler::Manual::Scheduled);
    CHECK(n.poll(now + 1000) == NtpScheduler::Tick::None);
    CHECK(n.poll(now + 60000) == NtpScheduler::Tick::Start);
    now += 60000;
    // Clock set manually: in-flight is cancelled; next auto sync after 6 h.
    CHECK(n.clockOverridden(now + 1));
    CHECK(n.poll(now + 120000) == NtpScheduler::Tick::None);
    CHECK(n.poll(now + 1 + NtpScheduler::kResyncMs) == NtpScheduler::Tick::Start);
    now += 1 + NtpScheduler::kResyncMs;
    // Server change cancels and reschedules.
    CHECK(n.configChanged(now + 2));
    CHECK(n.poll(now + 60000) == NtpScheduler::Tick::Start);
    now += 60000;
    // Disabled: nothing, manual says DISABLED.
    n.setEnabled(false, now);
    CHECK(n.state() == NtpState::Disabled && n.requestManual(now) == NtpScheduler::Manual::Disabled);
    CHECK(n.poll(now + 10 * NtpScheduler::kResyncMs) == NtpScheduler::Tick::None);
    n.setEnabled(true, now);
    n.setConnected(false, now);
    CHECK(n.requestManual(now) == NtpScheduler::Manual::Offline);
}

static void testWifiScheduler() {
    WifiScheduler w;
    CHECK(!w.poll(false, 0));  // no credentials: never tries
    w.setCredentials(true, 100);
    CHECK(w.poll(false, 100) && w.attempting());
    CHECK(!w.attemptTimedOut(false, 100 + 14999));
    CHECK(w.attemptTimedOut(false, 100 + 15000));
    int64_t t = 100 + 15000;
    CHECK(!w.poll(false, t + 59999) && w.poll(false, t + 60000));
    t += 60000;
    w.onDisconnected(t + 10);  // attempt failed via event
    CHECK(!w.poll(false, t + 10 + 299999) && w.poll(false, t + 10 + 300000));
    w.onConnected();
    CHECK(!w.poll(true, t + 400000));
    w.onDisconnected(t + 500000);  // dropped after working
    CHECK(w.poll(false, t + 501000));
}

// ---------------------------------------------------------------- date editor
static void testDateEditor() {
    DateEditor d;
    d.load({2024, 2, 29, 23, 59, 59});
    CHECK(d.adjust(DateEditor::Year, +1) && d.draft().year == 2025 && d.draft().day == 28);
    d.load({2026, 1, 31, 0, 0, 0});
    d.adjust(DateEditor::Month, +1);
    CHECK(d.draft().month == 2 && d.draft().day == 28);
    d.adjust(DateEditor::Month, +1);
    CHECK(d.draft().day == 28);  // not re-expanded
    d.load({2099, 12, 31, 23, 59, 59});
    CHECK(!d.adjust(DateEditor::Year, +1) && !d.adjust(DateEditor::Month, +1));
    CHECK(!d.adjust(DateEditor::Day, +1) && !d.adjust(DateEditor::Hour, +1));
    CHECK(!d.adjust(DateEditor::Minute, +1) && !d.adjust(DateEditor::Second, +1));
    d.load({2000, 1, 1, 0, 0, 0});
    CHECK(!d.adjust(DateEditor::Year, -1) && !d.adjust(DateEditor::Second, -1));
    CHECK(d.valid());
    d.load({2026, 4, 30, 0, 0, 0});
    CHECK(!d.adjust(DateEditor::Day, +1) && d.draft().day == 30);
}

// ---------------------------------------------------------------- input
static void testInput() {
    InputRouter r;
    int64_t t = 0;
    auto step = [&](bool a, bool b, int64_t dt, bool touch = false, int16_t x = 0, int16_t y = 0) {
        t += dt;
        return r.update(a, b, touch, x, y, t).type;
    };
    // Short press fires on release.
    CHECK(step(true, false, 10) == InputType::None);
    CHECK(step(false, false, 100) == InputType::ShortA);
    // Long press fires once at 1.5 s, no short afterwards.
    step(false, true, 10);
    CHECK(step(false, true, 1499) == InputType::None);
    CHECK(step(false, true, 1) == InputType::LongB);
    CHECK(step(false, true, 500) == InputType::None);
    CHECK(step(false, false, 10) == InputType::None);
    // A+B: nothing fires (Launcher return), not even after long holds or on release.
    CHECK(step(true, false, 10) == InputType::None);
    CHECK(step(true, true, 30) == InputType::None);
    CHECK(step(true, true, 2000) == InputType::None);
    CHECK(step(false, true, 10) == InputType::None);
    CHECK(step(false, true, 2000) == InputType::None);  // B still held: no LongB
    CHECK(step(false, false, 10) == InputType::None);
    // Normal operation resumes afterwards.
    step(false, true, 10);
    CHECK(step(false, false, 10) == InputType::ShortB);
    // Touch: tap and long touch.
    CHECK(step(false, false, 10, true, 200, 200) == InputType::None);
    CHECK(step(false, false, 50) == InputType::Tap);
    step(false, false, 10, true, 233, 233);
    CHECK(step(false, false, 1500, true, 233, 233) == InputType::LongTouch);
    CHECK(step(false, false, 10) == InputType::None);
    // Drag is neither tap nor long touch.
    step(false, false, 10, true, 100, 100);
    step(false, false, 10, true, 100, 160);
    CHECK(step(false, false, 1600, true, 100, 160) == InputType::None);
    CHECK(step(false, false, 10) == InputType::None);
}

// ---------------------------------------------------------------- persistence
static void testCodec() {
    EventRepository r;
    std::string a, b;
    r.add(input("3DPの集い 2026", 1790780400LL, 1794704400LL), rnd, &a);
    r.add(input("HoriLab 出店", 1792681200LL, 1797123600LL), rnd, &b);
    r.select(b);
    bool changed;
    r.processArrivals(1794704399LL, 1794704400LL, true, changed);
    const std::string json = encodeEvents(r);
    std::vector<Event> out;
    std::string sel, err;
    CHECK(decodeEvents(json, out, sel, err) == LoadResult::Ok);
    CHECK(out.size() == 2 && sel == b && out[0].name == "3DPの集い 2026");
    CHECK(out[0].handled() && !out[1].handled() && out[1].targetUtc == 1797123600LL);

    // v2 with local times + global timezone migrates to the same UTC seconds.
    const char* v2 =
        R"({"schemaVersion":2,"selectedEventId":"evt-0001","timezone":"Asia/Tokyo","events":[)"
        R"({"id":"evt-0001","name":"3DPの集い 2026","startLocal":"2026-10-01T00:00:00","targetLocal":"2026-11-15T10:00:00","enabled":true},)"
        R"({"id":"evt-0002","name":"HoriLab 出店","startLocal":"2026-10-23T00:00:00","targetLocal":"2026-12-13T10:00:00","enabled":true}]})";
    CHECK(decodeEvents(v2, out, sel, err) == LoadResult::Migrated);
    CHECK(out.size() == 2 && out[0].startUtc == 1790780400LL && out[0].targetUtc == 1794704400LL);
    CHECK(out[1].startUtc == 1792681200LL && out[1].targetUtc == 1797123600LL && sel == "evt-0001");
    // v2 already in UTC is not shifted again.
    const char* v2utc =
        R"({"schemaVersion":2,"timezone":"Asia/Tokyo","events":[{"id":"e1","name":"X","startUtc":1790780400,"targetUtc":1794704400}]})";
    CHECK(decodeEvents(v2utc, out, sel, err) == LoadResult::Migrated && out[0].targetUtc == 1794704400LL);
    // v2 without timezone / with bad date: rejected, nothing converted.
    const char* v2noTz =
        R"({"schemaVersion":2,"events":[{"id":"e1","name":"X","startLocal":"2026-10-01T00:00:00","targetLocal":"2026-11-15T10:00:00"}]})";
    CHECK(decodeEvents(v2noTz, out, sel, err) == LoadResult::Corrupt && out.empty());
    const char* v2bad =
        R"({"schemaVersion":2,"timezone":"Asia/Tokyo","events":[{"id":"e1","name":"X","startLocal":"2026-02-30T00:00:00","targetLocal":"2026-11-15T10:00:00"}]})";
    CHECK(decodeEvents(v2bad, out, sel, err) == LoadResult::Corrupt);
    const char* v2tz =
        R"({"schemaVersion":2,"timezone":"America/New_York","events":[{"id":"e1","name":"X","startLocal":"2026-10-01T00:00:00","targetLocal":"2026-11-15T10:00:00"}]})";
    CHECK(decodeEvents(v2tz, out, sel, err) == LoadResult::Corrupt);
    // Start >= target in storage is corrupt.
    const char* v3bad =
        R"({"schemaVersion":3,"events":[{"id":"e1","name":"X","startUtc":1794704400,"targetUtc":1794704400,"timezone":"UTC","enabled":true}]})";
    CHECK(decodeEvents(v3bad, out, sel, err) == LoadResult::Corrupt);
    CHECK(decodeEvents("{nope", out, sel, err) == LoadResult::Corrupt);
    CHECK(decodeEvents("", out, sel, err) == LoadResult::Empty);

    DeviceSettings s;
    s.brightness = 80;
    s.rtcMode = RtcMode::LocalJst;
    s.ntpEnabled = false;
    std::strcpy(s.ntpServer, "ntp.nict.jp");
    DeviceSettings back;
    CHECK(decodeSettings(encodeSettings(s), back) && back == s);
    DeviceSettings def;
    CHECK(def.brightness == 65 && def.dimAfterSec == 30 && !def.sound && def.vibration && def.ntpEnabled);
    CHECK(validNtpServer("pool.ntp.org") && !validNtpServer("bad host") && !validNtpServer(""));
    CHECK(cycleStep(kBrightnessSteps, uint8_t(65), +1) == 80);
    CHECK(cycleStep(kBrightnessSteps, uint8_t(10), -1) == 100);
    // Values set from the web that are not presets move to the nearest preset.
    CHECK(cycleStep(kBrightnessSteps, uint8_t(42), +1) == 50);
    CHECK(cycleStep(kBrightnessSteps, uint8_t(42), -1) == 35);
    CHECK(cycleStep(kBrightnessSteps, uint8_t(5), -1) == 100);
    CHECK(cycleStep(kBrightnessSteps, uint8_t(5), +1) == 10);
    CHECK(cycleStep(kDimSteps, uint16_t(45), +1) == 60);
    CHECK(cycleStep(kDimSteps, uint16_t(45), -1) == 30);
    CHECK(cycleStep(kDimSteps, uint16_t(3000), +1) == 0);  // past 300 s comes "never"
    CHECK(cycleStep(kDimSteps, uint16_t(300), +1) == 0);
}

int main() {
    testTimeMath();
    testNtpEra();
    testCountdown();
    testUtf8();
    testRepository();
    testArrival();
    testClock();
    testNtpScheduler();
    testWifiScheduler();
    testDateEditor();
    testInput();
    testCodec();
    std::printf("%d passed, %d failed\n", g_pass, g_fail);
    return g_fail == 0 ? 0 : 1;
}
