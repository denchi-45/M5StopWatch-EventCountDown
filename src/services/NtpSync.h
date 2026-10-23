#pragma once
// SNTP query on a worker task (spec 6.1, 6.2). The main loop never waits: it
// posts a request tagged with a generation and later polls for the result.
// Results whose generation is no longer current are discarded by the caller.
// The worker never touches the clock, RTC, NVS or UI.
#include <atomic>
#include <cstdint>

namespace cd {

struct NtpResult {
    uint32_t generation = 0;
    bool ok = false;
    int64_t utcMs = 0;         // server time at the moment of receipt
    int64_t monoMsAtRx = 0;    // monotonic time of receipt
    char error[40] = {0};
};

class NtpSync {
public:
    static constexpr int64_t kTimeoutMs = 10 * 1000;

    bool begin();
    // False when the worker is still busy with an older request.
    bool request(uint32_t generation, const char* server);
    bool poll(NtpResult& out);

private:
    void* reqQueue_ = nullptr;
    void* resQueue_ = nullptr;
    std::atomic<bool> busy_{false};
    static void task(void* self);
};

}  // namespace cd
