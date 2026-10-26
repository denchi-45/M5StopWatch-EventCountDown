#include "SettingsStore.h"
#include <esp_random.h>
#include <cstdio>
#include "../hal/KvStore.h"

namespace cd {

namespace {
KvStore prefs;
constexpr const char* kEvents = "events";
// Unreadable event blobs are kept under "evbak_<fnv1a32>": one key per distinct
// payload, so a later corruption never overwrites an earlier backup.
constexpr const char* kEventsBackupPrefix = "evbak_";
constexpr const char* kSettings = "cfg";
constexpr const char* kWifiSsid = "sta_ssid";
constexpr const char* kWifiPass = "sta_pw";
constexpr const char* kApPass = "ap_pw";
constexpr const char* kLkg = "clk_lkg";
constexpr const char* kNtpOk = "ntp_ok";
}  // namespace

bool SettingsStore::begin() {
    ok_ = prefs.begin(kNamespace);
    return ok_;
}

bool SettingsStore::getBlob(const char* key, std::string& out) {
    return ok_ && prefs.getBlob(key, out);
}

bool SettingsStore::putBlobVerified(const char* key, const std::string& value) {
    if (!ok_ || !prefs.putBlob(key, value)) return false;
    std::string back;
    return getBlob(key, back) && back == value;
}

bool SettingsStore::putStringVerified(const char* key, const std::string& value) {
    if (!ok_) return false;
    if (value.empty()) return prefs.remove(key);
    std::string back;
    return prefs.putString(key, value) && prefs.getString(key, back) && back == value;
}

bool SettingsStore::putI64Verified(const char* key, int64_t v) {
    int64_t back = 0;
    return ok_ && prefs.putI64(key, v) && prefs.getI64(key, back) && back == v;
}

std::string SettingsStore::getString(const char* key) {
    std::string v;
    if (ok_) prefs.getString(key, v);
    return v;
}

int64_t SettingsStore::getI64(const char* key) {
    int64_t v = 0;
    return ok_ && prefs.getI64(key, v) ? v : 0;
}

bool SettingsStore::loadEvents(std::string& json) { return getBlob(kEvents, json); }
bool SettingsStore::saveEvents(const std::string& json) { return putBlobVerified(kEvents, json); }
bool SettingsStore::backupCorruptEvents(const std::string& json) {
    uint32_t h = 2166136261u;
    for (unsigned char c : json) h = (h ^ c) * 16777619u;
    char key[16];
    std::snprintf(key, sizeof key, "%s%08lx", kEventsBackupPrefix, (unsigned long)h);
    std::string existing;
    if (getBlob(key, existing)) return existing == json;  // same payload already kept
    return putBlobVerified(key, json);
}
bool SettingsStore::loadSettings(std::string& json) { return getBlob(kSettings, json); }
bool SettingsStore::saveSettings(const std::string& json) {
    return putBlobVerified(kSettings, json);
}

std::string SettingsStore::wifiSsid() { return getString(kWifiSsid); }
bool SettingsStore::wifiPasswordConfigured() { return ok_ && prefs.exists(kWifiPass); }
std::string SettingsStore::wifiPassword() { return getString(kWifiPass); }
bool SettingsStore::saveWifi(const std::string& ssid, const std::string* password) {
    if (!putStringVerified(kWifiSsid, ssid)) return false;
    if (password) return putStringVerified(kWifiPass, *password);
    return true;
}

std::string SettingsStore::apPassword() {
    std::string pw = getString(kApPass);
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

int64_t SettingsStore::lastKnownGoodUtc() { return getI64(kLkg); }
bool SettingsStore::saveLastKnownGoodUtc(int64_t utc) { return putI64Verified(kLkg, utc); }
int64_t SettingsStore::lastNtpSuccessUtc() { return getI64(kNtpOk); }
bool SettingsStore::saveLastNtpSuccessUtc(int64_t utc) { return putI64Verified(kNtpOk, utc); }

}  // namespace cd
