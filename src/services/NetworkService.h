#pragma once
// STA (home Wi-Fi, for NTP) and SoftAP (setup web UI) management (spec 6.1, 7).
// Never blocks: connection attempts are timed by WifiScheduler.
#include <cstdint>
#include <string>
#include "../core/SyncScheduler.h"

namespace cd {

class SettingsStore;

enum class StaState : uint8_t { NotConfigured, Offline, Connecting, Connected };
const char* staStateText(StaState s);

class NetworkService {
public:
    void begin(SettingsStore* store, int64_t nowMs);
    // Call every loop. Returns +1 when the STA just got an IP, -1 when it was
    // lost, 0 otherwise (SoftAP client joins never count).
    int loop(int64_t nowMs);

    void credentialsChanged(int64_t nowMs);  // after saving STA settings
    StaState staState() const;
    bool staConnected() const;
    std::string staIp() const;
    std::string staSsid() const { return ssid_; }
    bool passwordConfigured() const;

    bool startAp(int64_t nowMs);
    void stopAp();
    bool apActive() const { return apActive_; }
    std::string apSsid() const { return apSsid_; }
    std::string apPassword() const { return apPass_; }
    std::string apIp() const;

private:
    SettingsStore* store_ = nullptr;
    WifiScheduler sched_;
    std::string ssid_;
    bool hasCreds_ = false;
    bool apActive_ = false;
    bool wasConnected_ = false;
    int64_t attemptStartedMs_ = 0;
    std::string apSsid_, apPass_;
    void applyMode();
    void beginAttempt();
};

}  // namespace cd
