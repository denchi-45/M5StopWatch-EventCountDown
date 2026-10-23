// Simulator: NVS Preferences replacement backed by one file per key under
// sim_data/<namespace>/ (relative to the working directory).
#pragma once
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <string>

class Preferences {
public:
    bool begin(const char* name, bool /*readOnly*/) {
        dir_ = std::filesystem::path("sim_data") / name;
        std::error_code ec;
        std::filesystem::create_directories(dir_, ec);
        return !ec;
    }
    bool isKey(const char* key) const { return std::filesystem::exists(path(key)); }
    bool remove(const char* key) {
        std::error_code ec;
        return std::filesystem::remove(path(key), ec);
    }
    size_t getBytesLength(const char* key) const { return read(key).size(); }
    size_t getBytes(const char* key, void* buf, size_t len) const {
        const std::string v = read(key);
        if (v.size() > len) return 0;
        std::memcpy(buf, v.data(), v.size());
        return v.size();
    }
    size_t putBytes(const char* key, const void* buf, size_t len) {
        return write(key, std::string(static_cast<const char*>(buf), len)) ? len : 0;
    }
    std::string getString(const char* key, const char* def) const {
        return isKey(key) ? read(key) : std::string(def);
    }
    size_t putString(const char* key, const char* value) {
        return write(key, value) ? std::strlen(value) : 0;
    }
    int64_t getLong64(const char* key, int64_t def) const {
        return isKey(key) ? std::stoll(read(key)) : def;
    }
    size_t putLong64(const char* key, int64_t v) { return write(key, std::to_string(v)) ? 8 : 0; }

private:
    std::filesystem::path dir_;
    std::filesystem::path path(const char* key) const { return dir_ / key; }
    std::string read(const char* key) const {
        std::ifstream f(path(key), std::ios::binary);
        return std::string(std::istreambuf_iterator<char>(f), {});
    }
    bool write(const char* key, const std::string& v) {
        std::ofstream f(path(key), std::ios::binary | std::ios::trunc);
        f.write(v.data(), std::streamsize(v.size()));
        return bool(f);
    }
};
