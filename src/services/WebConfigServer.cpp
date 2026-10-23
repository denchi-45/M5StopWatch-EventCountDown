#include "WebConfigServer.h"
#include <ArduinoJson.h>
#include <WebServer.h>
#include <WiFi.h>
#include <esp_random.h>
#include <uri/UriBraces.h>
#include "../app/App.h"
#include "JsonCodec.h"

extern const uint8_t index_html_start[] asm("_binary_web_index_html_start");
extern const uint8_t index_html_end[] asm("_binary_web_index_html_end");

namespace cd {

namespace {
constexpr size_t kMaxBody = 8 * 1024;
const char* kHeaders[] = {"Host", "X-CD-Session"};

std::string randomHex(int bytes) {
    static const char* hex = "0123456789abcdef";
    std::string s;
    for (int i = 0; i < bytes; ++i) {
        const uint8_t b = uint8_t(esp_random());
        s += hex[b >> 4];
        s += hex[b & 15];
    }
    return s;
}

const char* repoStatus(RepoError e, int& code) {
    switch (e) {
        case RepoError::Ok: code = 200; break;
        case RepoError::NotFound: code = 404; break;
        case RepoError::Full:
        case RepoError::Disabled: code = 409; break;
        default: code = 400; break;
    }
    return repoErrorText(e);
}

// Strict event body: every field present with the right JSON type.
bool parseEventInput(JsonObjectConst o, EventInput& in, const char*& err) {
    if (!o["name"].is<const char*>()) { err = "name must be a string"; return false; }
    if (!o["startUtc"].is<int64_t>() || !o["targetUtc"].is<int64_t>()) {
        err = "startUtc/targetUtc must be integer UTC seconds";
        return false;
    }
    if (!o["timezone"].is<const char*>() || !parseTz(o["timezone"].as<const char*>(), in.tz)) {
        err = "timezone must be Asia/Tokyo or UTC";
        return false;
    }
    if (!o["enabled"].is<bool>()) { err = "enabled must be a boolean"; return false; }
    in.name = o["name"].as<const char*>();
    in.startUtc = o["startUtc"].as<int64_t>();
    in.targetUtc = o["targetUtc"].as<int64_t>();
    in.enabled = o["enabled"].as<bool>();
    return true;
}
}  // namespace

WebConfigServer::WebConfigServer(App& app) : app_(app) {}
WebConfigServer::~WebConfigServer() { stop(); }

void WebConfigServer::start() {
    if (server_) return;
    session_ = randomHex(16);
    exitAt_ = 0;
    previewToken_.clear();
    server_.reset(new WebServer(80));
    server_->collectHeaders(kHeaders, 2);
    routes();
    server_->begin();
}

void WebConfigServer::stop() {
    if (!server_) return;
    server_->stop();
    server_.reset();
    session_.clear();
    previewToken_.clear();
    exitAt_ = 0;
}

void WebConfigServer::loop() {
    if (server_) server_->handleClient();
}

bool WebConfigServer::allowed(bool mutating) {
    WebServer& s = *server_;
    const IPAddress ap = WiFi.softAPIP();
    // Only the setup network, never the home network the STA is joined to.
    if (s.client().localIP() != ap) {
        sendError(403, "setup API is only available on the device access point");
        return false;
    }
    const String host = s.header("Host");
    const String ip = ap.toString();
    if (host != ip && host != ip + ":80") {
        sendError(403, "unexpected Host header");
        return false;
    }
    if (mutating && (session_.empty() || s.header("X-CD-Session") != session_.c_str())) {
        sendError(401, "missing or stale session; reload the page");
        return false;
    }
    if (s.clientContentLength() > int(kMaxBody)) {
        sendError(413, "request body too large");
        return false;
    }
    return true;
}

bool WebConfigServer::readJson(void* docPtr) {
    JsonDocument& doc = *static_cast<JsonDocument*>(docPtr);
    const String& body = server_->arg("plain");
    if (body.length() > kMaxBody) { sendError(413, "request body too large"); return false; }
    if (deserializeJson(doc, body.c_str(), body.length()) || !doc.is<JsonObject>()) {
        sendError(400, "body must be a JSON object");
        return false;
    }
    return true;
}

void WebConfigServer::sendJson(int code, const std::string& body) {
    server_->sendHeader("Cache-Control", "no-store");
    server_->sendHeader("X-Content-Type-Options", "nosniff");
    server_->send(code, "application/json", body.c_str());
}

void WebConfigServer::sendError(int code, const char* msg) {
    JsonDocument doc;
    doc["ok"] = false;
    doc["error"] = msg;
    std::string out;
    serializeJson(doc, out);
    sendJson(code, out);
}

void WebConfigServer::sendOk() { sendJson(200, "{\"ok\":true}"); }

void WebConfigServer::routes() {
    WebServer& s = *server_;
    s.on("/", HTTP_GET, [this]() {
        if (!allowed(false)) return;
        server_->sendHeader("Cache-Control", "no-store");
        server_->sendHeader("X-Frame-Options", "DENY");
        server_->sendHeader("Content-Security-Policy",
                            "default-src 'self'; script-src 'unsafe-inline'; style-src 'unsafe-inline'; "
                            "connect-src 'self'; frame-ancestors 'none'");
        const size_t len = size_t(index_html_end - index_html_start) - 1;  // trailing NUL
        server_->send_P(200, "text/html; charset=utf-8",
                        reinterpret_cast<const char*>(index_html_start), len);
    });
    s.on("/api/session", HTTP_GET, [this]() {
        if (!allowed(false)) return;
        JsonDocument doc;
        doc["token"] = session_;
        std::string out;
        serializeJson(doc, out);
        sendJson(200, out);
    });
    s.on("/api/status", HTTP_GET, [this]() { if (allowed(false)) handleStatus(); });
    s.on("/api/events/order", HTTP_PUT, [this]() { if (allowed(true)) handleOrder(); });
    s.on("/api/events", HTTP_GET, [this]() { if (allowed(false)) handleEventsGet(); });
    s.on("/api/events", HTTP_POST, [this]() { if (allowed(true)) handleEventsPost(); });
    s.on(UriBraces("/api/events/{}"), HTTP_PUT, [this]() { if (allowed(true)) handleEventPut(); });
    s.on(UriBraces("/api/events/{}"), HTTP_DELETE, [this]() { if (allowed(true)) handleEventDelete(); });
    s.on("/api/selection", HTTP_PUT, [this]() { if (allowed(true)) handleSelection(); });
    s.on("/api/settings", HTTP_GET, [this]() { if (allowed(false)) handleSettingsGet(); });
    s.on("/api/settings", HTTP_PUT, [this]() { if (allowed(true)) handleSettingsPut(); });
    s.on("/api/rtc/preview", HTTP_POST, [this]() { if (allowed(true)) handleRtcPreview(); });
    s.on("/api/wifi", HTTP_GET, [this]() { if (allowed(false)) handleWifiGet(); });
    s.on("/api/wifi", HTTP_PUT, [this]() { if (allowed(true)) handleWifiPut(); });
    s.on("/api/time", HTTP_POST, [this]() { if (allowed(true)) handleTime(); });
    s.on("/api/ntp/sync", HTTP_POST, [this]() { if (allowed(true)) handleNtp(); });
    s.on("/api/session/activity", HTTP_POST, [this]() { if (allowed(true)) handleActivity(); });
    s.on("/api/setup/exit", HTTP_POST, [this]() { if (allowed(true)) handleExit(); });
    s.onNotFound([this]() { sendError(404, "not found"); });
}

// ---------------------------------------------------------------- status

void WebConfigServer::handleStatus() {
    ClockService& clk = app_.clock();
    JsonDocument doc;
    doc["clockValid"] = clk.valid();
    doc["clockSource"] = clockSourceText(clk.source());
    doc["rtcAvailable"] = clk.rtcAvailable();
    doc["rtcValid"] = clk.rtcValid();
    doc["rtcWriteStatus"] = rtcWriteStatusText(clk.rtcWriteStatus());
    doc["rtcInterpretation"] = rtcModeId(clk.rtcMode());
    if (clk.valid()) doc["nowUtc"] = clk.nowUtc();
    else doc["nowUtc"] = nullptr;
    doc["timezone"] = tzId(app_.settings().displayTz);
    JsonObject ntp = doc["ntp"].to<JsonObject>();
    ntp["state"] = ntpStateText(app_.ntpState());
    if (app_.lastNtpSuccessUtc() > 0) ntp["lastNtpSuccessUtc"] = app_.lastNtpSuccessUtc();
    else ntp["lastNtpSuccessUtc"] = nullptr;
    if (!app_.lastNtpError().empty()) ntp["lastNtpError"] = app_.lastNtpError();
    else ntp["lastNtpError"] = nullptr;
    JsonObject wifi = doc["wifi"].to<JsonObject>();
    wifi["state"] = staStateText(app_.net().staState());
    wifi["ssid"] = app_.net().staSsid();
    wifi["ip"] = app_.net().staIp();
    doc["apRemainingSec"] = app_.apRemainingMs() / 1000;
    doc["selectedEventId"] = app_.events().selectedId();
    doc["board"] = app_.caps().boardName;
    if (!app_.configError().empty()) doc["configError"] = app_.configError();
    std::string out;
    serializeJson(doc, out);
    sendJson(200, out);
}

// ---------------------------------------------------------------- events

void WebConfigServer::handleEventsGet() {
    JsonDocument doc;
    const EventRepository& repo = app_.events();
    if (repo.selectedId().empty()) doc["selectedEventId"] = nullptr;
    else doc["selectedEventId"] = repo.selectedId();
    doc["max"] = kMaxEvents;
    JsonArray arr = doc["events"].to<JsonArray>();
    for (const auto& e : repo.events()) eventToJson(e, arr.add<JsonObject>());
    std::string out;
    serializeJson(doc, out);
    sendJson(200, out);
}

void WebConfigServer::handleEventsPost() {
    JsonDocument doc;
    if (!readJson(&doc)) return;
    EventInput in;
    const char* err = nullptr;
    if (!parseEventInput(doc.as<JsonObjectConst>(), in, err)) return sendError(400, err);
    std::string id, detail;
    bool saveFailed = false;
    const RepoError r = app_.addEvent(in, &id, &detail, saveFailed);
    if (saveFailed) return sendError(500, "could not save to NVS; nothing changed");
    int code;
    const char* msg = repoStatus(r, code);
    if (r != RepoError::Ok) return sendError(code, detail.empty() ? msg : detail.c_str());
    app_.noteSetupActivity();
    JsonDocument res;
    res["ok"] = true;
    res["id"] = id;
    std::string out;
    serializeJson(res, out);
    sendJson(201, out);
}

void WebConfigServer::handleEventPut() {
    JsonDocument doc;
    if (!readJson(&doc)) return;
    EventInput in;
    const char* err = nullptr;
    if (!parseEventInput(doc.as<JsonObjectConst>(), in, err)) return sendError(400, err);
    std::string detail;
    bool saveFailed = false;
    const RepoError r = app_.updateEvent(server_->pathArg(0).c_str(), in, &detail, saveFailed);
    if (saveFailed) return sendError(500, "could not save to NVS; nothing changed");
    int code;
    const char* msg = repoStatus(r, code);
    if (r != RepoError::Ok) return sendError(code, detail.empty() ? msg : detail.c_str());
    app_.noteSetupActivity();
    sendOk();
}

void WebConfigServer::handleEventDelete() {
    bool saveFailed = false;
    const RepoError r = app_.deleteEvent(server_->pathArg(0).c_str(), saveFailed);
    if (saveFailed) return sendError(500, "could not save to NVS; nothing changed");
    int code;
    const char* msg = repoStatus(r, code);
    if (r != RepoError::Ok) return sendError(code, msg);
    app_.noteSetupActivity();
    sendOk();
}

void WebConfigServer::handleOrder() {
    JsonDocument doc;
    if (!readJson(&doc)) return;
    JsonArrayConst ids = doc["ids"].as<JsonArrayConst>();
    if (ids.isNull()) return sendError(400, "ids must be an array");
    std::vector<std::string> list;
    for (JsonVariantConst v : ids) {
        if (!v.is<const char*>()) return sendError(400, "ids must be strings");
        list.emplace_back(v.as<const char*>());
    }
    bool saveFailed = false;
    const RepoError r = app_.reorderEvents(list, saveFailed);
    if (saveFailed) return sendError(500, "could not save to NVS; nothing changed");
    if (r != RepoError::Ok) return sendError(400, repoErrorText(r));
    app_.noteSetupActivity();
    sendOk();
}

void WebConfigServer::handleSelection() {
    JsonDocument doc;
    if (!readJson(&doc)) return;
    if (!doc["id"].is<const char*>()) return sendError(400, "id must be a string");
    bool saveFailed = false;
    const RepoError r = app_.selectEvent(doc["id"].as<const char*>(), true, &saveFailed);
    if (saveFailed) return sendError(500, "could not save the selection");
    int code;
    const char* msg = repoStatus(r, code);
    if (r != RepoError::Ok) return sendError(code, msg);
    app_.noteSetupActivity();
    sendOk();
}

// ---------------------------------------------------------------- settings

void WebConfigServer::handleSettingsGet() {
    JsonDocument doc;
    deserializeJson(doc, encodeSettings(app_.settings()));
    doc.remove("schemaVersion");
    JsonObject caps = doc["capabilities"].to<JsonObject>();
    caps["sound"] = app_.caps().speaker;
    caps["vibration"] = app_.caps().vibration;
    caps["rtc"] = app_.caps().rtc;
    std::string out;
    serializeJson(doc, out);
    sendJson(200, out);
}

void WebConfigServer::handleSettingsPut() {
    JsonDocument doc;
    if (!readJson(&doc)) return;
    DeviceSettings s = app_.settings();
    if (!doc["brightness"].isNull()) {
        if (!doc["brightness"].is<int>()) return sendError(400, "brightness must be an integer");
        const int b = doc["brightness"];
        if (b < 5 || b > 100) return sendError(400, "brightness must be 5..100");
        s.brightness = uint8_t(b);
    }
    if (!doc["dimAfterSec"].isNull()) {
        if (!doc["dimAfterSec"].is<int>()) return sendError(400, "dimAfterSec must be an integer");
        const int d = doc["dimAfterSec"];
        if (d < 0 || d > 3600) return sendError(400, "dimAfterSec must be 0..3600");
        s.dimAfterSec = uint16_t(d);
    }
    auto flag = [&](const char* key, bool& field, bool supported) {
        if (doc[key].isNull()) return true;
        if (!doc[key].is<bool>()) return false;
        field = supported ? doc[key].as<bool>() : false;
        return true;
    };
    if (!flag("soundEnabled", s.sound, app_.caps().speaker) ||
        !flag("vibrationEnabled", s.vibration, app_.caps().vibration) ||
        !flag("ntpEnabled", s.ntpEnabled, true) || !flag("keepAwakeOnUsb", s.keepAwakeOnUsb, true))
        return sendError(400, "flags must be booleans");
    if (!doc["timezone"].isNull() && !parseTz(doc["timezone"] | "", s.displayTz))
        return sendError(400, "timezone must be Asia/Tokyo or UTC");
    if (!doc["ntpServer"].isNull()) {
        const char* srv = doc["ntpServer"] | "";
        if (!validNtpServer(srv)) return sendError(400, "invalid NTP server host name");
        std::strncpy(s.ntpServer, srv, sizeof s.ntpServer - 1);
        s.ntpServer[sizeof s.ntpServer - 1] = 0;
    }
    // RTC interpretation: only through a confirmed preview (spec 2.14).
    bool changeMode = false;
    RtcMode mode = app_.settings().rtcMode;
    if (!doc["rtcInterpretation"].isNull()) {
        if (!parseRtcMode(doc["rtcInterpretation"] | "", mode))
            return sendError(400, "rtcInterpretation must be UTC or LOCAL_JST");
        changeMode = mode != app_.settings().rtcMode;
        if (changeMode) {
            const char* tok = doc["previewToken"] | "";
            const bool valid = !previewToken_.empty() && previewToken_ == tok &&
                               app_.monoNow() < previewExpires_ &&
                               previewGen_ == app_.clock().generation() &&
                               previewMode_ == uint8_t(mode) &&
                               previewFromMode_ == uint8_t(app_.settings().rtcMode);
            if (!valid) return sendError(409, "RTC preview expired or the clock changed; preview again");
        }
    }
    if (!app_.saveSettings(s)) return sendError(500, "could not save settings; nothing changed");
    bool followed = true;
    if (changeMode) {
        previewToken_.clear();
        if (!app_.saveRtcMode(mode, &followed))
            return sendError(500, "settings saved, but the RTC mode could not be saved (unchanged)");
    }
    app_.noteSetupActivity();
    // clockFollowed=false: the RTC failed the trust checks; the running clock was kept.
    sendJson(200, followed ? "{\"ok\":true,\"clockFollowed\":true}"
                           : "{\"ok\":true,\"clockFollowed\":false}");
}

void WebConfigServer::handleRtcPreview() {
    JsonDocument doc;
    if (!readJson(&doc)) return;
    RtcMode mode;
    if (!parseRtcMode(doc["mode"] | "", mode)) return sendError(400, "mode must be UTC or LOCAL_JST");
    ClockService& clk = app_.clock();
    const Tz tz = app_.settings().displayTz;
    JsonDocument res;
    char d[16], t[12];
    if (clk.valid()) {
        const Civil c = utcToLocal(clk.nowUtc(), tz);
        formatDate(c, d, sizeof d);
        formatTime(c, t, sizeof t);
        res["before"] = std::string(d) + " " + t;
    } else {
        res["before"] = nullptr;
    }
    int64_t after;
    if (clk.previewRtc(mode, after)) {
        const Civil c = utcToLocal(after, tz);
        formatDate(c, d, sizeof d);
        formatTime(c, t, sizeof t);
        res["after"] = std::string(d) + " " + t;
    } else {
        res["after"] = "UNAVAILABLE";
    }
    res["timezone"] = tzShort(tz);
    previewToken_ = randomHex(12);
    previewExpires_ = app_.monoNow() + 60 * 1000;
    previewGen_ = clk.generation();
    previewMode_ = uint8_t(mode);
    previewFromMode_ = uint8_t(app_.settings().rtcMode);
    res["previewToken"] = previewToken_;
    res["expiresInSec"] = 60;
    app_.noteSetupActivity();
    std::string out;
    serializeJson(res, out);
    sendJson(200, out);
}

// ---------------------------------------------------------------- wifi

void WebConfigServer::handleWifiGet() {
    JsonDocument doc;
    doc["ssid"] = app_.net().staSsid();
    doc["passwordConfigured"] = app_.net().passwordConfigured();
    doc["state"] = staStateText(app_.net().staState());
    doc["ip"] = app_.net().staIp();
    std::string out;
    serializeJson(doc, out);
    sendJson(200, out);
}

void WebConfigServer::handleWifiPut() {
    JsonDocument doc;
    if (!readJson(&doc)) return;
    if (!doc["ssid"].is<const char*>()) return sendError(400, "ssid must be a string");
    const std::string ssid = doc["ssid"].as<const char*>();
    if (ssid.size() > 32) return sendError(400, "ssid is longer than 32 bytes");
    std::string pw;
    const std::string* pwPtr = nullptr;  // omitted: keep the stored password
    if (!doc["password"].isNull()) {
        if (!doc["password"].is<const char*>()) return sendError(400, "password must be a string");
        pw = doc["password"].as<const char*>();
        if (!pw.empty() && (pw.size() < 8 || pw.size() > 63))
            return sendError(400, "WPA2 password must be 8..63 characters");
        for (char c : pw)
            if (c < 0x20 || c > 0x7E) return sendError(400, "password must be printable ASCII");
        pwPtr = &pw;
    }
    if (!app_.saveWifi(ssid, pwPtr)) return sendError(500, "could not save Wi-Fi settings");
    app_.noteSetupActivity();
    // Connection result is reported asynchronously through /api/status.
    sendJson(200, "{\"ok\":true,\"connecting\":true}");
}

// ---------------------------------------------------------------- time

void WebConfigServer::handleTime() {
    JsonDocument doc;
    if (!readJson(&doc)) return;
    if (!doc["epochMs"].is<int64_t>()) return sendError(400, "epochMs must be an integer");
    const int64_t ms = doc["epochMs"].as<int64_t>();
    const char* src = doc["source"] | "";
    ClockSource cs;
    if (std::strcmp(src, "MANUAL") == 0) cs = ClockSource::Manual;
    else if (std::strcmp(src, "BROWSER") == 0) cs = ClockSource::Browser;
    else return sendError(400, "source must be MANUAL or BROWSER");
    if (ms < 0) return sendError(400, "epochMs out of range");
    // ClockService keeps floor(epochMs / 1000) as the UTC second.
    const SetResult r = app_.setClockUtcMs(ms, cs);
    if (r == SetResult::Invalid) return sendError(400, "time outside 2000-2099 UTC");
    app_.noteSetupActivity();
    JsonDocument res;
    res["ok"] = true;
    res["nowUtc"] = app_.clock().nowUtc();
    res["rtcWriteStatus"] = rtcWriteStatusText(app_.clock().rtcWriteStatus());
    res["partial"] = r == SetResult::RtcWriteFailed;  // clock set, RTC not
    std::string out;
    serializeJson(res, out);
    sendJson(200, out);
}

void WebConfigServer::handleNtp() {
    const NtpScheduler::Manual m = app_.requestNtpSync();
    app_.noteSetupActivity();
    JsonDocument res;
    switch (m) {
        case NtpScheduler::Manual::Started: res["result"] = "STARTED"; break;
        case NtpScheduler::Manual::Scheduled: res["result"] = "SCHEDULED"; break;
        case NtpScheduler::Manual::Disabled: res["result"] = "DISABLED"; break;
        case NtpScheduler::Manual::Offline: res["result"] = "OFFLINE"; break;
    }
    res["state"] = ntpStateText(app_.ntpState());
    std::string out;
    serializeJson(res, out);
    const bool accepted = m == NtpScheduler::Manual::Started || m == NtpScheduler::Manual::Scheduled;
    sendJson(accepted ? 202 : 409, out);
}

void WebConfigServer::handleActivity() {
    app_.noteSetupActivity();
    sendOk();
}

void WebConfigServer::handleExit() {
    sendOk();
    exitAt_ = app_.monoNow() + 400;  // let the response leave before the AP goes down
}

}  // namespace cd
