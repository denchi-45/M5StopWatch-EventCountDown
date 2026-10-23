#pragma once
// The single owner of "what time is it" (spec 6, 6.1, 6.2).
// System clock and everything stored are UTC. Only the RTC edge converts, using
// the explicitly selected RTC interpretation (UTC or LOCAL JST).
#include <cstdint>
#include "TimeMath.h"

namespace cd {

enum class ClockSource : uint8_t { Invalid, Rtc, Ntp, Manual, Browser };
enum class RtcMode : uint8_t { Utc = 0, LocalJst = 1 };
enum class RtcWriteStatus : uint8_t { None, Ok, Failed, Unavailable };
enum class SetResult : uint8_t { Ok, RtcWriteFailed, Invalid };

const char* clockSourceText(ClockSource s);
const char* rtcModeText(RtcMode m);       // "UTC" / "LOCAL JST"
const char* rtcModeId(RtcMode m);         // "UTC" / "LOCAL_JST"
bool parseRtcMode(const char* s, RtcMode& out);
const char* rtcWriteStatusText(RtcWriteStatus s);

class IClockHal {
public:
    virtual ~IClockHal() = default;
    virtual bool rtcPresent() = 0;
    virtual bool readRtc(Civil& out) = 0;
    virtual bool writeRtc(const Civil& c) = 0;
    virtual void setSystemUtcUs(int64_t utcUs) = 0;
    virtual int64_t systemUtcUs() = 0;
};

// Calendar <-> UTC for one RTC mode.
int64_t rtcCalendarToUtc(const Civil& c, RtcMode m);
bool utcToRtcCalendar(int64_t utc, RtcMode m, Civil& out);

class ClockService {
public:
    // Oldest RTC value accepted at boot (a cleared RX8130 reads 2000-01-01).
    static constexpr int kMinTrustedYear = 2024;

    // lastKnownGoodUtc: last instant this app established (0 = none). An RTC
    // reading earlier than that is treated as reset/garbage.
    void begin(IClockHal* hal, RtcMode mode, int64_t lastKnownGoodUtc);

    bool valid() const { return valid_; }
    int64_t nowUtc() const;          // seconds; meaningful only when valid()
    int64_t nowUtcMs() const;
    ClockSource source() const { return source_; }
    RtcMode rtcMode() const { return mode_; }
    bool rtcAvailable() const { return rtcPresent_; }
    bool rtcValid() const { return rtcValid_; }
    RtcWriteStatus rtcWriteStatus() const { return rtcWrite_; }
    // Incremented on every clock establishment/correction. Anything derived from
    // an older generation (NTP requests, RTC previews, arrival tracking) is stale.
    uint32_t generation() const { return generation_; }
    int64_t lastEstablishedUtc() const { return lastEstablished_; }

    // NTP / MANUAL / BROWSER. Validates, sets the system clock, writes the RTC in
    // the current mode and reads it back.
    SetResult setUtcMs(int64_t utcMs, ClockSource src);

    // Interpretation of the current RTC calendar value under `mode`.
    // False when no readable/valid RTC exists (preview = UNAVAILABLE).
    bool previewRtc(RtcMode mode, int64_t& utcOut);
    // Applies a new interpretation (spec 2.14): the RTC calendar is not rewritten;
    // when it is readable, the system clock follows the new interpretation.
    // Call only after the mode was persisted.
    void applyRtcMode(RtcMode mode);

private:
    IClockHal* hal_ = nullptr;
    RtcMode mode_ = RtcMode::Utc;
    bool valid_ = false;
    bool rtcPresent_ = false;
    bool rtcValid_ = false;
    RtcWriteStatus rtcWrite_ = RtcWriteStatus::None;
    ClockSource source_ = ClockSource::Invalid;
    uint32_t generation_ = 0;
    int64_t lastEstablished_ = 0;
    int64_t lastKnownGood_ = 0;

    bool readTrustedRtc(RtcMode mode, int64_t& utc);
};

}  // namespace cd
