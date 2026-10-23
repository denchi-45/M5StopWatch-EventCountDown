#include "ClockService.h"
#include <cstring>

namespace cd {

namespace {
constexpr int32_t kJstOffset = 9 * 3600;
// Tolerance for an RTC that runs slightly behind the last established instant.
constexpr int64_t kLastKnownGoodSlack = 24 * 3600;
}

const char* clockSourceText(ClockSource s) {
    switch (s) {
        case ClockSource::Invalid: return "INVALID";
        case ClockSource::Rtc: return "RTC";
        case ClockSource::Ntp: return "NTP";
        case ClockSource::Manual: return "MANUAL";
        case ClockSource::Browser: return "BROWSER";
    }
    return "INVALID";
}
const char* rtcModeText(RtcMode m) { return m == RtcMode::Utc ? "UTC" : "LOCAL JST"; }
const char* rtcModeId(RtcMode m) { return m == RtcMode::Utc ? "UTC" : "LOCAL_JST"; }
bool parseRtcMode(const char* s, RtcMode& out) {
    if (!s) return false;
    if (std::strcmp(s, "UTC") == 0) { out = RtcMode::Utc; return true; }
    if (std::strcmp(s, "LOCAL_JST") == 0) { out = RtcMode::LocalJst; return true; }
    return false;
}
const char* rtcWriteStatusText(RtcWriteStatus s) {
    switch (s) {
        case RtcWriteStatus::None: return "NONE";
        case RtcWriteStatus::Ok: return "OK";
        case RtcWriteStatus::Failed: return "FAILED";
        case RtcWriteStatus::Unavailable: return "UNAVAILABLE";
    }
    return "NONE";
}

int64_t rtcCalendarToUtc(const Civil& c, RtcMode m) {
    return civilToEpoch(c) - (m == RtcMode::LocalJst ? kJstOffset : 0);
}
bool utcToRtcCalendar(int64_t utc, RtcMode m, Civil& out) {
    out = epochToCivil(utc + (m == RtcMode::LocalJst ? kJstOffset : 0));
    return validCivil(out);  // calendar year must stay within 2000..2099
}

void ClockService::begin(IClockHal* hal, RtcMode mode, int64_t lastKnownGoodUtc) {
    hal_ = hal;
    mode_ = mode;
    lastKnownGood_ = lastKnownGoodUtc;
    rtcPresent_ = hal_ && hal_->rtcPresent();
    int64_t utc;
    if (readTrustedRtc(mode_, utc)) {
        hal_->setSystemUtcUs(utc * 1000000LL);
        valid_ = true;
        source_ = ClockSource::Rtc;
        lastEstablished_ = utc;
        ++generation_;
    }
}

bool ClockService::readTrustedRtc(RtcMode mode, int64_t& utc) {
    rtcValid_ = false;
    if (!hal_ || !rtcPresent_) return false;
    Civil c;
    if (!hal_->readRtc(c)) return false;
    if (!validCivil(c) || c.year < kMinTrustedYear) return false;
    utc = rtcCalendarToUtc(c, mode);
    if (!inUtcRange(utc)) return false;
    if (lastKnownGood_ > 0 && utc + kLastKnownGoodSlack < lastKnownGood_) return false;
    rtcValid_ = true;
    return true;
}

int64_t ClockService::nowUtc() const {
    const int64_t us = hal_ ? hal_->systemUtcUs() : 0;
    return us >= 0 ? us / 1000000LL : -((-us + 999999) / 1000000LL);
}
int64_t ClockService::nowUtcMs() const { return hal_ ? hal_->systemUtcUs() / 1000LL : 0; }

SetResult ClockService::setUtcMs(int64_t utcMs, ClockSource src) {
    if (!hal_ || utcMs < 0) return SetResult::Invalid;
    const int64_t sec = utcMs / 1000;
    if (!inUtcRange(sec) || src == ClockSource::Invalid || src == ClockSource::Rtc)
        return SetResult::Invalid;
    Civil cal;
    if (!utcToRtcCalendar(sec, mode_, cal)) return SetResult::Invalid;

    hal_->setSystemUtcUs(utcMs * 1000LL);
    valid_ = true;
    source_ = src;
    lastEstablished_ = sec;
    lastKnownGood_ = sec;
    ++generation_;

    if (!rtcPresent_) {
        rtcWrite_ = RtcWriteStatus::Unavailable;
        return SetResult::RtcWriteFailed;
    }
    // Write the whole second the system clock is in, then verify by reading back.
    Civil back;
    const bool ok = hal_->writeRtc(cal) && hal_->readRtc(back) &&
                    civilToEpoch(back) - civilToEpoch(cal) >= 0 &&
                    civilToEpoch(back) - civilToEpoch(cal) <= 2;
    rtcWrite_ = ok ? RtcWriteStatus::Ok : RtcWriteStatus::Failed;
    rtcValid_ = ok;
    return ok ? SetResult::Ok : SetResult::RtcWriteFailed;
}

bool ClockService::previewRtc(RtcMode mode, int64_t& utcOut) {
    if (!hal_ || !rtcPresent_) return false;
    Civil c;
    if (!hal_->readRtc(c) || !validCivil(c)) return false;
    utcOut = rtcCalendarToUtc(c, mode);
    return inUtcRange(utcOut);
}

void ClockService::applyRtcMode(RtcMode mode) {
    if (mode == mode_) return;
    mode_ = mode;
    int64_t utc;
    // Reinterpret the stored calendar. Without a readable RTC the running
    // clock stays as it is (an RTC mode change never invalidates the clock).
    if (previewRtc(mode, utc)) {
        hal_->setSystemUtcUs(utc * 1000000LL);
        valid_ = true;
        source_ = ClockSource::Rtc;
        rtcValid_ = true;
        lastEstablished_ = utc;
    }
    ++generation_;
}

}  // namespace cd
