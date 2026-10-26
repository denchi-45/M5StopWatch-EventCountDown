// Simulator: network services are not available on the PC.
// Wi-Fi never connects (so NTP never runs); the setup AP "starts" so its
// device screen can be checked, but no web server is served.
#include <cstdio>
#include "../src/services/NetworkService.h"
#include "../src/services/NtpSync.h"
#include "../src/services/SettingsStore.h"
#include "../src/services/WebConfigServer.h"


namespace cd {

const char* staStateText(StaState s) {
    switch (s) {
        case StaState::NotConfigured: return "NOT CONFIGURED";
        case StaState::Offline: return "OFFLINE";
        case StaState::Connecting: return "CONNECTING";
        case StaState::Connected: return "CONNECTED";
    }
    return "OFFLINE";
}

void NetworkService::begin(SettingsStore* store, int64_t nowMs) {
    store_ = store;
    apSsid_ = "EventCountDown-SIM";
    credentialsChanged(nowMs);
}
void NetworkService::credentialsChanged(int64_t) {
    ssid_ = store_->wifiSsid();
    hasCreds_ = !ssid_.empty();
}
void NetworkService::applyMode() {}
void NetworkService::beginAttempt() {}
int NetworkService::loop(int64_t) { return 0; }
bool NetworkService::staConnected() const { return false; }
StaState NetworkService::staState() const {
    return hasCreds_ ? StaState::Offline : StaState::NotConfigured;
}
bool NetworkService::passwordConfigured() const { return store_ && store_->wifiPasswordConfigured(); }
std::string NetworkService::staIp() const { return std::string(); }
bool NetworkService::startAp(int64_t) {
    apActive_ = true;
    apPass_ = store_->apPassword();
    std::printf("[sim] setup AP shown on screen only; no web server in the simulator\n");
    return true;
}
void NetworkService::stopAp() { apActive_ = false; }
std::string NetworkService::apIp() const { return apActive_ ? "192.168.4.1" : std::string(); }

bool NtpSync::begin() { return true; }
bool NtpSync::request(uint32_t, const char*) { return false; }
bool NtpSync::poll(NtpResult&) { return false; }

WebConfigServer::WebConfigServer(App& app) : app_(app) {}
WebConfigServer::~WebConfigServer() = default;
void WebConfigServer::start() {}
void WebConfigServer::stop() {}

}  // namespace cd
