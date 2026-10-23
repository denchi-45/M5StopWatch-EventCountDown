#pragma once
// Civil-time arithmetic without TZ/tzset/localtime. Everything inside the app is
// UTC epoch seconds (signed 64-bit); a civil value only exists at the edges
// (display, RTC calendar, web input).
#include <cstdint>
#include <cstdio>
#include <cstring>

namespace cd {

// Accepted UTC window (spec 5.1): 2000-01-01T00:00:00Z .. 2099-12-31T23:59:59Z.
constexpr int64_t kMinUtc = 946684800LL;
constexpr int64_t kMaxUtc = 4102444799LL;
constexpr int kMinYear = 2000, kMaxYear = 2099;

struct Civil {
    int year = 2000, month = 1, day = 1, hour = 0, minute = 0, second = 0;
    bool operator==(const Civil& o) const {
        return year == o.year && month == o.month && day == o.day &&
               hour == o.hour && minute == o.minute && second == o.second;
    }
};

// Time zones supported by v1 (no DST anywhere).
enum class Tz : uint8_t { AsiaTokyo = 0, Utc = 1 };

inline int32_t tzOffsetSec(Tz tz) { return tz == Tz::AsiaTokyo ? 9 * 3600 : 0; }
inline const char* tzId(Tz tz) { return tz == Tz::AsiaTokyo ? "Asia/Tokyo" : "UTC"; }
inline const char* tzShort(Tz tz) { return tz == Tz::AsiaTokyo ? "JST" : "UTC"; }
inline bool parseTz(const char* s, Tz& out) {
    if (!s) return false;
    if (std::strcmp(s, "Asia/Tokyo") == 0) { out = Tz::AsiaTokyo; return true; }
    if (std::strcmp(s, "UTC") == 0) { out = Tz::Utc; return true; }
    return false;
}

inline bool isLeap(int y) { return (y % 4 == 0 && y % 100 != 0) || y % 400 == 0; }
inline int daysInMonth(int y, int m) {
    static const int kDays[12] = {31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31};
    if (m < 1 || m > 12) return 0;
    return (m == 2 && isLeap(y)) ? 29 : kDays[m - 1];
}

// Howard Hinnant's days_from_civil / civil_from_days.
inline int64_t daysFromCivil(int y, int m, int d) {
    y -= m <= 2;
    const int64_t era = (y >= 0 ? y : y - 399) / 400;
    const int64_t yoe = y - era * 400;
    const int64_t doy = (153 * (m + (m > 2 ? -3 : 9)) + 2) / 5 + d - 1;
    const int64_t doe = yoe * 365 + yoe / 4 - yoe / 100 + doy;
    return era * 146097 + doe - 719468;
}
inline void civilFromDays(int64_t z, int& y, int& m, int& d) {
    z += 719468;
    const int64_t era = (z >= 0 ? z : z - 146096) / 146097;
    const int64_t doe = z - era * 146097;
    const int64_t yoe = (doe - doe / 1460 + doe / 36524 - doe / 146096) / 365;
    const int64_t doy = doe - (365 * yoe + yoe / 4 - yoe / 100);
    const int64_t mp = (5 * doy + 2) / 153;
    d = int(doy - (153 * mp + 2) / 5 + 1);
    m = int(mp < 10 ? mp + 3 : mp - 9);
    y = int(yoe + era * 400 + (m <= 2));
}
// 0 = Sunday.
inline int weekdayFromDays(int64_t z) { return int(z >= -4 ? (z + 4) % 7 : (z + 5) % 7 + 6); }

// Field ranges only (year limited to the supported window).
inline bool validCivil(const Civil& c) {
    return c.year >= kMinYear && c.year <= kMaxYear && c.month >= 1 && c.month <= 12 &&
           c.day >= 1 && c.day <= daysInMonth(c.year, c.month) && c.hour >= 0 &&
           c.hour <= 23 && c.minute >= 0 && c.minute <= 59 && c.second >= 0 && c.second <= 59;
}

inline int64_t civilToEpoch(const Civil& c) {
    return daysFromCivil(c.year, c.month, c.day) * 86400LL + c.hour * 3600LL + c.minute * 60LL +
           c.second;
}
inline Civil epochToCivil(int64_t t) {
    int64_t days = t / 86400;
    int64_t rem = t % 86400;
    if (rem < 0) { rem += 86400; --days; }
    Civil c;
    civilFromDays(days, c.year, c.month, c.day);
    c.hour = int(rem / 3600);
    c.minute = int(rem % 3600 / 60);
    c.second = int(rem % 60);
    return c;
}

inline bool inUtcRange(int64_t t) { return t >= kMinUtc && t <= kMaxUtc; }

// Local civil (in tz) -> UTC. False when the civil value itself is invalid or
// the resulting instant leaves the supported window.
inline bool localToUtc(const Civil& local, Tz tz, int64_t& utc) {
    if (!validCivil(local)) return false;
    utc = civilToEpoch(local) - tzOffsetSec(tz);
    return inUtcRange(utc);
}
inline Civil utcToLocal(int64_t utc, Tz tz) { return epochToCivil(utc + tzOffsetSec(tz)); }

// "YYYY-MM-DDTHH:MM:SS" (seconds optional). Strict digits only.
inline bool parseLocalIso(const char* s, Civil& out) {
    if (!s) return false;
    const size_t n = std::strlen(s);
    if (n != 16 && n != 19) return false;
    auto num = [&](size_t pos, size_t len, int& v) {
        v = 0;
        for (size_t i = pos; i < pos + len; ++i) {
            if (s[i] < '0' || s[i] > '9') return false;
            v = v * 10 + (s[i] - '0');
        }
        return true;
    };
    if (s[4] != '-' || s[7] != '-' || s[10] != 'T' || s[13] != ':') return false;
    Civil c;
    if (!num(0, 4, c.year) || !num(5, 2, c.month) || !num(8, 2, c.day) || !num(11, 2, c.hour) ||
        !num(14, 2, c.minute))
        return false;
    c.second = 0;
    if (n == 19 && (s[16] != ':' || !num(17, 2, c.second))) return false;
    if (!validCivil(c)) return false;
    out = c;
    return true;
}

inline void formatDate(const Civil& c, char* buf, size_t n) {
    std::snprintf(buf, n, "%04d-%02d-%02d", c.year, c.month, c.day);
}
inline void formatTime(const Civil& c, char* buf, size_t n) {
    std::snprintf(buf, n, "%02d:%02d:%02d", c.hour, c.minute, c.second);
}

}  // namespace cd
