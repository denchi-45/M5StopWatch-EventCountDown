#include "RtcHal.h"
#include <M5Unified.h>
#include <esp_timer.h>
#include <sys/time.h>

namespace cd {

int64_t monoMs() { return esp_timer_get_time() / 1000; }

bool RtcHal::rtcPresent() { return M5.Rtc.isEnabled(); }

bool RtcHal::readRtc(Civil& out) {
    if (!M5.Rtc.isEnabled()) return false;
    m5::rtc_datetime_t dt;
    if (!M5.Rtc.getDateTime(&dt)) return false;
    out = {dt.date.year, dt.date.month, dt.date.date, dt.time.hours, dt.time.minutes,
           dt.time.seconds};
    return true;
}

bool RtcHal::writeRtc(const Civil& c) {
    if (!M5.Rtc.isEnabled()) return false;
    const int8_t wd = int8_t(weekdayFromDays(daysFromCivil(c.year, c.month, c.day)));
    // setDateTime() reports nothing; ClockService verifies by reading back.
    M5.Rtc.setDateTime(m5::rtc_datetime_t{
        m5::rtc_date_t(int16_t(c.year), int8_t(c.month), int8_t(c.day), wd),
        m5::rtc_time_t(int8_t(c.hour), int8_t(c.minute), int8_t(c.second))});
    return true;
}

void RtcHal::setSystemUtcUs(int64_t utcUs) {
    timeval tv;
    tv.tv_sec = time_t(utcUs / 1000000);
    tv.tv_usec = suseconds_t(utcUs % 1000000);
    settimeofday(&tv, nullptr);
}

int64_t RtcHal::systemUtcUs() {
    timeval tv;
    gettimeofday(&tv, nullptr);
    return int64_t(tv.tv_sec) * 1000000LL + tv.tv_usec;
}

}  // namespace cd
