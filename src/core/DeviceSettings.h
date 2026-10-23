#pragma once
// User-facing device options (spec 2.13, 3.3) with their defaults.
#include <cstdint>
#include <cstring>
#include "ClockService.h"
#include "TimeMath.h"

namespace cd {

constexpr uint8_t kBrightnessSteps[] = {10, 20, 35, 50, 65, 80, 100};
constexpr uint16_t kDimSteps[] = {10, 30, 60, 120, 300, 0};  // 0 = never

struct DeviceSettings {
    uint8_t brightness = 65;    // percent
    uint16_t dimAfterSec = 30;  // 0 = never dim
    bool sound = false;
    bool vibration = true;
    bool keepAwakeOnUsb = false;
    Tz displayTz = Tz::AsiaTokyo;
    RtcMode rtcMode = RtcMode::Utc;
    bool ntpEnabled = true;
    char ntpServer[64] = "pool.ntp.org";

    bool operator==(const DeviceSettings& o) const {
        return brightness == o.brightness && dimAfterSec == o.dimAfterSec && sound == o.sound &&
               vibration == o.vibration && keepAwakeOnUsb == o.keepAwakeOnUsb &&
               displayTz == o.displayTz && rtcMode == o.rtcMode && ntpEnabled == o.ntpEnabled &&
               std::strcmp(ntpServer, o.ntpServer) == 0;
    }
    bool operator!=(const DeviceSettings& o) const { return !(*this == o); }
};

// Hostname: letters, digits, '-' and '.', 1..63 chars, no leading/trailing dot or dash.
inline bool validNtpServer(const char* s) {
    const size_t n = s ? std::strlen(s) : 0;
    if (n == 0 || n > 63) return false;
    for (size_t i = 0; i < n; ++i) {
        const char c = s[i];
        const bool ok = (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') ||
                        c == '-' || c == '.';
        if (!ok) return false;
    }
    return s[0] != '.' && s[0] != '-' && s[n - 1] != '.' && s[n - 1] != '-';
}

template <typename T, size_t N>
T cycleStep(const T (&steps)[N], T current, int dir) {
    size_t i = 0;
    while (i < N && steps[i] != current) ++i;
    if (i == N) return steps[0];
    return steps[(i + N + (dir >= 0 ? 1 : N - 1)) % N];
}

}  // namespace cd
