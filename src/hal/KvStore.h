#pragma once
// One NVS namespace as typed key/value pairs (blob, string, int64), the same
// entry types Arduino's Preferences wrote, so data stored by the Arduino build
// stays readable. Every put commits before returning.
// Device: hal/KvStore.cpp (nvs). Simulator: sim/SimKvStore.cpp (files).
#include <cstdint>
#include <string>

namespace cd {

class KvStore {
public:
    bool begin(const char* ns);
    bool exists(const char* key);
    bool remove(const char* key);  // true when the key is gone afterwards
    // False when the key is missing or unreadable.
    bool getBlob(const char* key, std::string& out);
    bool putBlob(const char* key, const std::string& value);
    bool getString(const char* key, std::string& out);
    bool putString(const char* key, const std::string& value);
    bool getI64(const char* key, int64_t& out);
    bool putI64(const char* key, int64_t value);

private:
    uint32_t handle_ = 0;  // nvs_handle_t
    std::string dir_;      // simulator
    bool open_ = false;
};

}  // namespace cd
