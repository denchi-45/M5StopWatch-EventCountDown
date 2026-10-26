#include "DateEditor.h"

namespace cd {

int DateEditor::value(int field) const {
    switch (field) {
        case Year: return draft_.year;
        case Month: return draft_.month;
        case Day: return draft_.day;
        case Hour: return draft_.hour;
        case Minute: return draft_.minute;
        case Second: return draft_.second;
    }
    return 0;
}

bool DateEditor::adjust(int field, int delta) {
    int* v = nullptr;
    int lo = 0, hi = 0;
    switch (field) {
        case Year: v = &draft_.year; lo = kMinYear; hi = kMaxYear; break;
        case Month: v = &draft_.month; lo = 1; hi = 12; break;
        case Day: v = &draft_.day; lo = 1; hi = daysInMonth(draft_.year, draft_.month); break;
        case Hour: v = &draft_.hour; lo = 0; hi = 23; break;
        case Minute: v = &draft_.minute; lo = 0; hi = 59; break;
        case Second: v = &draft_.second; lo = 0; hi = 59; break;
        default: return false;
    }
    int next = *v + delta;
    if (next < lo) next = lo;
    if (next > hi) next = hi;
    if (next == *v) return false;
    *v = next;
    clampDay();
    return true;
}

void DateEditor::clampDay() {
    if (draft_.year < kMinYear) draft_.year = kMinYear;
    if (draft_.year > kMaxYear) draft_.year = kMaxYear;
    if (draft_.month < 1) draft_.month = 1;
    if (draft_.month > 12) draft_.month = 12;
    const int dim = daysInMonth(draft_.year, draft_.month);
    if (draft_.day > dim) draft_.day = dim;
    if (draft_.day < 1) draft_.day = 1;
}

}  // namespace cd
