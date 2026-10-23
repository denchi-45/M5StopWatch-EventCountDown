#include "TextUtil.h"
#include "../core/Utf8.h"

namespace cd {

namespace {
bool hasGlyph(const lgfx::IFont* font, uint32_t cp) {
    if (cp > 0xFFFF) return false;
    lgfx::FontMetrics m;
    font->getDefaultMetric(&m);
    return font->updateFontMetric(&m, uint16_t(cp));
}

void appendUtf8(std::string& out, uint32_t cp) {
    if (cp < 0x80) out += char(cp);
    else if (cp < 0x800) { out += char(0xC0 | cp >> 6); out += char(0x80 | (cp & 0x3F)); }
    else { out += char(0xE0 | cp >> 12); out += char(0x80 | (cp >> 6 & 0x3F)); out += char(0x80 | (cp & 0x3F)); }
}
}  // namespace

std::string fontSafe(const std::string& s, const lgfx::IFont* font) {
    const uint32_t placeholder = hasGlyph(font, 0x25A1) ? 0x25A1 : uint32_t('?');  // white square
    std::string out;
    out.reserve(s.size());
    for (size_t i = 0; i < s.size();) {
        uint32_t cp;
        const size_t n = utf8Decode(s.data(), s.size(), i, cp);
        if (n == 0) { out += '?'; ++i; continue; }
        if (cp == ' ' || hasGlyph(font, cp)) out.append(s, i, n);
        else appendUtf8(out, placeholder);
        i += n;
    }
    return out;
}

std::string ellipsize(Painter& p, const std::string& s, const lgfx::IFont* font, float size,
                      float maxW) {
    if (p.textWidth(s.c_str(), font, size) <= maxW) return s;
    const char* dots = hasGlyph(font, 0x2026) ? "\xE2\x80\xA6" : "...";
    // Code-point boundaries.
    std::string cut = s;
    while (!cut.empty()) {
        size_t i = cut.size() - 1;
        while (i > 0 && (uint8_t(cut[i]) & 0xC0) == 0x80) --i;
        cut.erase(i);
        while (!cut.empty() && cut.back() == ' ') cut.pop_back();
        const std::string t = cut + dots;
        if (p.textWidth(t.c_str(), font, size) <= maxW) return t;
    }
    return dots;
}

}  // namespace cd
