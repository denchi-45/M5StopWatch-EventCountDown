#pragma once
// Remaining time, progress and display state for one event (spec 5, 5.3).
#include <cstdint>

namespace cd {

constexpr int kRingSegments = 52;
constexpr int64_t kFinalCountdownSec = 3600;

enum class CountdownState : uint8_t {
    NoEvents,        // no enabled event
    TimeNotSet,      // enabled events exist, clock invalid
    EventStarted,    // now >= target
    FinalCountdown,  // 0 < remaining < 3600
    Active,
};

struct CountdownView {
    CountdownState state = CountdownState::NoEvents;
    int64_t remaining = 0;  // seconds, >= 0
    int64_t days = 0;
    int hours = 0, minutes = 0, seconds = 0;
    double progress = 0.0;  // 0..1, elapsed share of [start, target]
    int litSegments = 0;    // round(progress * segments)
};

const char* stateLabel(CountdownState s);

// Requires targetUtc > startUtc (validated on registration). The clock value
// is only read when clockValid; no seconds are derived from an invalid clock.
CountdownView computeCountdown(bool hasEnabledEvent, bool clockValid, int64_t nowUtc,
                               int64_t startUtc, int64_t targetUtc,
                               int segments = kRingSegments);

}  // namespace cd
