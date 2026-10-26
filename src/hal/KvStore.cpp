#include "KvStore.h"
#include <nvs.h>
#include <nvs_flash.h>

namespace cd {

bool KvStore::begin(const char* ns) {
    nvs_handle_t h;
    open_ = nvs_open(ns, NVS_READWRITE, &h) == ESP_OK;
    if (open_) handle_ = h;
    return open_;
}

bool KvStore::exists(const char* key) {
    return open_ && nvs_find_key(handle_, key, nullptr) == ESP_OK;
}

bool KvStore::remove(const char* key) {
    if (!open_) return false;
    if (!exists(key)) return true;
    if (nvs_erase_key(handle_, key) != ESP_OK || nvs_commit(handle_) != ESP_OK) return false;
    return !exists(key);
}

KvRead KvStore::readBlob(const char* key, std::string& out) {
    out.clear();
    if (!open_) return KvRead::Error;
    size_t n = 0;
    const esp_err_t e = nvs_get_blob(handle_, key, nullptr, &n);
    if (e == ESP_ERR_NVS_NOT_FOUND) return KvRead::Missing;
    if (e != ESP_OK) return KvRead::Error;
    out.resize(n);
    if (n && nvs_get_blob(handle_, key, &out[0], &n) != ESP_OK) {
        out.clear();
        return KvRead::Error;
    }
    out.resize(n);
    return KvRead::Ok;
}

bool KvStore::putBlob(const char* key, const std::string& value) {
    return open_ && nvs_set_blob(handle_, key, value.data(), value.size()) == ESP_OK &&
           nvs_commit(handle_) == ESP_OK;
}

bool KvStore::getString(const char* key, std::string& out) {
    out.clear();
    size_t n = 0;  // including the terminating NUL
    if (!open_ || nvs_get_str(handle_, key, nullptr, &n) != ESP_OK || n == 0) return false;
    out.resize(n);
    if (nvs_get_str(handle_, key, &out[0], &n) != ESP_OK) return false;
    out.resize(n - 1);
    return true;
}

bool KvStore::putString(const char* key, const std::string& value) {
    return open_ && nvs_set_str(handle_, key, value.c_str()) == ESP_OK && nvs_commit(handle_) == ESP_OK;
}

bool KvStore::getI64(const char* key, int64_t& out) {
    return open_ && nvs_get_i64(handle_, key, &out) == ESP_OK;
}

bool KvStore::putI64(const char* key, int64_t value) {
    return open_ && nvs_set_i64(handle_, key, value) == ESP_OK && nvs_commit(handle_) == ESP_OK;
}

}  // namespace cd
