// Simulator entry point (pio run -e native -t exec).
// Window = M5StopWatch 466x466 round panel. Mouse = touch.
// KEY A = Left arrow or A, KEY B = Down arrow or B (hold both for A+B).
// program.exe --demo replaces the stored events with sample events relative to
// now (the simulator has no web setup to create them), including one that
// arrives 90 s after start to check the final countdown and the arrival.
#include <M5Unified.h>
#include <chrono>
#include <cstring>
#include <esp_random.h>
#include "../src/app/App.h"
#include "../src/services/JsonCodec.h"

namespace {
// Panel_sdl keeps its window state protected; the simulator only needs to
// close the 320x240 default window M5GFX opened for the StopWatch board.
struct PanelAccess : lgfx::Panel_sdl {
    static void close(lgfx::Panel_sdl* p) { static_cast<PanelAccess*>(p)->monitor.closing = true; }
};

constexpr int kSize = 466;
bool g_demo = false;

void writeDemoEvents() {
    using namespace std::chrono;
    const int64_t now = duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
    struct Demo {
        const char* name;
        int64_t start, target;
        bool enabled;
    };
    const Demo demos[] = {
        {"3DPの集い 2026", now - 30 * 86400, now + 12 * 86400 + 5 * 3600, true},
        {"HoriLab 出店", now - 10 * 86400, now + 51 * 86400, true},
        {"FINAL TEST (90 s)", now - 3600, now + 90, true},
        {"SAMPLE EVENT (OFF)", now - 86400, now + 69 * 86400, false},
    };
    cd::EventRepository repo;
    for (const Demo& d : demos) {
        cd::EventInput in;
        in.name = d.name;
        in.startUtc = d.start;
        in.targetUtc = d.target;
        in.enabled = d.enabled;
        std::string id;
        repo.add(in, [] { return esp_random(); }, &id, nullptr);
    }
    cd::SettingsStore store;
    store.begin();
    const bool ok = store.saveEvents(cd::encodeEvents(repo));
    std::printf("[sim] demo events %s\n", ok ? "written" : "NOT written");
}

void useStopWatchPanel() {
    auto* old = static_cast<lgfx::Panel_sdl*>(M5.Display.panel());
    auto* p = new lgfx::Panel_sdl();
    auto cfg = p->config();
    cfg.memory_width = cfg.panel_width = kSize;
    cfg.memory_height = cfg.panel_height = kSize;
    cfg.bus_shared = false;
    p->config(cfg);
    p->setWindowTitle("EventCountDown (simulator)");
    p->setScaling(1, 1);
    auto* t = new lgfx::Touch_sdl();
    auto tc = t->config();
    tc.x_min = 0;
    tc.x_max = kSize - 1;
    tc.y_min = 0;
    tc.y_max = kSize - 1;
    tc.bus_shared = false;
    t->config(tc);
    p->touch(t);
    M5.Display.init(p);  // the new window exists before the old one closes
    if (old) PanelAccess::close(old);
}

int run(bool* running) {
    lgfx::Panel_sdl::addKeyCodeMapping(SDLK_a, 39);  // BtnA
    lgfx::Panel_sdl::addKeyCodeMapping(SDLK_b, 38);  // BtnB
    auto cfg = M5.config();
    cfg.internal_imu = false;
    cfg.internal_mic = false;
    if (g_demo) writeDemoEvents();
    M5.begin(cfg);
    useStopWatchPanel();
    M5.Touch.begin(&M5.Display);
    cd::app().setup(false, false);  // SDL panel: canvas double buffer; input is polled
    while (*running) {
        M5.update();
        cd::app().loop();
        M5.delay(4);
    }
    return 0;
}
}  // namespace

int main(int argc, char** argv) {
    for (int i = 1; i < argc; ++i)
        if (std::strcmp(argv[i], "--demo") == 0) g_demo = true;
    return lgfx::Panel_sdl::main(run);
}
