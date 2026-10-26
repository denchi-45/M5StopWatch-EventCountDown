// Simulator: KvStore backed by one file per key under sim_data/<namespace>/
// (relative to the working directory). Integers are stored as decimal text.
#include <filesystem>
#include <fstream>
#include <iterator>
#include "../src/hal/KvStore.h"

namespace cd {

namespace {
namespace fs = std::filesystem;
bool readFile(const fs::path& p, std::string& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), {});
    return true;
}
bool writeFile(const fs::path& p, const std::string& v) {
    std::ofstream f(p, std::ios::binary | std::ios::trunc);
    f.write(v.data(), std::streamsize(v.size()));
    return bool(f);
}
}  // namespace

bool KvStore::begin(const char* ns) {
    dir_ = (fs::path("sim_data") / ns).string();
    std::error_code ec;
    fs::create_directories(dir_, ec);
    open_ = !ec;
    return open_;
}

bool KvStore::exists(const char* key) { return open_ && fs::exists(fs::path(dir_) / key); }

bool KvStore::remove(const char* key) {
    std::error_code ec;
    fs::remove(fs::path(dir_) / key, ec);
    return !exists(key);
}

bool KvStore::getBlob(const char* key, std::string& out) {
    out.clear();
    return open_ && readFile(fs::path(dir_) / key, out);
}
bool KvStore::putBlob(const char* key, const std::string& v) { return open_ && writeFile(fs::path(dir_) / key, v); }
bool KvStore::getString(const char* key, std::string& out) { return getBlob(key, out); }
bool KvStore::putString(const char* key, const std::string& v) { return putBlob(key, v); }

bool KvStore::getI64(const char* key, int64_t& out) {
    std::string s;
    if (!getBlob(key, s)) return false;
    try {
        out = std::stoll(s);
    } catch (...) {
        return false;
    }
    return true;
}
bool KvStore::putI64(const char* key, int64_t v) { return putBlob(key, std::to_string(v)); }

}  // namespace cd
