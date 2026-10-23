#pragma once
// Six-field date/time draft for SET TIME (spec 2.8, 4).
// Values never leave their range; a day past the month end is clamped.
#include "TimeMath.h"

namespace cd {

class DateEditor {
public:
    enum Field : int { Year = 0, Month, Day, Hour, Minute, Second, kFieldCount };

    void load(const Civil& c) { draft_ = c; clampDay(); }
    const Civil& draft() const { return draft_; }
    int value(int field) const;
    // +1 / -1 within range (no wrap-around). Returns true if the value changed.
    bool adjust(int field, int delta);
    bool valid() const { return validCivil(draft_); }

private:
    Civil draft_;
    void clampDay();
};

}  // namespace cd
