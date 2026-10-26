#include "HudFont.h"
#include <cctype>

namespace cd {

namespace {
// Each polyline is a run of "xy" digit pairs (x 0..4, y 0..6, y down);
// polylines are separated by spaces.
const char* glyph(char c) {
    switch (std::toupper(static_cast<unsigned char>(c))) {
        case 'A': return "060110304146 0343";
        case 'B': return "0006 003041423303 3344453606";
        case 'C': return "4130100105163645";
        case 'D': return "00304145360600";
        case 'E': return "40000646 0333";
        case 'F': return "400006 0333";
        case 'G': return "41301001051636454323";
        case 'H': return "0006 4046 0343";
        case 'I': return "1030 2026 1636";
        case 'J': return "404536160504";
        case 'K': return "0006 401303 1346";
        case 'L': return "000646";
        case 'M': return "0600224046";
        case 'N': return "06004640";
        case 'O': case '0': return "103041453616050110";
        case 'P': return "06003041423303";
        case 'Q': return "103041453616050110 2446";
        case 'R': return "06003041423303 2346";
        case 'S': return "413010010213334445361605";
        case 'T': return "0040 2026";
        case 'U': return "000516364540";
        case 'V': return "0003264340";
        case 'W': return "0006244640";
        case 'X': return "0046 4006";
        case 'Y': return "002340 2326";
        case 'Z': return "00400646";
        case '1': return "112026 1636";
        case '2': return "01103041420646";
        case '3': return "01103041423313 334445361605";
        case '4': return "36300444";
        case '5': return "4000033344453606";
        case '6': return "30100105163645443303";
        case '7': return "00404116";
        case '8': return "103041423313020110 1304051636454433";
        case '9': return "43130201103041453616";
        case '-': return "1343";
        case '+': return "1343 2224";
        case '/': return "0640";
        case ':': return "2121 2525";
        case '.': return "2626";
        case '!': return "2024 2626";
        case '<': return "301336";
        case '>': return "103316";
        default: return nullptr;
    }
}

float advance(char c, const HudStyle& st) {
    if (c == ' ') return 3.0f;
    if (c == ':' || c == '.' || c == '!') return 2.0f + st.tracking;
    if (c == 'I' || c == 'i' || c == '1') return 4.0f + st.tracking;
    return 4.0f + st.tracking;
}

void strokeGlyph(Painter& p, const char* g, float x, float cy, float unit, float slant,
                 float width, Color col) {
    const float top = cy - 3 * unit;
    Pt prev{0, 0};
    bool havePrev = false;
    for (const char* q = g; *q;) {
        if (*q == ' ') { havePrev = false; ++q; continue; }
        const int gx = q[0] - '0', gy = q[1] - '0';
        q += 2;
        const Pt pt{x + gx * unit + (6 - gy) * unit * slant, top + gy * unit};
        if (havePrev) p.line(prev.x, prev.y, pt.x, pt.y, width, col);
        else if (*q == ' ' || *q == 0) p.line(pt.x, pt.y, pt.x, pt.y, width, col);  // dot
        prev = pt;
        havePrev = true;
    }
}
}  // namespace

float HudFont::width(const char* s, const HudStyle& st) {
    const float unit = st.height / 6.0f;
    float w = 0;
    for (const char* c = s; *c; ++c) w += advance(*c, st);
    if (*s) w -= st.tracking;
    return w * unit + 6 * unit * st.slant;
}

void HudFont::draw(Painter& p, const char* s, float x, float cy, const HudStyle& st) {
    const float unit = st.height / 6.0f;
    const float stroke = st.stroke > 0 ? st.stroke : st.height * 0.11f;
    // Layer by layer over the whole string so a glow never covers a neighbour's body.
    struct Layer { float w; Color c; };
    Layer layers[4];
    int n = 0;
    if (st.glow) {
        layers[n++] = {stroke + st.height * 0.17f, pal::glow1};
        layers[n++] = {stroke + st.height * 0.08f, pal::glow2};
    }
    layers[n++] = {stroke, st.body};
    layers[n++] = {stroke * 0.38f, st.core};
    for (int l = 0; l < n; ++l) {
        float cx = x;
        for (const char* c = s; *c; ++c) {
            if (const char* g = glyph(*c)) strokeGlyph(p, g, cx, cy, unit, st.slant, layers[l].w, layers[l].c);
            cx += advance(*c, st) * unit;
        }
    }
}

void HudFont::drawCentered(Painter& p, const char* s, float cx, float cy, const HudStyle& st) {
    draw(p, s, cx - width(s, st) / 2, cy, st);
}

}  // namespace cd
