#include "JsonCodec.h"

namespace cd {

void eventToJson(const Event& e, JsonObject o) {
    o["id"] = e.id;
    o["name"] = e.name;
    o["startUtc"] = e.startUtc;
    o["targetUtc"] = e.targetUtc;
    o["timezone"] = tzId(e.tz);
    o["enabled"] = e.enabled;
    if (e.hasHandled) o["handledTargetUtc"] = e.handledTargetUtc;
    else o["handledTargetUtc"] = nullptr;
}

std::string encodeEvents(const EventRepository& repo) {
    JsonDocument doc;
    doc["schemaVersion"] = kSchemaVersion;
    if (repo.selectedId().empty()) doc["selectedEventId"] = nullptr;
    else doc["selectedEventId"] = repo.selectedId();
    JsonArray arr = doc["events"].to<JsonArray>();
    for (const auto& e : repo.events()) eventToJson(e, arr.add<JsonObject>());
    std::string out;
    serializeJson(doc, out);
    return out;
}

namespace {

bool readEventV3(JsonObjectConst o, Event& e, std::string& error) {
    if (!o["id"].is<const char*>() || !o["name"].is<const char*>() ||
        !o["startUtc"].is<int64_t>() || !o["targetUtc"].is<int64_t>() ||
        !o["enabled"].is<bool>()) {
        error = "event field missing or of wrong type";
        return false;
    }
    e.id = o["id"].as<const char*>();
    e.name = o["name"].as<const char*>();
    e.startUtc = o["startUtc"].as<int64_t>();
    e.targetUtc = o["targetUtc"].as<int64_t>();
    e.enabled = o["enabled"].as<bool>();
    if (!parseTz(o["timezone"] | "Asia/Tokyo", e.tz)) {
        error = "unsupported event timezone";
        return false;
    }
    e.hasHandled = o["handledTargetUtc"].is<int64_t>();
    e.handledTargetUtc = e.hasHandled ? o["handledTargetUtc"].as<int64_t>() : 0;
    return true;
}

bool validStored(const Event& e, std::string& error) {
    EventInput in{e.name, e.startUtc, e.targetUtc, e.tz, e.enabled}, norm;
    const RepoError err = EventRepository::validate(in, norm, &error);
    if (err != RepoError::Ok) {
        if (error.empty()) error = repoErrorText(err);
        return false;
    }
    if (e.id.empty() || e.id.size() > 32) {
        error = "bad event id";
        return false;
    }
    return true;
}

// v2: {schemaVersion:2, timezone, selectedEventId, events:[{id,name,startLocal,
// targetLocal,enabled}]} (local times) or already-UTC startUtc/targetUtc.
bool readEventV2(JsonObjectConst o, const char* globalTz, Event& e, std::string& error) {
    if (!o["id"].is<const char*>() || !o["name"].is<const char*>()) {
        error = "v2 event without id/name";
        return false;
    }
    e.id = o["id"].as<const char*>();
    e.name = o["name"].as<const char*>();
    e.enabled = o["enabled"] | true;
    const char* tz = o["timezone"].is<const char*>() ? o["timezone"].as<const char*>() : globalTz;
    if (!tz || !parseTz(tz, e.tz)) {
        error = "v2 data has no supported timezone";
        return false;
    }
    if (o["startUtc"].is<int64_t>() && o["targetUtc"].is<int64_t>()) {
        // Already UTC: never shift a second time.
        e.startUtc = o["startUtc"].as<int64_t>();
        e.targetUtc = o["targetUtc"].as<int64_t>();
    } else {
        Civil s, t;
        if (!parseLocalIso(o["startLocal"] | "", s) || !parseLocalIso(o["targetLocal"] | "", t) ||
            !localToUtc(s, e.tz, e.startUtc) || !localToUtc(t, e.tz, e.targetUtc)) {
            error = "v2 event has an invalid local date";
            return false;
        }
    }
    e.hasHandled = false;  // 5.2 start-up rule applies once the clock is valid
    return true;
}

}  // namespace

LoadResult decodeEvents(const std::string& json, std::vector<Event>& out, std::string& selected,
                        std::string& error) {
    out.clear();
    selected.clear();
    if (json.empty()) return LoadResult::Empty;
    JsonDocument doc;
    if (deserializeJson(doc, json)) {
        error = "stored events are not valid JSON";
        return LoadResult::Corrupt;
    }
    const int version = doc["schemaVersion"] | 0;
    JsonArrayConst arr = doc["events"].as<JsonArrayConst>();
    if (arr.isNull() || arr.size() > kMaxEvents) {
        error = "stored events array missing or too long";
        return LoadResult::Corrupt;
    }
    if (version != 2 && version != kSchemaVersion) {
        error = "unknown schemaVersion";
        return LoadResult::Corrupt;
    }
    std::vector<Event> events;
    for (JsonObjectConst o : arr) {
        Event e;
        const bool ok = version == 2 ? readEventV2(o, doc["timezone"] | (const char*)nullptr, e, error)
                                     : readEventV3(o, e, error);
        if (!ok || !validStored(e, error)) return LoadResult::Corrupt;
        for (const auto& prev : events)
            if (prev.id == e.id) {
                error = "duplicate event id";
                return LoadResult::Corrupt;
            }
        events.push_back(e);
    }
    out = std::move(events);
    if (doc["selectedEventId"].is<const char*>()) selected = doc["selectedEventId"].as<const char*>();
    return version == 2 ? LoadResult::Migrated : LoadResult::Ok;
}

std::string encodeSettings(const DeviceSettings& s) {
    JsonDocument doc;
    doc["schemaVersion"] = kSchemaVersion;
    doc["brightness"] = s.brightness;
    doc["dimAfterSec"] = s.dimAfterSec;
    doc["soundEnabled"] = s.sound;
    doc["vibrationEnabled"] = s.vibration;
    doc["keepAwakeOnUsb"] = s.keepAwakeOnUsb;
    doc["timezone"] = tzId(s.displayTz);
    doc["rtcInterpretation"] = rtcModeId(s.rtcMode);
    doc["ntpEnabled"] = s.ntpEnabled;
    doc["ntpServer"] = s.ntpServer;
    std::string out;
    serializeJson(doc, out);
    return out;
}

bool decodeSettings(const std::string& json, DeviceSettings& out) {
    JsonDocument doc;
    if (json.empty() || deserializeJson(doc, json)) return false;
    DeviceSettings s;
    const int b = doc["brightness"] | int(s.brightness);
    s.brightness = uint8_t(b < 5 ? 5 : b > 100 ? 100 : b);
    const int d = doc["dimAfterSec"] | int(s.dimAfterSec);
    s.dimAfterSec = uint16_t(d < 0 ? 0 : d > 3600 ? 3600 : d);
    s.sound = doc["soundEnabled"] | s.sound;
    s.vibration = doc["vibrationEnabled"] | s.vibration;
    s.keepAwakeOnUsb = doc["keepAwakeOnUsb"] | s.keepAwakeOnUsb;
    parseTz(doc["timezone"] | "Asia/Tokyo", s.displayTz);
    parseRtcMode(doc["rtcInterpretation"] | "UTC", s.rtcMode);
    s.ntpEnabled = doc["ntpEnabled"] | s.ntpEnabled;
    const char* server = doc["ntpServer"] | "pool.ntp.org";
    if (validNtpServer(server)) {
        std::strncpy(s.ntpServer, server, sizeof s.ntpServer - 1);
        s.ntpServer[sizeof s.ntpServer - 1] = 0;
    }
    out = s;
    return true;
}

}  // namespace cd
