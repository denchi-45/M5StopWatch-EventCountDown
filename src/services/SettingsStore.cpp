#include "SettingsStore.h"
#include <Preferences.h>
#include <esp_random.h>
#include <vector>

namespace cd {

namespace {
Preferences prefs;
constexpr const char* kEvents = "events";
constexpr const char* kEventsLegacy = "events_v2";
constexpr const char* kSettings = "cfg";
constexpr const char* kWifiSsid = "sta_ssid";
constexpr const char* kWifiPass = "sta_pw";
constexpr const char* kApPass = "ap_pw";
constexpr const char* kLkg = "clk_lkg";
constexpr const char* kNtpOk = "ntp_ok";
}  // namespace

bool SettingsStore::begin() {
    ok_ = prefs.begin(kNamespace, false);
    return ok_;
}

bool SettingsStore::getBlob(const char* key, std::string& out) {
    out.clear();
    if (!ok_ || !prefs.isKey(key)) return false;
    const size_t n = prefs.getBytesLength(key);
    if (n == 0) return true;
    std::vector<char> buf(n);
    if (prefs.getBytes(key, buf.data(), n) != n) return false;
    out.assign(buf.data(), n);
    return true;
}

bool SettingsStore::putBlobVerified(const char* key, const std::string& value) {
    if (!ok_) return false;
    if (prefs.putBytes(key, value.data(), value.size()) != value.size()) return false;
    std::string back;
    return getBlob(key, back) && back == value;
}

bool SettingsStore::putStringVerified(const char* key, const std::string& value) {
    if (!ok_) return false;
    if (value.empty()) {
        if (prefs.isKey(key)) prefs.remove(key);
        return !prefs.isKey(key);
    }
    if (prefs.putString(key, value.c_str()) != value.size()) return false;
    return std::string(prefs.getString(key, "").c_str()) == value;
}

bool SettingsStore::putI64Verified(const char* key, int64_t v) {
    if (!ok_) return false;
    prefs.putLong64(key, v);
    return prefs.getLong64(key, v - 1) == v;
}

bool SettingsStore::loadEvents(std::string& json) { return getBlob(kEvents, json); }
bool SettingsStore::saveEvents(const std::string& json) { return putBlobVerified(kEvents, json); }
bool SettingsStore::backupLegacyEvents(const std::string& json) {
    return putBlobVerified(kEventsLegacy, json);
}
bool SettingsStore::loadSettings(std::string& json) { return getBlob(kSettings, json); }
bool SettingsStore::saveSettings(const std::string& json) {
    return putBlobVerified(kSettings, json);
}

std::string SettingsStore::wifiSsid() {
    return ok_ ? std::string(prefs.getString(kWifiSsid, "").c_str()) : std::string();
}
bool SettingsStore::wifiPasswordConfigured() { return ok_ && prefs.isKey(kWifiPass); }
std::string SettingsStore::wifiPassword() {
    return ok_ ? std::string(prefs.getString(kWifiPass, "").c_str()) : std::string();
}
bool SettingsStore::saveWifi(const std::string& ssid, const std::string* password) {
    if (!putStringVerified(kWifiSsid, ssid)) return false;
    if (password) return putStringVerified(kWifiPass, *password);
    return true;
}

std::string SettingsStore::apPassword() {
    std::string pw = ok_ ? std::string(prefs.getString(kApPass, "").c_str()) : std::string();
    if (pw.size() >= 12) return pw;
    // 12 symbols from a 31-letter alphabet without look-alikes (~59 bits),
    // shown as XXXX-XXXX-XXXX on the device screen only.
    static const char kAlphabet[] = "ABCDEFGHJKLMNPQRSTUVWXYZ2345679";
    pw.clear();
    for (int i = 0; i < 12; ++i) {
        if (i && i % 4 == 0) pw += '-';
        pw += kAlphabet[esp_random() % (sizeof kAlphabet - 1)];
    }
    putStringVerified(kApPass, pw);  // still usable for this session if NVS fails
    return pw;
}

int64_t SettingsStore::lastKnownGoodUtc() { return ok_ ? prefs.getLong64(kLkg, 0) : 0; }
bool SettingsStore::saveLastKnownGoodUtc(int64_t utc) { return putI64Verified(kLkg, utc); }
int64_t SettingsStore::lastNtpSuccessUtc() { return ok_ ? prefs.getLong64(kNtpOk, 0) : 0; }
bool SettingsStore::saveLastNtpSuccessUtc(int64_t utc) { return putI64Verified(kNtpOk, utc); }

}  // namespace cd
