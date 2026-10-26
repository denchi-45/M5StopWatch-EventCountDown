#include "NetworkService.h"
#include <esp_event.h>
#include <esp_mac.h>
#include <esp_netif.h>
#include <esp_wifi.h>
#include <atomic>
#include <cstdio>
#include <cstring>
#include "SettingsStore.h"

namespace cd {

namespace {
// Set from the default event loop task, consumed in loop().
std::atomic<bool> g_staDisconnected{false};
std::atomic<uint32_t> g_staIp{0};  // network byte order, 0 = no address
esp_netif_t* g_apNetif = nullptr;

void onWifiEvent(void*, esp_event_base_t base, int32_t id, void* data) {
    if (base == WIFI_EVENT && id == WIFI_EVENT_STA_DISCONNECTED) {
        g_staIp = 0;
        g_staDisconnected = true;
    } else if (base == IP_EVENT && id == IP_EVENT_STA_GOT_IP) {
        g_staIp = static_cast<ip_event_got_ip_t*>(data)->ip_info.ip.addr;
    } else if (base == IP_EVENT && id == IP_EVENT_STA_LOST_IP) {
        g_staIp = 0;
    }
}

std::string ipText(uint32_t addr) {
    if (!addr) return std::string();
    esp_ip4_addr_t ip;
    ip.addr = addr;
    char buf[16];
    std::snprintf(buf, sizeof buf, IPSTR, IP2STR(&ip));
    return buf;
}

template <size_t N>
void copyField(uint8_t (&dst)[N], const std::string& src) {
    std::memset(dst, 0, N);
    std::memcpy(dst, src.data(), src.size() < N ? src.size() : N);
}
}  // namespace

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
    uint8_t mac[6] = {0};
    esp_read_mac(mac, ESP_MAC_WIFI_STA);
    char buf[32];
    std::snprintf(buf, sizeof buf, "EventCountDown-%02X%02X", mac[4], mac[5]);
    apSsid_ = buf;
    credentialsChanged(nowMs);
}

bool NetworkService::initRadio() {
    if (inited_) return true;
    if (esp_netif_init() != ESP_OK) return false;
    const esp_err_t loop = esp_event_loop_create_default();
    if (loop != ESP_OK && loop != ESP_ERR_INVALID_STATE) return false;
    esp_netif_create_default_wifi_sta();
    g_apNetif = esp_netif_create_default_wifi_ap();
    wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
    // Keep Wi-Fi credentials out of the shared NVS (the Launcher owns it too).
    cfg.nvs_enable = 0;
    if (esp_wifi_init(&cfg) != ESP_OK) return false;
    esp_wifi_set_storage(WIFI_STORAGE_RAM);
    esp_event_handler_register(WIFI_EVENT, WIFI_EVENT_STA_DISCONNECTED, &onWifiEvent, nullptr);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_GOT_IP, &onWifiEvent, nullptr);
    esp_event_handler_register(IP_EVENT, IP_EVENT_STA_LOST_IP, &onWifiEvent, nullptr);
    inited_ = true;
    return true;
}

void NetworkService::credentialsChanged(int64_t nowMs) {
    ssid_ = store_->wifiSsid();
    hasCreds_ = !ssid_.empty();
    if (mode_ & WIFI_MODE_STA) esp_wifi_disconnect();
    g_staIp = 0;
    wasConnected_ = false;
    g_staDisconnected = false;
    sched_.setCredentials(hasCreds_, nowMs);
    applyMode();
}

void NetworkService::applyMode() {
    wifi_mode_t mode = WIFI_MODE_NULL;
    if (hasCreds_ && apActive_) mode = WIFI_MODE_APSTA;
    else if (apActive_) mode = WIFI_MODE_AP;
    else if (hasCreds_) mode = WIFI_MODE_STA;
    if (mode == mode_) return;
    if (mode == WIFI_MODE_NULL) {
        esp_wifi_stop();
        g_staIp = 0;
        mode_ = WIFI_MODE_NULL;
        return;
    }
    if (!initRadio()) return;
    if (esp_wifi_set_mode(mode) != ESP_OK) return;
    if (mode_ == WIFI_MODE_NULL && esp_wifi_start() != ESP_OK) return;
    if (!(mode & WIFI_MODE_STA)) g_staIp = 0;
    mode_ = mode;
}

void NetworkService::beginAttempt() {
    if (!(mode_ & WIFI_MODE_STA)) return;
    const std::string pw = store_->wifiPassword();
    wifi_config_t cfg{};
    copyField(cfg.sta.ssid, ssid_);
    copyField(cfg.sta.password, pw);
    cfg.sta.threshold.authmode = pw.empty() ? WIFI_AUTH_OPEN : WIFI_AUTH_WPA2_PSK;
    cfg.sta.pmf_cfg.capable = true;
    esp_wifi_set_config(WIFI_IF_STA, &cfg);
    esp_wifi_connect();
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
    if (sched_.attemptTimedOut(connected, nowMs)) esp_wifi_disconnect();
    if (hasCreds_ && sched_.poll(connected, nowMs)) {
        applyMode();
        attemptStartedMs_ = nowMs;
        beginAttempt();
    }
    return edge;
}

bool NetworkService::staConnected() const {
    return hasCreds_ && (mode_ & WIFI_MODE_STA) && g_staIp != 0;
}

StaState NetworkService::staState() const {
    if (!hasCreds_) return StaState::NotConfigured;
    if (staConnected()) return StaState::Connected;
    return sched_.attempting() ? StaState::Connecting : StaState::Offline;
}

bool NetworkService::passwordConfigured() const { return store_ && store_->wifiPasswordConfigured(); }

std::string NetworkService::staIp() const { return staConnected() ? ipText(g_staIp) : std::string(); }

bool NetworkService::startAp(int64_t) {
    if (apActive_) return true;
    apActive_ = true;
    applyMode();
    if (!(mode_ & WIFI_MODE_AP)) {
        apActive_ = false;
        applyMode();
        return false;
    }
    // The hardware RNG is only truly random while the radio is on, so the
    // device key is generated (first time only) after the mode switch.
    apPass_ = store_->apPassword();
    wifi_config_t cfg{};
    copyField(cfg.ap.ssid, apSsid_);
    cfg.ap.ssid_len = uint8_t(apSsid_.size());
    copyField(cfg.ap.password, apPass_);
    cfg.ap.channel = 1;
    cfg.ap.authmode = WIFI_AUTH_WPA2_PSK;
    cfg.ap.max_connection = 4;
    cfg.ap.pmf_cfg.capable = true;
    if (esp_wifi_set_config(WIFI_IF_AP, &cfg) != ESP_OK) {
        apActive_ = false;
        applyMode();
        return false;
    }
    return true;
}

void NetworkService::stopAp() {
    if (!apActive_) return;
    // Only the AP interface goes away; the STA link and its clock sync stay.
    apActive_ = false;
    applyMode();
}

std::string NetworkService::apIp() const {
    esp_netif_ip_info_t info{};
    if (!apActive_ || !g_apNetif || esp_netif_get_ip_info(g_apNetif, &info) != ESP_OK) return std::string();
    return ipText(info.ip.addr);
}

}  // namespace cd
