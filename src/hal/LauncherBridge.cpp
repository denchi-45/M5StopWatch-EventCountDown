#include "LauncherBridge.h"

#if defined(CD_MULTIFIRM_GUEST)
#include <multifirm_guest.h>
#endif

namespace cd {
namespace launcher {

#if defined(CD_MULTIFIRM_GUEST)
void checkStartupEscape() { multifirm::guest::checkStartupEscape(); }
void poll(bool a, bool b, bool touched, uint32_t nowMs, void (*shutdown)(void*), void* ctx) {
    // Outside a v1 guest slot (standalone install) returnToHost() refuses and
    // logs once per hold; the app keeps running.
    multifirm::guest::poll(a, b, touched, nowMs, shutdown, ctx);
}
bool supported() { return true; }
#else
void checkStartupEscape() {}
void poll(bool, bool, bool, uint32_t, void (*)(void*), void*) {}
bool supported() { return false; }
#endif

}  // namespace launcher
}  // namespace cd
