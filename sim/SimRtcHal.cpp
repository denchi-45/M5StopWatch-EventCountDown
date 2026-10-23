// Simulator: RTC and clocks on the PC.
// The "RTC" is the PC's UTC clock plus an offset kept in sim_data/rtc_offset,
// so it starts out holding UTC like an RTC set by the Launcher, survives
// restarts, and can be rewritten by the app (SET TIME, browser time, ...).
#include <chrono>
#include <fstream>
#include "../src/hal/RtcHal.h"

namespace cd {

namespace {
const char* kOffsetFile = "sim_data/rtc_offset";
int64_t g_baseUtcUs = 0;
int64_t g_baseMonoUs = 0;

int64_t monoUs() {
    using namespace std::chrono;
    return duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}
int64_t pcUtc() {
    using namespace std::chrono;
    return duration_cast<seconds>(system_clock::now().time_since_epoch()).count();
}
int64_t rtcOffset() {
    std::ifstream f(kOffsetFile);
    int64_t v = 0;
    return (f >> v) ? v : 0;
}
}  // namespace

int64_t monoMs() { return monoUs() / 1000; }

bool RtcHal::rtcPresent() { return true; }

bool RtcHal::readRtc(Civil& out) {
    out = epochToCivil(pcUtc() + rtcOffset());
    return true;
}

bool RtcHal::writeRtc(const Civil& c) {
    std::ofstream f(kOffsetFile, std::ios::trunc);
    f << (civilToEpoch(c) - pcUtc());
    return bool(f);
}

void RtcHal::setSystemUtcUs(int64_t utcUs) {
    g_baseMonoUs = monoUs();
    g_baseUtcUs = utcUs;
}

int64_t RtcHal::systemUtcUs() { return g_baseUtcUs + (monoUs() - g_baseMonoUs); }

}  // namespace cd
