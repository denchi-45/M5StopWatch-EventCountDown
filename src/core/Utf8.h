#pragma once
// Strict UTF-8 helpers for event names (spec 3.2: max 48 code points / 192 bytes).
#include <cstddef>
#include <cstdint>
#include <string>

namespace cd {

constexpr size_t kNameMaxCodepoints = 48;
constexpr size_t kNameMaxBytes = 192;

// Decodes one code point at s[i]. Returns bytes consumed (0 = invalid: overlong,
// surrogate, > U+10FFFF, truncated or stray continuation byte).
size_t utf8Decode(const char* s, size_t len, size_t i, uint32_t& cp);

// True when the whole string is well-formed UTF-8. count receives code points.
bool utf8Validate(const std::string& s, size_t& count);

enum class NameError : uint8_t { Ok, Empty, TooLong, InvalidUtf8, ControlChar };
// Validates and trims ASCII whitespace at both ends. Whitespace-only is Empty.
NameError validateEventName(const std::string& in, std::string& trimmed);
const char* nameErrorText(NameError e);

}  // namespace cd
