#include "RtcHal.h"
#include <M5Unified.h>
#include <esp_timer.h>
#include <sys/time.h>

namespace cd {

namespace {
// time_t is 32-bit on Arduino-ESP32 2.x (IDF 4.4), so the libc clock cannot
// hold instants after 2038-01-19 while the app accepts dates up to 2099.
// The wall clock is therefore kept here as UTC at a monotonic base.
int64_t g_baseUtcUs = 0;
int64_t g_baseMonoUs = 0;
}  // namespace

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
    g_baseMonoUs = esp_timer_get_time();
    g_baseUtcUs = utcUs;
    // Mirror into the libc clock only while time_t can represent it.
    const int64_t sec = utcUs / 1000000;
    if (sec == int64_t(time_t(sec))) {
        timeval tv;
        tv.tv_sec = time_t(sec);
        tv.tv_usec = suseconds_t(utcUs % 1000000);
        settimeofday(&tv, nullptr);
    }
}

int64_t RtcHal::systemUtcUs() { return g_baseUtcUs + (esp_timer_get_time() - g_baseMonoUs); }

}  // namespace cd
