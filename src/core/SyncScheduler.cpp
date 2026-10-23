#include "SyncScheduler.h"
#include <algorithm>

namespace cd {

int64_t retryDelayMs(uint32_t failures) {
    if (failures <= 1) return 60 * 1000;
    if (failures == 2) return 300 * 1000;
    return 900 * 1000;
}

const char* ntpStateText(NtpState s) {
    switch (s) {
        case NtpState::Disabled: return "DISABLED";
        case NtpState::Offline: return "OFFLINE";
        case NtpState::Idle: return "IDLE";
        case NtpState::Syncing: return "SYNCING";
        case NtpState::Synced: return "SYNCED";
        case NtpState::Failed: return "FAILED";
    }
    return "IDLE";
}

bool NtpScheduler::cancel() {
    const bool was = inFlight_;
    inFlight_ = false;
    return was;
}

void NtpScheduler::setEnabled(bool enabled, int64_t now) {
    if (enabled == enabled_) return;
    enabled_ = enabled;
    cancel();
    nextDue_ = enabled ? now : kNever;
    failures_ = 0;
}

bool NtpScheduler::setConnected(bool connected, int64_t now) {
    if (connected == connected_) return false;
    connected_ = connected;
    const bool cancelled = cancel();
    // On (re)connect: sync now, but respect the 60 s spacing in poll().
    nextDue_ = connected ? now : kNever;
    if (connected) failures_ = 0;
    return cancelled;
}

bool NtpScheduler::configChanged(int64_t now) {
    const bool cancelled = cancel();
    failures_ = 0;
    nextDue_ = now;
    return cancelled;
}

bool NtpScheduler::clockOverridden(int64_t now) {
    const bool cancelled = cancel();
    if (connected_) nextDue_ = now + kResyncMs;
    return cancelled;
}

NtpScheduler::Manual NtpScheduler::requestManual(int64_t now) {
    if (!enabled_) return Manual::Disabled;
    if (!connected_) return Manual::Offline;
    if (inFlight_) return Manual::Started;
    nextDue_ = now;
    return now - lastStart_ >= kMinIntervalMs ? Manual::Started : Manual::Scheduled;
}

NtpScheduler::Tick NtpScheduler::poll(int64_t now) {
    if (inFlight_) {
        if (now - inFlightStart_ >= kTimeoutMs) {
            inFlight_ = false;
            onFailure(now);
            return Tick::TimedOut;
        }
        return Tick::None;
    }
    if (!enabled_ || !connected_ || nextDue_ == kNever) return Tick::None;
    const int64_t due = std::max(nextDue_, lastStart_ + kMinIntervalMs);
    if (now < due) return Tick::None;
    inFlight_ = true;
    inFlightStart_ = now;
    lastStart_ = now;
    return Tick::Start;
}

void NtpScheduler::onSuccess(int64_t now) {
    inFlight_ = false;
    everSucceeded_ = true;
    lastFailed_ = false;
    failures_ = 0;
    nextDue_ = now + kResyncMs;
}

void NtpScheduler::onFailure(int64_t now) {
    inFlight_ = false;
    lastFailed_ = true;
    ++failures_;
    nextDue_ = now + retryDelayMs(failures_);
}

NtpState NtpScheduler::state() const {
    if (!enabled_) return NtpState::Disabled;
    if (!connected_) return NtpState::Offline;
    if (inFlight_) return NtpState::Syncing;
    if (lastFailed_) return NtpState::Failed;
    if (everSucceeded_) return NtpState::Synced;
    return NtpState::Idle;
}

void WifiScheduler::setCredentials(bool present, int64_t now) {
    credentials_ = present;
    attempting_ = false;
    failures_ = 0;
    nextDue_ = present ? now : kNever;
}

bool WifiScheduler::poll(bool connected, int64_t now) {
    if (connected || attempting_ || !credentials_ || now < nextDue_) return false;
    attempting_ = true;
    attemptStart_ = now;
    return true;
}

bool WifiScheduler::attemptTimedOut(bool connected, int64_t now) {
    if (!attempting_ || connected) return false;
    if (now - attemptStart_ < kAttemptTimeoutMs) return false;
    attempting_ = false;
    ++failures_;
    nextDue_ = now + retryDelayMs(failures_);
    return true;
}

void WifiScheduler::onConnected() {
    attempting_ = false;
    failures_ = 0;
    nextDue_ = kNever;
}

void WifiScheduler::onDisconnected(int64_t now) {
    if (!credentials_) return;
    if (attempting_) {
        // The attempt itself failed (wrong password, AP not found, ...).
        attempting_ = false;
        ++failures_;
        nextDue_ = now + retryDelayMs(failures_);
        return;
    }
    // A drop after a working connection retries promptly; failures back off.
    if (nextDue_ == kNever) nextDue_ = now + 1000;
}

}  // namespace cd
