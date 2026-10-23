#include "Utf8.h"

namespace cd {

size_t utf8Decode(const char* s, size_t len, size_t i, uint32_t& cp) {
    const uint8_t c = uint8_t(s[i]);
    size_t n;
    uint32_t min;
    if (c < 0x80) { cp = c; return 1; }
    if ((c & 0xE0) == 0xC0) { n = 2; cp = c & 0x1F; min = 0x80; }
    else if ((c & 0xF0) == 0xE0) { n = 3; cp = c & 0x0F; min = 0x800; }
    else if ((c & 0xF8) == 0xF0) { n = 4; cp = c & 0x07; min = 0x10000; }
    else return 0;
    if (i + n > len) return 0;
    for (size_t k = 1; k < n; ++k) {
        const uint8_t cc = uint8_t(s[i + k]);
        if ((cc & 0xC0) != 0x80) return 0;
        cp = (cp << 6) | (cc & 0x3F);
    }
    if (cp < min || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) return 0;
    return n;
}

bool utf8Validate(const std::string& s, size_t& count) {
    count = 0;
    for (size_t i = 0; i < s.size();) {
        uint32_t cp;
        const size_t n = utf8Decode(s.data(), s.size(), i, cp);
        if (n == 0) return false;
        i += n;
        ++count;
    }
    return true;
}

NameError validateEventName(const std::string& in, std::string& trimmed) {
    size_t count;
    if (!utf8Validate(in, count)) return NameError::InvalidUtf8;
    auto ws = [](char c) { return c == ' ' || c == '\t' || c == '\r' || c == '\n'; };
    size_t b = 0, e = in.size();
    while (b < e && ws(in[b])) ++b;
    while (e > b && ws(in[e - 1])) --e;
    trimmed = in.substr(b, e - b);
    if (trimmed.empty()) return NameError::Empty;
    // Control characters (C0, DEL, C1) would break rendering and logs.
    for (size_t i = 0; i < trimmed.size();) {
        uint32_t cp;
        const size_t n = utf8Decode(trimmed.data(), trimmed.size(), i, cp);
        if (cp < 0x20 || cp == 0x7F || (cp >= 0x80 && cp < 0xA0)) return NameError::ControlChar;
        i += n;
    }
    utf8Validate(trimmed, count);
    if (count > kNameMaxCodepoints || trimmed.size() > kNameMaxBytes) return NameError::TooLong;
    return NameError::Ok;
}

const char* nameErrorText(NameError e) {
    switch (e) {
        case NameError::Ok: return "ok";
        case NameError::Empty: return "name is empty";
        case NameError::TooLong: return "name exceeds 48 characters / 192 bytes";
        case NameError::InvalidUtf8: return "name is not valid UTF-8";
        case NameError::ControlChar: return "name contains control characters";
    }
    return "invalid name";
}

}  // namespace cd
