#pragma once
// JSON encoding of the persisted event blob (schemaVersion 3) and settings.
// Events and the selected id live in ONE blob so deletion/disable and the
// selection fallback are written as a single unit (spec 5.3).
#include <ArduinoJson.h>
#include <string>
#include <vector>
#include "../core/DeviceSettings.h"
#include "../core/EventRepository.h"

namespace cd {

constexpr int kSchemaVersion = 3;

enum class LoadResult : uint8_t {
    Ok,         // v3 data loaded
    Empty,      // nothing stored
    Migrated,   // legacy v2 converted; caller must persist as v3
    Corrupt,    // unreadable / failed validation: keep the raw data, show error
};

std::string encodeEvents(const EventRepository& repo);
// v2 local times are converted with the event's or the document's "timezone";
// v2 data without a supported timezone is Corrupt and left untouched.
LoadResult decodeEvents(const std::string& json, std::vector<Event>& out, std::string& selected,
                        std::string& error);

std::string encodeSettings(const DeviceSettings& s);
bool decodeSettings(const std::string& json, DeviceSettings& out);

// API representation of one event (UTC seconds + timezone id).
void eventToJson(const Event& e, JsonObject o);

}  // namespace cd
