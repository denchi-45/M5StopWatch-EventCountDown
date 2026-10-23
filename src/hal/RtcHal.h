#pragma once
// IClockHal on M5Unified's RTC and the ESP32 system clock.
#include "../core/ClockService.h"

namespace cd {

class RtcHal : public IClockHal {
public:
    bool rtcPresent() override;
    bool readRtc(Civil& out) override;
    bool writeRtc(const Civil& c) override;
    void setSystemUtcUs(int64_t utcUs) override;
    int64_t systemUtcUs() override;
};

// Monotonic milliseconds since boot (never affected by clock corrections).
int64_t monoMs();

}  // namespace cd
