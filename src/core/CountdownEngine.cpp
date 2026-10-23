#include "CountdownEngine.h"
#include <cmath>

namespace cd {

const char* stateLabel(CountdownState s) {
    switch (s) {
        case CountdownState::NoEvents: return "NO EVENTS";
        case CountdownState::TimeNotSet: return "TIME NOT SET";
        case CountdownState::EventStarted: return "EVENT STARTED";
        case CountdownState::FinalCountdown: return "FINAL COUNTDOWN";
        case CountdownState::Active: return "COUNTDOWN ACTIVE";
    }
    return "";
}

CountdownView computeCountdown(bool hasEnabledEvent, bool clockValid, int64_t nowUtc,
                               int64_t startUtc, int64_t targetUtc, int segments) {
    CountdownView v;
    if (!hasEnabledEvent) { v.state = CountdownState::NoEvents; return v; }
    if (!clockValid) { v.state = CountdownState::TimeNotSet; return v; }

    const int64_t diff = targetUtc - nowUtc;
    v.remaining = diff > 0 ? diff : 0;
    v.days = v.remaining / 86400;
    v.hours = int(v.remaining % 86400 / 3600);
    v.minutes = int(v.remaining % 3600 / 60);
    v.seconds = int(v.remaining % 60);

    const int64_t span = targetUtc - startUtc;
    double p = span > 0 ? double(nowUtc - startUtc) / double(span) : 1.0;
    if (p < 0.0) p = 0.0;
    if (p > 1.0) p = 1.0;
    v.progress = p;
    v.litSegments = int(std::lround(p * segments));

    // Reaching the target is decided by time, not by the ring (round() may
    // light every segment slightly before the target).
    if (nowUtc >= targetUtc) v.state = CountdownState::EventStarted;
    else if (v.remaining < kFinalCountdownSec) v.state = CountdownState::FinalCountdown;
    else v.state = CountdownState::Active;
    return v;
}

}  // namespace cd
