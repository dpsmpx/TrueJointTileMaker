#include "core/palette.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <map>
#include <sstream>

namespace tjtm::pal {

namespace {

struct Hsvd {
    double h = 0, s = 0, v = 0;  // h 0..360, s и v 0..1
};

/// HSV с дробями: целые проценты оригинала для рядов и сдвигов грубы —
/// соседние ступени ряда сливались бы в один цвет.
Hsvd toHsv(Color c) {
    const double r = c.r / 255.0;
    const double g = c.g / 255.0;
    const double b = c.b / 255.0;
    const double mx = std::max({r, g, b});
    const double mn = std::min({r, g, b});
    const double d = mx - mn;
    Hsvd o;
    o.v = mx;
    o.s = mx > 0 ? d / mx : 0;
    if (d > 0) {
        if (mx == r) {
            o.h = 60.0 * std::fmod((g - b) / d, 6.0);
        } else if (mx == g) {
            o.h = 60.0 * ((b - r) / d + 2.0);
        } else {
            o.h = 60.0 * ((r - g) / d + 4.0);
        }
    }
    if (o.h < 0) o.h += 360.0;
    return o;
}

Color fromHsv(Hsvd x, int alpha) {
    double h = std::fmod(x.h, 360.0);
    if (h < 0) h += 360.0;
    const double s = std::clamp(x.s, 0.0, 1.0);
    const double v = std::clamp(x.v, 0.0, 1.0);
    const double c = v * s;
    const double hp = h / 60.0;
    const double xx = c * (1 - std::fabs(std::fmod(hp, 2.0) - 1));
    double r = 0;
    double g = 0;
    double b = 0;
    switch (static_cast<int>(hp) % 6) {
        case 0:
            r = c;
            g = xx;
            break;
        case 1:
            r = xx;
            g = c;
            break;
        case 2:
            g = c;
            b = xx;
            break;
        case 3:
            g = xx;
            b = c;
            break;
        case 4:
            r = xx;
            b = c;
            break;
        default:
            r = c;
            b = xx;
            break;
    }
    const double m = v - c;
    auto to8 = [](double t) {
        return static_cast<int>(std::lround(std::clamp(t, 0.0, 1.0) * 255.0));
    };
    return argb(alpha, to8(r + m), to8(g + m), to8(b + m));
}

double luma(Color c) {
    // Светлота по Rec. 601: глаз видит зелёный ярче синего.
    return 0.299 * c.r + 0.587 * c.g + 0.114 * c.b;
}

std::string trim(const std::string& s) {
    std::size_t a = 0;
    std::size_t b = s.size();
    while (a < b && (s[a] == ' ' || s[a] == '\t' || s[a] == '\r'))
        ++a;
    while (b > a && (s[b - 1] == ' ' || s[b - 1] == '\t' || s[b - 1] == '\r'))
        --b;
    return s.substr(a, b - a);
}

std::vector<std::string> lines(const std::string& text) {
    std::vector<std::string> out;
    std::istringstream in(text);
    std::string l;
    while (std::getline(in, l))
        out.push_back(trim(l));
    // Метка BOM в начале — не часть первой строки.
    if (!out.empty() && out[0].starts_with("\xEF\xBB\xBF")) out[0] = out[0].substr(3);
    return out;
}

}  // namespace

std::string sortKeyName(int key) {
    switch (key) {
        case BY_HUE: return "тону";
        case BY_SAT: return "насыщенности";
        case BY_VAL: return "яркости";
        case BY_LUMA: return "светлоте";
        case BY_RED: return "красному";
        case BY_GREEN: return "зелёному";
        case BY_BLUE: return "синему";
        default: return "прозрачности";
    }
}

std::vector<int> sortOrder(const std::vector<Color>& p, int key, int from, int to) {
    from = std::max(0, from);
    to = std::min(static_cast<int>(p.size()) - 1, to);
    std::vector<int> order;
    for (int k = from; k <= to; ++k)
        order.push_back(k);
    auto value = [&](int k) -> double {
        const Color c = p[static_cast<std::size_t>(k)];
        switch (key) {
            case BY_HUE: {
                // Серые — в конец: у них нет тона, и среди цветных они
                // разрывали бы радугу.
                const Hsvd h = toHsv(c);
                return h.s < 0.04 ? 1000.0 + h.v : h.h;
            }
            case BY_SAT: return toHsv(c).s;
            case BY_VAL: return toHsv(c).v;
            case BY_LUMA: return luma(c);
            case BY_RED: return c.r;
            case BY_GREEN: return c.g;
            case BY_BLUE: return c.b;
            default: return c.a;
        }
    };
    std::stable_sort(order.begin(), order.end(), [&](int a, int b) { return value(a) < value(b); });
    return order;
}

std::vector<Color> ramp(Color a, Color b, int steps, int hueShift) {
    std::vector<Color> out;
    if (steps < 1) return out;
    if (steps == 1) return {a};
    const Hsvd ha = toHsv(a);
    const Hsvd hb = toHsv(b);
    // Тон идёт коротким путём по кругу: от красного к пурпурному через
    // красный, а не через всю радугу.
    double dh = hb.h - ha.h;
    if (dh > 180) dh -= 360;
    if (dh < -180) dh += 360;
    // Серый конец не тянет тон: берётся тон цветного конца.
    double h0 = ha.h;
    if (ha.s < 0.04) h0 = hb.h, dh = 0;
    if (hb.s < 0.04 && ha.s >= 0.04) dh = 0;
    for (int k = 0; k < steps; ++k) {
        const double t = static_cast<double>(k) / (steps - 1);
        Hsvd x;
        // Изгиб: сдвиг наибольший на середине, на концах ноль — концы ряда
        // остаются теми цветами, что выбрали.
        x.h = h0 + dh * t + hueShift * 4.0 * t * (1 - t);
        x.s = ha.s + (hb.s - ha.s) * t;
        x.v = ha.v + (hb.v - ha.v) * t;
        const int alpha = static_cast<int>(std::lround(a.a + (b.a - a.a) * t));
        out.push_back(k == 0 ? a : (k == steps - 1 ? b : fromHsv(x, alpha)));
    }
    return out;
}

Color adjustHSV(Color c, int dh, int ds, int dv) {
    Hsvd x = toHsv(c);
    x.h += dh;
    x.s += ds / 100.0;
    x.v += dv / 100.0;
    return fromHsv(x, c.a);
}

std::vector<Color> extract(const Grid<Color>& g, int maxColors) {
    // Счёт цветов в порядке появления: палитра читается так же, как
    // картинка, сверху вниз, а не вперемешку по значению.
    std::vector<Color> order;
    std::map<std::uint32_t, int> count;
    for (const Color c : g.d) {
        if (c.a == 0) continue;
        const std::uint32_t k = c.argb();
        if (count[k]++ == 0) order.push_back(c);
    }
    if (static_cast<int>(order.size()) <= maxColors || maxColors < 1) return order;

    // Медианный разрез: коробку с наибольшим разбросом делим пополам по её
    // самому длинному каналу, пока коробок не станет maxColors.
    struct Box {
        std::vector<Color> cols;
    };
    std::vector<Box> boxes{{order}};
    auto range = [](const Box& b, int ch) {
        int lo = 255;
        int hi = 0;
        for (const Color c : b.cols) {
            const int v = ch == 0 ? c.r : (ch == 1 ? c.g : c.b);
            lo = std::min(lo, v);
            hi = std::max(hi, v);
        }
        return hi - lo;
    };
    while (static_cast<int>(boxes.size()) < maxColors) {
        int bestBox = -1;
        int bestCh = 0;
        int bestRange = 0;
        for (std::size_t i = 0; i < boxes.size(); ++i) {
            if (boxes[i].cols.size() < 2) continue;
            for (int ch = 0; ch < 3; ++ch) {
                const int r = range(boxes[i], ch);
                if (r > bestRange) {
                    bestRange = r;
                    bestBox = static_cast<int>(i);
                    bestCh = ch;
                }
            }
        }
        if (bestBox < 0) break;
        Box& b = boxes[static_cast<std::size_t>(bestBox)];
        auto chan = [bestCh](Color c) { return bestCh == 0 ? c.r : (bestCh == 1 ? c.g : c.b); };
        std::stable_sort(b.cols.begin(), b.cols.end(),
                         [&](Color x, Color y) { return chan(x) < chan(y); });
        // Медиана по числу точек, а не по числу разных цветов: частый цвет
        // заслуживает своей коробки.
        long long total = 0;
        for (const Color c : b.cols)
            total += count[c.argb()];
        long long acc = 0;
        std::size_t cut = 1;
        for (std::size_t i = 0; i + 1 < b.cols.size(); ++i) {
            acc += count[b.cols[i].argb()];
            cut = i + 1;
            if (acc * 2 >= total) break;
        }
        Box hi{std::vector<Color>(b.cols.begin() + static_cast<std::ptrdiff_t>(cut), b.cols.end())};
        b.cols.resize(cut);
        boxes.push_back(std::move(hi));
    }
    std::vector<Color> out;
    for (const Box& b : boxes) {
        long long n = 0;
        long long r = 0;
        long long gg = 0;
        long long bb = 0;
        long long a = 0;
        for (const Color c : b.cols) {
            const long long w = count[c.argb()];
            n += w;
            r += c.r * w;
            gg += c.g * w;
            bb += c.b * w;
            a += c.a * w;
        }
        if (n == 0) continue;
        out.push_back(argb(static_cast<int>(a / n), static_cast<int>(r / n),
                           static_cast<int>(gg / n), static_cast<int>(bb / n)));
    }
    return out;
}

std::vector<Color> fromImage(const Grid<Color>& g) {
    if (g.empty()) return {};
    if (g.w * g.h <= MAX_PALETTE) return g.d;
    return extract(g, MAX_PALETTE);
}

int nearest(const std::vector<Color>& p, Color c) {
    int best = -1;
    int bestD = 0;
    for (std::size_t k = 0; k < p.size(); ++k) {
        const int dr = c.r - p[k].r;
        const int dg = c.g - p[k].g;
        const int db = c.b - p[k].b;
        const int d = dr * dr + dg * dg + db * db;
        if (best < 0 || d < bestD) {
            best = static_cast<int>(k);
            bestD = d;
        }
    }
    return best;
}

// --- файлы палитр -------------------------------------------------------------

bool parseGpl(const std::string& text, std::vector<Color>& out, std::string& error) {
    const auto ls = lines(text);
    if (ls.empty() || ls[0] != "GIMP Palette") {
        error = "это не палитра GIMP: нет строки «GIMP Palette»";
        return false;
    }
    out.clear();
    for (std::size_t i = 1; i < ls.size(); ++i) {
        const std::string& l = ls[i];
        if (l.empty() || l[0] == '#' || l.starts_with("Name:") || l.starts_with("Columns:"))
            continue;
        std::istringstream in(l);
        int r = 0;
        int g = 0;
        int b = 0;
        if (!(in >> r >> g >> b)) continue;
        out.push_back(
            argb(255, std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255)));
    }
    if (out.empty()) {
        error = "в палитре нет ни одного цвета";
        return false;
    }
    if (static_cast<int>(out.size()) > MAX_PALETTE) out.resize(MAX_PALETTE);
    return true;
}

std::string writeGpl(const std::vector<Color>& p, const std::string& name) {
    std::string s = "GIMP Palette\nName: " + name + "\nColumns: 8\n#\n";
    char buf[64];
    for (const Color c : p) {
        std::snprintf(buf, sizeof buf, "%3d %3d %3d\t#%02X%02X%02X\n", c.r, c.g, c.b, c.r, c.g,
                      c.b);
        s += buf;
    }
    return s;
}

bool parseHex(const std::string& text, std::vector<Color>& out, std::string& error) {
    out.clear();
    for (std::string l : lines(text)) {
        if (l.empty() || l[0] == ';' || (l.size() > 1 && l[0] == '/' && l[1] == '/')) continue;
        if (l[0] == '#') l = l.substr(1);
        if (l.size() != 6 && l.size() != 8) {
            error = "строка «" + l + "» — не цвет RRGGBB";
            return false;
        }
        unsigned v = 0;
        for (const char ch : l) {
            int d = -1;
            if (ch >= '0' && ch <= '9') d = ch - '0';
            if (ch >= 'a' && ch <= 'f') d = ch - 'a' + 10;
            if (ch >= 'A' && ch <= 'F') d = ch - 'A' + 10;
            if (d < 0) {
                error = "строка «" + l + "» — не цвет RRGGBB";
                return false;
            }
            v = v * 16 + static_cast<unsigned>(d);
        }
        out.push_back(l.size() == 6 ? Color::fromArgb(0xFF000000u | v) : Color::fromArgb(v));
    }
    if (out.empty()) {
        error = "в файле нет ни одного цвета";
        return false;
    }
    if (static_cast<int>(out.size()) > MAX_PALETTE) out.resize(MAX_PALETTE);
    return true;
}

std::string writeHex(const std::vector<Color>& p) {
    std::string s;
    char buf[16];
    for (const Color c : p) {
        std::snprintf(buf, sizeof buf, "%02x%02x%02x\n", c.r, c.g, c.b);
        s += buf;
    }
    return s;
}

bool parseJasc(const std::string& text, std::vector<Color>& out, std::string& error) {
    const auto ls = lines(text);
    if (ls.size() < 3 || ls[0] != "JASC-PAL") {
        error = "это не палитра JASC-PAL";
        return false;
    }
    const int n = std::atoi(ls[2].c_str());
    out.clear();
    for (std::size_t i = 3; i < ls.size() && static_cast<int>(out.size()) < n; ++i) {
        std::istringstream in(ls[i]);
        int r = 0;
        int g = 0;
        int b = 0;
        if (!(in >> r >> g >> b)) continue;
        out.push_back(
            argb(255, std::clamp(r, 0, 255), std::clamp(g, 0, 255), std::clamp(b, 0, 255)));
    }
    if (out.empty()) {
        error = "в палитре нет ни одного цвета";
        return false;
    }
    if (static_cast<int>(out.size()) > MAX_PALETTE) out.resize(MAX_PALETTE);
    return true;
}

std::string writeJasc(const std::vector<Color>& p) {
    // Перевод строки CRLF: формат родом из Windows, и иные читалки ждут его.
    std::string s = "JASC-PAL\r\n0100\r\n" + std::to_string(p.size()) + "\r\n";
    for (const Color c : p)
        s += std::to_string(c.r) + " " + std::to_string(c.g) + " " + std::to_string(c.b) + "\r\n";
    return s;
}

}  // namespace tjtm::pal
