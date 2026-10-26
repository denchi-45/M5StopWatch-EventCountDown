#pragma once
// The UI task (main.cpp) owns App. Other tasks that must call into App (the
// web server's handlers) take this lock first; the UI task holds it for each
// loop step and releases it only while it waits. notify() ends that wait early.
// Device only; the simulator and the preview harness are single-threaded.
#include <cstdint>

namespace cd {
namespace uitask {

void begin();  // on the UI task, before the first lock()
void lock();
void unlock();
bool tryLock(uint32_t timeoutMs);
void notify();  // from any task (not from an ISR)

}  // namespace uitask
}  // namespace cd
