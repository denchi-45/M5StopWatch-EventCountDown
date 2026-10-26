#pragma once
// Power management for the device build, after wararyoLauncher (its
// architecture.md 7): DFS between 240 and 80 MHz with tickless idle, automatic
// light sleep only while the App allows it, and input interrupts so the UI task
// can wait for its next deadline instead of polling.
// Device only (hal/PowerHal.cpp); the simulator keeps polling.
#include <cstdint>

namespace cd {
namespace power {

// After M5.begin(). Light sleep starts forbidden.
void begin(int maxMhz, int minMhz);
// The M5StopWatch panel draws into a PSRAM framebuffer that M5GFX transfers on
// endWrite() (only the changed area). True when that framebuffer reads back;
// the panel is then declared readable so blending matches a sprite.
bool declareReadableFramebuffer();
// KEY A, KEY B and the touch INT end a wait (and a light sleep). False when the
// board has no such pins or the interrupts failed: the caller must poll.
bool beginInputWake();
bool inputWakeAvailable();
// Light sleep is used only while allowed; call with the panel transfer done.
void setLightSleepAllowed(bool allowed);
// Waits for a notification (input interrupt, web request) or the timeout.
void wait(uint32_t timeoutMs);

}  // namespace power
}  // namespace cd
