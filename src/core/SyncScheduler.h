#pragma once
// Timing decisions for STA connection attempts and NTP requests (spec 6.1).
// All times are monotonic milliseconds, never the (correctable) wall clock.
#include <cstdint>

namespace cd {

constexpr int64_t kNever = INT64_MAX;

// 60 s, 300 s, then 900 s between failed attempts.
int64_t retryDelayMs(uint32_t failures);

enum class NtpState : uint8_t { Disabled, Offline, Idle, Syncing, Synced, Failed };
const char* ntpStateText(NtpState s);

class NtpScheduler {
public:
    static constexpr int64_t kMinIntervalMs = 60 * 1000;
    static constexpr int64_t kTimeoutMs = 10 * 1000;
    static constexpr int64_t kResyncMs = 6LL * 3600 * 1000;

    enum class Manual : uint8_t { Started, Scheduled, Disabled, Offline };
    enum class Tick : uint8_t { None, Start, TimedOut };

    void setEnabled(bool enabled, int64_t now);
    // STA got an IP (true) or lost it (false). Returns true if an in-flight
    // request was cancelled (the caller must invalidate its generation).
    bool setConnected(bool connected, int64_t now);
    // Server or enable change: schedule a fresh sync, cancelling any in-flight one.
    bool configChanged(int64_t now);
    // Clock set manually/by browser or RTC mode change: drop in-flight results;
    // the next automatic correction is on reconnect or after 6 hours.
    bool clockOverridden(int64_t now);
    Manual requestManual(int64_t now);

    // Call every loop. Start => send a request now. TimedOut => treat as failure
    // (already recorded) and invalidate the generation.
    Tick poll(int64_t now);
    void onSuccess(int64_t now);
    void onFailure(int64_t now);

    NtpState state() const;
    bool inFlight() const { return inFlight_; }
    int64_t nextDueMs() const { return nextDue_; }

private:
    bool enabled_ = true;
    bool connected_ = false;
    bool inFlight_ = false;
    bool everSucceeded_ = false;
    bool lastFailed_ = false;
    uint32_t failures_ = 0;
    int64_t lastStart_ = -kMinIntervalMs * 2;
    int64_t inFlightStart_ = 0;
    int64_t nextDue_ = kNever;
    bool cancel();
};

// STA connection attempts: 15 s per try, then 60/300/900 s back-off.
class WifiScheduler {
public:
    static constexpr int64_t kAttemptTimeoutMs = 15 * 1000;

    void setCredentials(bool present, int64_t now);  // also resets back-off
    // Returns true when a connection attempt should begin now.
    bool poll(bool connected, int64_t now);
    // Returns true when the current attempt timed out (caller disconnects).
    bool attemptTimedOut(bool connected, int64_t now);
    void onConnected();
    // Call on a STA disconnect event (during an attempt or after a connection).
    void onDisconnected(int64_t now);
    bool attempting() const { return attempting_; }

private:
    bool credentials_ = false;
    bool attempting_ = false;
    uint32_t failures_ = 0;
    int64_t attemptStart_ = 0;
    int64_t nextDue_ = kNever;
};

}  // namespace cd
