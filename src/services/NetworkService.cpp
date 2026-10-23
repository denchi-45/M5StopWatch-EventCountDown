#include "NetworkService.h"
#include <WiFi.h>
#include <atomic>
#include "SettingsStore.h"

namespace cd {

namespace {
// Set from the Wi-Fi event task, consumed in loop().
std::atomic<bool> g_staDisconnected{false};
}

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
    // Keep Wi-Fi credentials out of the shared NVS (the Launcher owns it too).
    WiFi.persistent(false);
    WiFi.setAutoReconnect(false);
    WiFi.onEvent([](arduino_event_id_t, arduino_event_info_t) { g_staDisconnected = true; },
                 ARDUINO_EVENT_WIFI_STA_DISCONNECTED);
    uint8_t mac[6];
    WiFi.macAddress(mac);
    char buf[32];
    snprintf(buf, sizeof buf, "M5-Countdown-%02X%02X", mac[4], mac[5]);
    apSsid_ = buf;
    credentialsChanged(nowMs);
}

void NetworkService::credentialsChanged(int64_t nowMs) {
    ssid_ = store_->wifiSsid();
    hasCreds_ = !ssid_.empty();
    if (WiFi.getMode() & WIFI_MODE_STA) WiFi.disconnect(false, false);
    wasConnected_ = false;
    g_staDisconnected = false;
    sched_.setCredentials(hasCreds_, nowMs);
    applyMode();
}

void NetworkService::applyMode() {
    wifi_mode_t mode = WIFI_OFF;
    if (hasCreds_ && apActive_) mode = WIFI_AP_STA;
    else if (apActive_) mode = WIFI_AP;
    else if (hasCreds_) mode = WIFI_STA;
    if (WiFi.getMode() != mode) WiFi.mode(mode);
}

void NetworkService::beginAttempt() {
    const std::string pw = store_->wifiPassword();
    WiFi.begin(ssid_.c_str(), pw.empty() ? nullptr : pw.c_str());
}

int NetworkService::loop(int64_t nowMs) {
    const bool connected = staConnected();
    int edge = 0;
    if (connected && !wasConnected_) {
        sched_.onConnected();
        edge = +1;
    } else if (!connected && wasConnected_) {
        sched_.onDisconnected(nowMs);
        edge = -1;
    }
    wasConnected_ = connected;

    // Disconnect events raised by tearing down the previous link right before a
    // new attempt are not failures of that attempt.
    if (g_staDisconnected.exchange(false) && !connected && nowMs - attemptStartedMs_ > 1000)
        sched_.onDisconnected(nowMs);
    if (sched_.attemptTimedOut(connected, nowMs)) WiFi.disconnect(false, false);
    if (hasCreds_ && sched_.poll(connected, nowMs)) {
        applyMode();
        attemptStartedMs_ = nowMs;
        beginAttempt();
    }
    return edge;
}

bool NetworkService::staConnected() const {
    return hasCreds_ && WiFi.status() == WL_CONNECTED && WiFi.localIP() != IPAddress(0, 0, 0, 0);
}

StaState NetworkService::staState() const {
    if (!hasCreds_) return StaState::NotConfigured;
    if (staConnected()) return StaState::Connected;
    return sched_.attempting() ? StaState::Connecting : StaState::Offline;
}

bool NetworkService::passwordConfigured() const { return store_ && store_->wifiPasswordConfigured(); }

std::string NetworkService::staIp() const {
    return staConnected() ? std::string(WiFi.localIP().toString().c_str()) : std::string();
}

bool NetworkService::startAp(int64_t) {
    if (apActive_) return true;
    apActive_ = true;
    applyMode();
    // The hardware RNG is only truly random while the radio is on, so the
    // device key is generated (first time only) after the mode switch.
    apPass_ = store_->apPassword();
    if (!WiFi.softAP(apSsid_.c_str(), apPass_.c_str(), 1, 0, 4)) {
        apActive_ = false;
        applyMode();
        return false;
    }
    return true;
}

void NetworkService::stopAp() {
    if (!apActive_) return;
    // Only the AP interface goes away; the STA link and its clock sync stay.
    WiFi.softAPdisconnect(false);
    apActive_ = false;
    applyMode();
}

std::string NetworkService::apIp() const {
    return apActive_ ? std::string(WiFi.softAPIP().toString().c_str()) : std::string();
}

}  // namespace cd
