#pragma once
// Event-name rendering helpers: glyphs missing from the font are replaced
// (spec 1.2) and long names are shortened with an ellipsis instead of pushing
// neighbouring elements (spec 1.3).
#include <string>
#include "Painter.h"

namespace cd {

// Replaces code points the font cannot draw (and everything above U+FFFF)
// with a placeholder the font does have.
std::string fontSafe(const std::string& utf8, const lgfx::IFont* font);
// Cuts at code-point boundaries and appends an ellipsis to fit maxW.
std::string ellipsize(Painter& p, const std::string& utf8, const lgfx::IFont* font, float size,
                      float maxW);
// ellipsize(fontSafe(name)) with a small cache, for names drawn on every frame.
const std::string& fittedName(Painter& p, const std::string& utf8, const lgfx::IFont* font,
                              float size, float maxW);

}  // namespace cd
