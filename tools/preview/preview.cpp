// Host-side screenshot harness: renders the real screen classes with M5GFX
// (Linux build, sprites only) into PNG files for visual review.
// Fixture data is fictitious, like the design kit. Build: tools/preview/build.sh
#include <cstdio>
#include <string>
#include <vector>
#include <algorithm>
#include <sys/time.h>
#include "../../src/app/App.h"
#include "../../src/services/WebConfigServer.h"
#include "../../src/ui/CountdownScreen.h"
#include "../../src/ui/HudWidgets.h"
#include "../../src/ui/SettingsScreen.h"


namespace cd {

// ---- minimal stand-ins for the hardware-facing pieces used by the screens
int64_t monoMs() { return 100000; }
bool RtcHal::rtcPresent() { return false; }
bool RtcHal::readRtc(Civil&) { return false; }
bool RtcHal::writeRtc(const Civil&) { return false; }
void RtcHal::setSystemUtcUs(int64_t) {}
int64_t RtcHal::systemUtcUs() { return 0; }
const char* staStateText(StaState s) { return s == StaState::Connected ? "CONNECTED" : "NOT CONFIGURED"; }
WebConfigServer::~WebConfigServer() {}
StaState NetworkService::staState() const { return hasCreds_ ? StaState::Connected : StaState::NotConfigured; }
bool NetworkService::staConnected() const { return hasCreds_; }
std::string NetworkService::staIp() const { return "192.168.1.23"; }
std::string NetworkService::apIp() const { return "192.168.4.1"; }
bool NetworkService::passwordConfigured() const { return true; }

namespace {
struct FakeHal : IClockHal {
    Civil rtc{2026, 10, 22, 20, 18, 0};
    int64_t us = 0;
    bool rtcPresent() override { return true; }
    bool readRtc(Civil& o) override { o = rtc; return true; }
    bool writeRtc(const Civil& c) override { rtc = c; return true; }
    void setSystemUtcUs(int64_t v) override { us = v; }
    int64_t systemUtcUs() override { return us; }
} g_hal;
bool g_web = false;
}  // namespace

CountdownView App::countdown() {
    const Event* e = repo_.selected();
    if (!e) return computeCountdown(false, clock_.valid(), 0, 0, 1);
    return computeCountdown(true, clock_.valid(), clock_.valid() ? clock_.nowUtc() : 0, e->startUtc, e->targetUtc);
}
bool App::selectPrev() { return repo_.selectPrev(); }
bool App::selectNext() { return repo_.selectNext(); }
RepoError App::selectEvent(const std::string& id, bool, bool*) { return repo_.select(id); }
SetResult App::setClockLocal(const Civil&) { return SetResult::Ok; }
bool App::saveSettings(const DeviceSettings& s) { settings_ = s; return true; }
bool App::saveRtcMode(RtcMode, bool*) { return true; }
NtpScheduler::Manual App::requestNtpSync() { return NtpScheduler::Manual::Started; }
void App::previewBrightness(uint8_t) {}
void App::restoreBrightness() {}
bool App::startWebConfig() { return g_web = true; }
void App::stopWebConfig() { g_web = false; }
bool App::webConfigActive() const { return g_web; }
void App::noteSetupActivity() {}
int64_t App::apRemainingMs() const { return 4 * 60 * 1000 + 37 * 1000; }
void App::navigate(ScreenId) {}
void App::back() {}
void App::toast(const char* m, bool e) { toast_ = m; toastError_ = e; toastUntil_ = m ? 1 : 0; }
const char* App::toastText() const { return toastUntil_ ? toast_.c_str() : nullptr; }

}  // namespace cd

using namespace cd;

static App g;

static int64_t jst(int y, int mo, int d, int h = 0, int mi = 0, int s = 0) {
    int64_t t;
    localToUtc(Civil{y, mo, d, h, mi, s}, Tz::AsiaTokyo, t);
    return t;
}

static void setNow(int64_t utc) {
    g.clock_.begin(&g_hal, RtcMode::Utc, 0);
    g.clock_.setUtcMs(utc * 1000, ClockSource::Ntp);
    g.clock_.source_ = ClockSource::Rtc;
}

static void addEvent(const char* name, int64_t s, int64_t t, bool enabled = true) {
    EventInput in;
    in.name = name;
    in.startUtc = s;
    in.targetUtc = t;
    in.enabled = enabled;
    std::string id;
    static uint32_t r = 0x4f2a0000;
    g.repo_.add(in, [] { return ++r; }, &id);
}

static void shoot(Screen& s, const char* file, bool round, int w, int h) {
    M5Canvas c;
    c.setColorDepth(16);
    c.createSprite(w, h);
    const Layout l = s.layout();
    Painter p(c, l.scale, l.ox, l.oy);
    s.drawBackground(p);
    s.draw(p);
    if (const char* t = g.toastText()) {
        const float cx = l.designW / 2, cy = l.designH / 2;
        const float tw = std::min(l.designW - 60, p.textWidth(t, fontset::small, 1) + 60);
        hud::frame(p, {cx - tw / 2, cy - 22, tw, 44}, 12, 2, true);
        p.text(t, cx, cy, middle_center, fontset::small, 1, g.toastError() ? pal::amber : pal::text);
    }
    if (round) {  // what the round panel actually shows
        for (int y = 0; y < h; ++y)
            for (int x = 0; x < w; ++x) {
                const float dx = x + 0.5f - w / 2.0f, dy = y + 0.5f - h / 2.0f;
                if (dx * dx + dy * dy > (w / 2.0f) * (w / 2.0f)) c.drawPixel(x, y, 0x0000);
            }
    }
    // Minimal PNG: RGB8 in zlib "stored" blocks (no compression library needed).
    std::vector<uint8_t> raw;
    for (int y = 0; y < h; ++y) {
        raw.push_back(0);
        for (int x = 0; x < w; ++x) {
            const auto px = c.readPixelRGB(x, y);
            raw.push_back(px.R8());
            raw.push_back(px.G8());
            raw.push_back(px.B8());
        }
    }
    std::vector<uint8_t> z{0x78, 0x01};
    uint32_t a = 1, b = 0;
    for (uint8_t v : raw) { a = (a + v) % 65521; b = (b + a) % 65521; }
    for (size_t i = 0; i < raw.size(); i += 65535) {
        const size_t n = std::min<size_t>(65535, raw.size() - i);
        z.push_back(i + n == raw.size());
        z.push_back(n & 255); z.push_back(n >> 8); z.push_back(~n & 255); z.push_back((~n >> 8) & 255);
        z.insert(z.end(), raw.begin() + i, raw.begin() + i + n);
    }
    const uint32_t ad = (b << 16) | a;
    for (int k = 3; k >= 0; --k) z.push_back(uint8_t(ad >> (8 * k)));
    auto crc = [](const std::vector<uint8_t>& d) {
        uint32_t c2 = 0xFFFFFFFF;
        for (uint8_t v : d) { c2 ^= v; for (int k = 0; k < 8; ++k) c2 = (c2 >> 1) ^ (0xEDB88320u & (0u - (c2 & 1))); }
        return ~c2;
    };
    std::vector<uint8_t> png{0x89, 'P', 'N', 'G', '\r', '\n', 0x1A, '\n'};
    auto chunk = [&](const char* type, const std::vector<uint8_t>& data) {
        const uint32_t n = uint32_t(data.size());
        for (int k = 3; k >= 0; --k) png.push_back(uint8_t(n >> (8 * k)));
        std::vector<uint8_t> td(type, type + 4);
        td.insert(td.end(), data.begin(), data.end());
        png.insert(png.end(), td.begin(), td.end());
        const uint32_t cr = crc(td);
        for (int k = 3; k >= 0; --k) png.push_back(uint8_t(cr >> (8 * k)));
    };
    std::vector<uint8_t> ihdr;
    for (uint32_t v : {uint32_t(w), uint32_t(h)})
        for (int k = 3; k >= 0; --k) ihdr.push_back(uint8_t(v >> (8 * k)));
    ihdr.insert(ihdr.end(), {8, 2, 0, 0, 0});
    chunk("IHDR", ihdr);
    chunk("IDAT", z);
    chunk("IEND", {});
    const size_t len = png.size();
    if (FILE* f = std::fopen(file, "wb")) {
        std::fwrite(png.data(), 1, len, f);
        std::fclose(f);
    }
    std::printf("%s (%zu bytes)\n", file, len);
}

// The partial frames of App::render() against full frames: from `start`, each
// second is drawn as App draws it (only liveRect(), into a canvas of that
// area, while the keys stay the same) and compared pixel by pixel with a full
// redraw of that second.
static bool partialMatchesFull(Screen& s, int w, int h, int64_t start, int seconds, const char* label) {
    const Layout l = s.layout();
    M5Canvas bg, frame, ref, live;
    for (M5Canvas* c : {&bg, &frame, &ref, &live}) c->setColorDepth(16);
    for (M5Canvas* c : {&bg, &frame, &ref}) c->createSprite(w, h);
    auto full = [&](M5Canvas& dst) {
        Painter bp(bg, l.scale, l.ox, l.oy);
        s.drawBackground(bp);
        bg.pushSprite(&dst, 0, 0);
        Painter p(dst, l.scale, l.ox, l.oy);
        s.draw(p);
    };
    setNow(start);
    full(frame);
    uint32_t bgKey = s.backgroundKey(), content = s.contentKey();
    int partial = 0, mismatched = 0;
    for (int i = 1; i <= seconds; ++i) {
        setNow(start + i);
        Rect r;
        if (s.backgroundKey() == bgKey && s.contentKey() == content && s.ticks() && s.liveRect(r)) {
            const Painter m(frame, l.scale, l.ox, l.oy);
            const int cx = m.X(r.x) - 1, cy = m.Y(r.y) - 1;
            live.createSprite(m.X(r.x + r.w) - cx + 2, m.Y(r.y + r.h) - cy + 2);
            bg.pushSprite(&live, -cx, -cy);
            Painter p(live, l.scale, l.ox - cx, l.oy - cy);
            s.draw(p);
            live.pushSprite(&frame, cx, cy);
            ++partial;
        } else {
            full(frame);
            bgKey = s.backgroundKey();
            content = s.contentKey();
        }
        full(ref);
        const auto* a = static_cast<const uint16_t*>(frame.getBuffer());
        const auto* b = static_cast<const uint16_t*>(ref.getBuffer());
        int diff = 0, x0 = w, y0 = h, x1 = -1, y1 = -1;
        for (int k = 0; k < w * h; ++k) {
            if (a[k] == b[k]) continue;
            ++diff;
            x0 = std::min(x0, k % w), x1 = std::max(x1, k % w);
            y0 = std::min(y0, k / w), y1 = std::max(y1, k / w);
        }
        if (diff) {
            ++mismatched;
            std::printf("  %s +%ds: %d pixels differ in x %d..%d y %d..%d\n", label, i, diff, x0, x1, y0, y1);
        }
    }
    std::printf("partial check %s: %d frames, %d partial, %d mismatched\n", label, seconds, partial, mismatched);
    return mismatched == 0;
}

int main(int argc, char** argv) {
    const std::string out = argc > 1 ? argv[1] : ".";
    auto path = [&](const char* n) { return (out + "/" + n).c_str(); };
    std::string f;

    g.caps_.shape = ScreenShape::Round;
    g.caps_.width = g.caps_.height = 466;
    g.caps_.touch = true;
    g.caps_.buttons = 2;
    g.caps_.rtc = g.caps_.speaker = g.caps_.vibration = true;
    g.caps_.boardName = "M5StopWatch";
    g.net_.apSsid_ = "EventCountDown-4F2A";
    g.net_.apPass_ = "K7QD-M2XT-9PRH";

    CountdownScreen cs(g);
    const int64_t s1 = jst(2026, 10, 1), t1 = jst(2026, 11, 15, 10);
    addEvent("3DPの集い 2026", s1, t1);
    addEvent("HoriLab 出店", jst(2026, 10, 23), jst(2026, 12, 13, 10));
    addEvent("SAMPLE EVENT", jst(2026, 10, 23), jst(2027, 1, 1));

    setNow(t1 - (8 * 86400 + 12 * 3600 + 34 * 60 + 56));
    shoot(cs, (f = out + "/01_countdown_main.png").c_str(), true, 466, 466);
    g.repo_.selectNext();
    shoot(cs, (f = out + "/02_countdown_event_b.png").c_str(), true, 466, 466);
    g.repo_.selectPrev();
    setNow(t1 - (42 * 60 + 10));
    shoot(cs, (f = out + "/03_countdown_final_hour.png").c_str(), true, 466, 466);
    setNow(t1 + 120);
    shoot(cs, (f = out + "/04_event_started.png").c_str(), true, 466, 466);

    // Long Japanese name + emoji (outside the font): ellipsis and placeholder.
    App& a = g;
    (void)a;
    EventRepository saved = g.repo_;
    g.repo_ = EventRepository();
    addEvent("とても長いイベント名のテスト：第十二回 東京メイカーズ・フェスティバル🚀", s1, t1);
    setNow(t1 - 3 * 86400);
    shoot(cs, (f = out + "/01b_long_name.png").c_str(), true, 466, 466);
    g.repo_ = EventRepository();
    addEvent("Far future", jst(2026, 1, 1), jst(2030, 6, 1));
    setNow(jst(2026, 10, 23));
    shoot(cs, (f = out + "/01c_four_digit_days.png").c_str(), true, 466, 466);

    g.repo_ = EventRepository();
    shoot(cs, (f = out + "/05_no_events.png").c_str(), true, 466, 466);
    g.repo_ = saved;
    g.clock_ = ClockService();
    g.clock_.begin(nullptr, RtcMode::Utc, 0);
    shoot(cs, (f = out + "/06_time_not_set.png").c_str(), true, 466, 466);

    setNow(jst(2026, 10, 23, 5, 18, 0));
    g.lastNtpSuccessUtc_ = jst(2026, 10, 23, 4, 0, 0);
    MenuPage menu(g);
    menu.enter(ScreenId::Countdown);
    shoot(menu, (f = out + "/07_device_settings_menu.png").c_str(), true, 466, 466);
    SetTimePage st(g);
    st.enter(ScreenId::Menu);
    shoot(st, (f = out + "/08_device_set_time.png").c_str(), true, 466, 466);
    WebConfigPage web(g);
    web.enter(ScreenId::Menu);
    shoot(web, (f = out + "/09_device_web_config.png").c_str(), true, 466, 466);
    TimeStatusPage ts(g);
    shoot(ts, (f = out + "/10_device_time_status.png").c_str(), true, 466, 466);
    EventListPage el(g);
    el.enter(ScreenId::Menu);
    shoot(el, (f = out + "/11_device_event_list.png").c_str(), true, 466, 466);
    OptionsPage op(g);
    op.enter(ScreenId::Menu);
    shoot(op, (f = out + "/13_device_options.png").c_str(), true, 466, 466);
    RtcModePage rm(g);
    rm.enter(ScreenId::Options);
    shoot(rm, (f = out + "/14_device_rtc_mode.png").c_str(), true, 466, 466);
    g.toast("TIME SAVED");
    shoot(menu, (f = out + "/15_toast.png").c_str(), true, 466, 466);
    g.toastUntil_ = 0;

    // Rectangular boards.
    g.caps_.shape = ScreenShape::Rect;
    g.caps_.width = 320;
    g.caps_.height = 240;
    setNow(t1 - (8 * 86400 + 12 * 3600 + 34 * 60 + 56));
    shoot(cs, (f = out + "/12_rectangular_layout.png").c_str(), false, 320, 240);
    g.caps_.width = 240;
    g.caps_.height = 135;
    g.caps_.compact = true;
    shoot(cs, (f = out + "/12b_compact_240x135.png").c_str(), false, 240, 135);

    // Partial frames: across minute, hour and day boundaries, into the final
    // hour and past the arrival, on the round and the rectangular layout.
    bool ok = true;
    g.caps_.compact = false;
    ok &= partialMatchesFull(cs, 320, 240, t1 - (2 * 86400 + 65), 130, "rect 320x240 (day boundary)");
    g.caps_.shape = ScreenShape::Round;
    g.caps_.width = g.caps_.height = 466;
    ok &= partialMatchesFull(cs, 466, 466, t1 - (8 * 86400 + 3605), 70, "round (hour boundary)");
    ok &= partialMatchesFull(cs, 466, 466, t1 - (3 * 86400 + 70), 140, "round (day boundary)");
    ok &= partialMatchesFull(cs, 466, 466, t1 - 3605, 10, "round (final hour)");
    ok &= partialMatchesFull(cs, 466, 466, t1 - 5, 10, "round (arrival)");
    (void)path;
    return ok ? 0 : 1;
}
