#pragma once
// Event list, selection and arrival-notification bookkeeping (spec 5, 5.2, 5.3).
// Pure logic: persistence is done by the caller (SettingsStore) after a mutation.
#include <cstdint>
#include <functional>
#include <string>
#include <vector>
#include "TimeMath.h"
#include "Utf8.h"

namespace cd {

constexpr size_t kMaxEvents = 10;

struct Event {
    std::string id;    // immutable, generated on the device
    std::string name;  // UTF-8
    int64_t startUtc = 0;
    int64_t targetUtc = 0;
    Tz tz = Tz::AsiaTokyo;  // for editing only; the instants are UTC
    bool enabled = true;
    bool hasHandled = false;  // handledTargetUtc != null
    int64_t handledTargetUtc = 0;

    bool handled() const { return hasHandled && handledTargetUtc == targetUtc; }
};

struct EventInput {
    std::string name;
    int64_t startUtc = 0;
    int64_t targetUtc = 0;
    Tz tz = Tz::AsiaTokyo;
    bool enabled = true;
};

enum class RepoError : uint8_t {
    Ok, BadName, OutOfRange, StartNotBeforeTarget, Full, NotFound, Disabled, BadOrder,
};
const char* repoErrorText(RepoError e);

class EventRepository {
public:
    using RandomFn = std::function<uint32_t()>;

    const std::vector<Event>& events() const { return events_; }
    const std::string& selectedId() const { return selected_; }
    size_t enabledCount() const;
    int indexOf(const std::string& id) const;
    const Event* find(const std::string& id) const;
    // The selected event when it exists and is enabled, else nullptr.
    const Event* selected() const;

    // Validates the input. name is trimmed into `normalized`.
    static RepoError validate(const EventInput& in, EventInput& normalized, std::string* detail);

    RepoError add(const EventInput& in, const RandomFn& rnd, std::string* outId,
                  std::string* detail = nullptr);
    // Target edits reset the notification record (5.2); name/tz/enabled do not.
    RepoError update(const std::string& id, const EventInput& in, std::string* detail = nullptr);
    RepoError remove(const std::string& id);
    RepoError reorder(const std::vector<std::string>& ids);
    RepoError select(const std::string& id);
    // KEY A / KEY B: cycle among enabled events in list order. True if changed.
    bool selectPrev();
    bool selectNext();
    // Falls back to the first enabled event (or none). True if the id changed.
    bool ensureSelection();

    // Arrival processing for one clock step from prevUtc to nowUtc.
    // normalProgress == false means the clock was set/jumped (or this is the
    // first valid reading): nothing notifies, every reached target is marked.
    // Returns true when the selected event must notify. `changed` is set when
    // any handled record changed (the caller persists before notifying).
    bool processArrivals(int64_t prevUtc, int64_t nowUtc, bool normalProgress, bool& changed);

    // Replace everything (load / migration). Selection is then normalised.
    void assign(std::vector<Event> events, const std::string& selectedId);

private:
    std::vector<Event> events_;
    std::string selected_;
    bool step(int dir);
};

}  // namespace cd
