#include "EventRepository.h"
#include <algorithm>
#include <cstdio>
#include <set>

namespace cd {

const char* repoErrorText(RepoError e) {
    switch (e) {
        case RepoError::Ok: return "ok";
        case RepoError::BadName: return "invalid name";
        case RepoError::OutOfRange: return "date out of range (2000-2099 UTC)";
        case RepoError::StartNotBeforeTarget: return "countdown start must be before the event";
        case RepoError::Full: return "maximum 10 events";
        case RepoError::NotFound: return "event not found";
        case RepoError::Disabled: return "event is disabled";
        case RepoError::BadOrder: return "order must list every event id exactly once";
    }
    return "error";
}

size_t EventRepository::enabledCount() const {
    return size_t(std::count_if(events_.begin(), events_.end(),
                                [](const Event& e) { return e.enabled; }));
}

int EventRepository::indexOf(const std::string& id) const {
    for (size_t i = 0; i < events_.size(); ++i)
        if (events_[i].id == id) return int(i);
    return -1;
}

const Event* EventRepository::find(const std::string& id) const {
    const int i = indexOf(id);
    return i < 0 ? nullptr : &events_[size_t(i)];
}

const Event* EventRepository::selected() const {
    const Event* e = find(selected_);
    return (e && e->enabled) ? e : nullptr;
}

RepoError EventRepository::validate(const EventInput& in, EventInput& out, std::string* detail) {
    out = in;
    const NameError ne = validateEventName(in.name, out.name);
    if (ne != NameError::Ok) {
        if (detail) *detail = nameErrorText(ne);
        return RepoError::BadName;
    }
    if (!inUtcRange(in.startUtc) || !inUtcRange(in.targetUtc)) return RepoError::OutOfRange;
    if (in.startUtc >= in.targetUtc) return RepoError::StartNotBeforeTarget;
    return RepoError::Ok;
}

RepoError EventRepository::add(const EventInput& in, const RandomFn& rnd, std::string* outId,
                               std::string* detail) {
    if (events_.size() >= kMaxEvents) return RepoError::Full;
    EventInput v;
    const RepoError err = validate(in, v, detail);
    if (err != RepoError::Ok) return err;
    Event e;
    char buf[16];
    do {
        std::snprintf(buf, sizeof buf, "evt-%08x", unsigned(rnd()));
    } while (indexOf(buf) >= 0);
    e.id = buf;
    e.name = v.name;
    e.startUtc = v.startUtc;
    e.targetUtc = v.targetUtc;
    e.tz = v.tz;
    e.enabled = v.enabled;
    events_.push_back(e);
    if (outId) *outId = e.id;
    ensureSelection();
    return RepoError::Ok;
}

RepoError EventRepository::update(const std::string& id, const EventInput& in,
                                  std::string* detail) {
    const int i = indexOf(id);
    if (i < 0) return RepoError::NotFound;
    EventInput v;
    const RepoError err = validate(in, v, detail);
    if (err != RepoError::Ok) return err;
    Event& e = events_[size_t(i)];
    if (e.targetUtc != v.targetUtc) {
        e.hasHandled = false;
        e.handledTargetUtc = 0;
    }
    e.name = v.name;
    e.startUtc = v.startUtc;
    e.targetUtc = v.targetUtc;
    e.tz = v.tz;
    e.enabled = v.enabled;
    ensureSelection();
    return RepoError::Ok;
}

RepoError EventRepository::remove(const std::string& id) {
    const int i = indexOf(id);
    if (i < 0) return RepoError::NotFound;
    events_.erase(events_.begin() + i);
    ensureSelection();
    return RepoError::Ok;
}

RepoError EventRepository::reorder(const std::vector<std::string>& ids) {
    if (ids.size() != events_.size()) return RepoError::BadOrder;
    std::set<std::string> seen;
    std::vector<Event> next;
    next.reserve(ids.size());
    for (const auto& id : ids) {
        const int i = indexOf(id);
        if (i < 0 || !seen.insert(id).second) return RepoError::BadOrder;
        next.push_back(events_[size_t(i)]);
    }
    events_.swap(next);
    return RepoError::Ok;
}

RepoError EventRepository::select(const std::string& id) {
    const Event* e = find(id);
    if (!e) return RepoError::NotFound;
    if (!e->enabled) return RepoError::Disabled;
    selected_ = id;
    return RepoError::Ok;
}

bool EventRepository::step(int dir) {
    if (enabledCount() == 0) return false;
    const int n = int(events_.size());
    int i = indexOf(selected_);
    if (i < 0) i = dir > 0 ? n - 1 : 0;
    for (int k = 1; k <= n; ++k) {
        const int j = ((i + dir * k) % n + n) % n;
        if (events_[size_t(j)].enabled) {
            const bool changed = events_[size_t(j)].id != selected_;
            selected_ = events_[size_t(j)].id;
            return changed;
        }
    }
    return false;
}
bool EventRepository::selectPrev() { return step(-1); }
bool EventRepository::selectNext() { return step(+1); }

bool EventRepository::ensureSelection() {
    if (selected()) return false;
    const std::string before = selected_;
    selected_.clear();
    for (const auto& e : events_)
        if (e.enabled) { selected_ = e.id; break; }
    return selected_ != before;
}

bool EventRepository::processArrivals(int64_t prevUtc, int64_t nowUtc, bool normalProgress,
                                      bool& changed) {
    changed = false;
    bool notify = false;
    const Event* sel = selected();
    for (auto& e : events_) {
        if (nowUtc < e.targetUtc || e.handled()) continue;
        // Reached and not yet recorded.
        if (normalProgress && sel == &e && prevUtc < e.targetUtc) notify = true;
        e.hasHandled = true;
        e.handledTargetUtc = e.targetUtc;
        changed = true;
    }
    return notify;
}

void EventRepository::assign(std::vector<Event> events, const std::string& selectedId) {
    events_ = std::move(events);
    if (events_.size() > kMaxEvents) events_.resize(kMaxEvents);
    selected_ = selectedId;
    ensureSelection();
}

}  // namespace cd
