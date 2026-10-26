#pragma once
// wararyoLauncher / M5StopWatch-MultiFirm guest integration (spec 4, 6).
// A+B hold (1.6 s, cancelled by touch) returns to the Launcher; holding KEY B
// at power-on escapes to the Launcher before M5.begin(). Builds without
// CD_MULTIFIRM_GUEST compile to no-ops.
#include <cstdint>

namespace cd {

namespace launcher {
// Call first in setup(), before M5.begin().
void checkStartupEscape();
// Call every loop with raw levels. `shutdown` silences sound/vibration.
void poll(bool a, bool b, bool touched, uint32_t nowMs, void (*shutdown)(void*), void* ctx);
bool supported();
}  // namespace launcher

}  // namespace cd
