// Force-included in the native (SDL) simulator build only.
// Provides the few Arduino-isms the app uses outside the replaced services.
#pragma once
#if defined(__cplusplus)
#include <cstdarg>
#include <cstdio>

namespace cd_sim {
struct SerialPort {
    void begin(unsigned long) {}
    int printf(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
        va_list ap;
        va_start(ap, fmt);
        const int n = std::vprintf(fmt, ap);
        va_end(ap);
        std::fflush(stdout);
        return n;
    }
};
inline SerialPort Serial;
}  // namespace cd_sim
using cd_sim::Serial;
#endif
