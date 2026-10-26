#pragma once
// NVS persistence in the app's own namespace (shared NVS partition under
// MultiFirm, so nothing outside "m5cd_v2" is touched).
// Every write is verified by reading it back (validate -> write -> verify).
#include <cstdint>
#include <string>
#include "../hal/KvStore.h"

namespace cd {

class SettingsStore {
public:
    static constexpr const char* kNamespace = "m5cd_v2";

    bool begin();

    // Error (not Missing) when the namespace or the key could not be read.
    KvRead loadEvents(std::string& json);
    bool saveEvents(const std::string& json);
    // Keeps unreadable event data aside before the slot may be reused. Never
    // overwrites an earlier backup; true only when the copy is verified.
    bool backupCorruptEvents(const std::string& json);

    bool loadSettings(std::string& json);
    bool saveSettings(const std::string& json);

    // Home Wi-Fi (STA). The password never leaves the device through the API.
    std::string wifiSsid();
    bool wifiPasswordConfigured();
    std::string wifiPassword();
    // password == nullptr keeps the stored one; "" deletes it. With a password,
    // SSID and password change together: on failure both previous values are
    // restored.
    bool saveWifi(const std::string& ssid, const std::string* password);

    // SoftAP WPA2 key: generated once per device from the hardware RNG.
    std::string apPassword();

    int64_t lastKnownGoodUtc();
    bool saveLastKnownGoodUtc(int64_t utc);
    int64_t lastNtpSuccessUtc();
    bool saveLastNtpSuccessUtc(int64_t utc);

private:
    bool ok_ = false;
    bool putBlobVerified(const char* key, const std::string& value);
    bool getBlob(const char* key, std::string& out);
    bool putStringVerified(const char* key, const std::string& value);
    bool putI64Verified(const char* key, int64_t v);
    std::string getString(const char* key);  // "" when missing
    int64_t getI64(const char* key);         // 0 when missing
};

}  // namespace cd
