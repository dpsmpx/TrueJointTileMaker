#include "core/scheme.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <deque>
#include <sstream>

#include "core/text.h"

namespace tjtm::theme {

namespace {

struct Lin {
    double r, g, b;
};
struct OkLab {
    double l, a, b;
};

double toLinear(int c8) {
    const double c = c8 / 255.0;
    return c <= 0.04045 ? c / 12.92 : std::pow((c + 0.055) / 1.055, 2.4);
}

int fromLinear(double c) {
    c = std::clamp(c, 0.0, 1.0);
    const double s = c <= 0.0031308 ? c * 12.92 : 1.055 * std::pow(c, 1.0 / 2.4) - 0.055;
    return static_cast<int>(std::lround(s * 255.0));
}

OkLab toOkLab(Color c) {
    const double r = toLinear(c.r);
    const double g = toLinear(c.g);
    const double b = toLinear(c.b);
    const double l = std::cbrt(0.4122214708 * r + 0.5363325363 * g + 0.0514459929 * b);
    const double m = std::cbrt(0.2119034982 * r + 0.6806995451 * g + 0.1073969566 * b);
    const double s = std::cbrt(0.0883024619 * r + 0.2817188376 * g + 0.6299787005 * b);
    return {0.2104542553 * l + 0.7936177850 * m - 0.0040720468 * s,
            1.9779984951 * l - 2.4285922050 * m + 0.4505937099 * s,
            0.0259040371 * l + 0.7827717662 * m - 0.8086757660 * s};
}

Lin fromOkLab(const OkLab& o) {
    const double l = std::pow(o.l + 0.3963377774 * o.a + 0.2158037573 * o.b, 3);
    const double m = std::pow(o.l - 0.1055613458 * o.a - 0.0638541728 * o.b, 3);
    const double s = std::pow(o.l - 0.0894841775 * o.a - 1.2914855480 * o.b, 3);
    return {4.0767416621 * l - 3.3077115913 * m + 0.2309699292 * s,
            -1.2684380046 * l + 2.6097574011 * m - 0.3413193965 * s,
            -0.0041960863 * l - 0.7034186147 * m + 1.7076147010 * s};
}

/// L* CIELAB по яркости Y (белая точка D65, Y белого — 1).
double lstar(const Lin& c) {
    const double y = 0.2126729 * c.r + 0.7151522 * c.g + 0.0721750 * c.b;
    return y > 216.0 / 24389.0 ? 116.0 * std::cbrt(y) - 16.0 : y * 24389.0 / 27.0;
}

bool inGamut(const Lin& c) {
    constexpr double e = 1e-4;
    return c.r >= -e && c.r <= 1 + e && c.g >= -e && c.g <= 1 + e && c.b >= -e && c.b <= 1 + e;
}

/// Цвет OKLCH (hue в радианах, цветность c) с тоном tone: светлота OKLab
/// подбирается делением пополам — L* растёт с ней монотонно.
Lin atTone(double hue, double c, double tone) {
    double lo = 0;
    double hi = 1;
    Lin got{};
    for (int i = 0; i < 40; ++i) {
        const double mid = (lo + hi) / 2;
        got = fromOkLab({mid, c * std::cos(hue), c * std::sin(hue)});
        if (lstar(got) < tone) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return got;
}

Color fromLin(const Lin& c) {
    return rgb(fromLinear(c.r), fromLinear(c.g), fromLinear(c.b));
}

/// Цвет тона tone с оттенком hue и цветностью не больше cmax: что не
/// влезает в sRGB, теряет цветность, а не тон.
Color hueTone(double hue, double cmax, double tone) {
    if (tone <= 0) return rgb(0, 0, 0);
    if (tone >= 100) return rgb(255, 255, 255);
    if (inGamut(atTone(hue, cmax, tone))) return fromLin(atTone(hue, cmax, tone));
    double lo = 0;
    double hi = cmax;
    for (int i = 0; i < 24; ++i) {
        const double mid = (lo + hi) / 2;
        if (inGamut(atTone(hue, mid, tone))) {
            lo = mid;
        } else {
            hi = mid;
        }
    }
    return fromLin(atTone(hue, lo, tone));
}

double hueOf(Color c) {
    const OkLab o = toOkLab(c);
    return std::atan2(o.b, o.a);
}

double chromaOf(Color c) {
    const OkLab o = toOkLab(c);
    return std::hypot(o.a, o.b);
}

constexpr std::array kAccents = {
    Accent{"Синий", kDefaultSeed},
    Accent{"Бирюзовый", rgb(0x03, 0x69, 0x6A)},
    Accent{"Зелёный", rgb(0x03, 0x6D, 0x30)},
    Accent{"Янтарный", rgb(0x85, 0x54, 0x04)},
    Accent{"Оранжевый", rgb(0x9F, 0x41, 0x03)},
    Accent{"Красный", rgb(0xB9, 0x1D, 0x1F)},
    Accent{"Розовый", rgb(0xA9, 0x2C, 0x69)},
    Accent{"Фиолетовый", rgb(0x74, 0x45, 0xB4)},
    Accent{"Графит", rgb(0x51, 0x60, 0x6F)},
};

constexpr std::array<const char*, 6> kNames = {
    "светлая", "тёмная", "светлая контрастная", "тёмная контрастная", "как в системе", "классика"};

/// Цвет ошибки — красный, при любом акценте.
constexpr Color kErrorSeed = rgb(0xB1, 0x2C, 0x28);

/// Имя темы из файла должно жить, пока жива тема: Theme хранит указатель.
const char* intern(const std::string& s) {
    static std::deque<std::string> pool;
    for (const std::string& p : pool) {
        if (p == s) return p.c_str();
    }
    pool.push_back(s);
    return pool.back().c_str();
}

}  // namespace

std::span<const Accent> accents() {
    return kAccents;
}

std::span<const char* const> builtinThemeNames() {
    return kNames;
}

Color toneOf(Color seed, double tone, double chroma) {
    // Светлые тона контейнеров у насыщенных акцентов (бирюзовый,
    // зелёный) спорили бы с картинкой: их цветность ограничена.
    double c = chromaOf(seed) * chroma;
    if (tone >= 80) c = std::min(c, 0.10);
    return hueTone(hueOf(seed), c, tone);
}

Color neutralTone(Color seed, double tone, double c) {
    return hueTone(hueOf(seed), c, tone);
}

double contrastRatio(Color a, Color b) {
    auto lum = [](Color c) {
        return 0.2126 * toLinear(c.r) + 0.7152 * toLinear(c.g) + 0.0722 * toLinear(c.b);
    };
    const double la = lum(a);
    const double lb = lum(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

Theme schemeTheme(Color seed, Variant v) {
    const bool dark = v == Variant::Dark || v == Variant::DarkContrast;
    const bool contrast = v == Variant::LightContrast || v == Variant::DarkContrast;
    if (seed == kDefaultSeed && !contrast) return dark ? darkTheme() : lightTheme();
    Theme t = dark ? darkTheme() : lightTheme();
    auto N = [&](double tone) { return neutralTone(seed, tone, 0.006); };
    auto NV = [&](double tone) { return neutralTone(seed, tone, 0.012); };
    auto P = [&](double tone) { return toneOf(seed, tone); };
    auto E = [&](double tone) { return toneOf(kErrorSeed, tone); };
    if (!dark) {
        t.window = N(98);
        t.back = N(100);
        t.panel = N(96);
        t.face = N(92);
        t.text = N(10);
        t.textDim = contrast ? N(15) : NV(30);
        t.outline = contrast ? NV(25) : NV(50);
        t.border = contrast ? NV(40) : NV(80);
        t.line = contrast ? NV(70) : NV(89);
        t.accent = contrast ? P(30) : P(40);
        t.onAccent = P(100);
        // Контрастная: выбранное залито акцентом, подпись — белая.
        t.accentContainer = contrast ? P(30) : P(90);
        t.onAccentContainer = contrast ? P(100) : P(10);
        t.inverseSurface = N(20);
        t.inverseOnSurface = N(95);
        t.warn = E(40);
        t.field = t.back;
        t.fieldSelection = P(90);
    } else {
        t.window = N(6);
        t.back = N(12);
        t.panel = N(10);
        t.face = N(17);
        t.field = N(4);
        t.text = contrast ? N(98) : N(90);
        t.textDim = contrast ? NV(92) : NV(80);
        t.outline = contrast ? NV(80) : NV(60);
        t.border = contrast ? NV(60) : NV(30);
        t.line = contrast ? NV(40) : N(22);
        t.accent = contrast ? P(90) : P(80);
        t.onAccent = contrast ? P(10) : P(20);
        t.accentContainer = contrast ? P(85) : P(30);
        t.onAccentContainer = contrast ? P(5) : P(90);
        t.inverseSurface = N(90);
        t.inverseOnSurface = N(20);
        t.warn = E(80);
        t.fieldSelection = P(30);
        t.curveChannel[0] = t.text;
        t.mapLabelBack = argb(210, t.face.r, t.face.g, t.face.b);
        t.mapLabelText = t.text;
    }
    t.hover = mix(t.back, t.text, 20);
    t.shadow = t.window;
    t.noteBack = argb(220, t.window.r, t.window.g, t.window.b);
    t.curveBack = dark ? t.field : t.back;
    t.name = contrast ? (dark ? kNames[3] : kNames[2]) : (dark ? kNames[1] : kNames[0]);
    return t;
}

bool parseHexColor(const std::string& s0, Color& out) {
    std::string s = netTrim(s0);
    if (s.starts_with("#")) s.erase(0, 1);
    if (s.size() != 6 && s.size() != 8) return false;
    unsigned v[4] = {0, 0, 0, 255};
    for (std::size_t i = 0; i < s.size() / 2; ++i) {
        unsigned x = 0;
        for (std::size_t k = 0; k < 2; ++k) {
            const char ch = s[i * 2 + k];
            unsigned d = 0;
            if (ch >= '0' && ch <= '9') {
                d = static_cast<unsigned>(ch - '0');
            } else if (ch >= 'a' && ch <= 'f') {
                d = static_cast<unsigned>(ch - 'a' + 10);
            } else if (ch >= 'A' && ch <= 'F') {
                d = static_cast<unsigned>(ch - 'A' + 10);
            } else {
                return false;
            }
            x = x * 16 + d;
        }
        v[i] = x;
    }
    out = argb(static_cast<int>(v[3]), static_cast<int>(v[0]), static_cast<int>(v[1]),
               static_cast<int>(v[2]));
    return true;
}

std::string hexColor(Color c) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string s = "#";
    for (const int v : {c.r, c.g, c.b}) {
        s += kHex[(v >> 4) & 15];
        s += kHex[v & 15];
    }
    return s;
}

namespace {

/// Роли файла темы → поля Theme; nullptr — нет такой роли.
Color Theme::*roleField(const std::string& role) {
    static const std::pair<const char*, Color Theme::*> kRoles[] = {
        {"surface", &Theme::window},
        {"surface_container_lowest", &Theme::field},
        {"surface_container_low", &Theme::panel},
        {"surface_container", &Theme::back},
        {"surface_container_high", &Theme::face},
        {"on_surface", &Theme::text},
        {"on_surface_variant", &Theme::textDim},
        {"outline", &Theme::outline},
        {"outline_variant", &Theme::border},
        {"divider", &Theme::line},
        {"primary", &Theme::accent},
        {"on_primary", &Theme::onAccent},
        {"primary_container", &Theme::accentContainer},
        {"on_primary_container", &Theme::onAccentContainer},
        {"error", &Theme::warn},
        {"inverse_surface", &Theme::inverseSurface},
        {"inverse_on_surface", &Theme::inverseOnSurface},
        {"canvas.backdrop", &Theme::canvas},
        {"canvas.checker.a", &Theme::checkA},
        {"canvas.checker.b", &Theme::checkB},
        {"guide", &Theme::guide},
        {"guide.center", &Theme::guideCenter},
        {"symmetry.axis", &Theme::symAxis},
        {"overview.viewport", &Theme::viewport},
        {"grid.major", &Theme::majorGrid},
    };
    for (const auto& [name, field] : kRoles) {
        if (role == name) return field;
    }
    return nullptr;
}

}  // namespace

bool parseThemeFile(const std::string& text, ThemeFile& out, std::vector<std::string>& problems) {
    std::istringstream in(text);
    std::string line;
    int n = 0;
    while (std::getline(in, line)) {
        ++n;
        if (!line.empty() && line.back() == '\r') line.pop_back();
        const std::string s = netTrim(line);
        if (s.empty() || s[0] == '#' || s[0] == ';') continue;
        const std::size_t eq = s.find('=');
        if (eq == std::string::npos) {
            problems.push_back("строка " + std::to_string(n) + ": нет «=»");
            continue;
        }
        const std::string key = netToLower(netTrim(s.substr(0, eq)));
        std::string val = netTrim(s.substr(eq + 1));
        // Комментарий бывает и после значения: «seed = #7445B4   # акцент».
        // Сам цвет тоже начинается с «#», поэтому ищется пробел с «#» за
        // первым знаком значения.
        for (std::size_t h = 1; h < val.size(); ++h) {
            if (val[h] == '#' && (val[h - 1] == ' ' || val[h - 1] == '\t')) {
                val = netTrim(val.substr(0, h));
                break;
            }
        }
        if (key == "name") {
            out.name = val;
        } else if (key == "base") {
            out.base = netToLower(val);
        } else if (key == "seed") {
            Color c;
            if (parseHexColor(val, c)) {
                out.seed = c;
                out.hasSeed = true;
            } else {
                problems.push_back("строка " + std::to_string(n) + ": seed — не цвет");
            }
        } else if (key == "shape.corners") {
            out.values.emplace_back(key, netToLower(val));
        } else if (key.starts_with("color.")) {
            Color c;
            if (roleField(key.substr(6)) == nullptr) {
                problems.push_back("строка " + std::to_string(n) + ": нет роли " + key.substr(6));
            } else if (!parseHexColor(val, c)) {
                problems.push_back("строка " + std::to_string(n) + ": " + key + " — не цвет");
            } else {
                out.values.emplace_back(key.substr(6), val);
            }
        } else {
            problems.push_back("строка " + std::to_string(n) + ": неизвестный ключ " + key);
        }
    }
    return !out.name.empty();
}

namespace {

/// Встроенная тема по имени; неизвестное имя — светлая.
Theme builtinTheme(const std::string& name, Color seed, bool systemDark) {
    if (name == "классика" || name == "classic") return classicTheme();
    if (name == "тёмная" || name == "dark") return schemeTheme(seed, Variant::Dark);
    if (name == "светлая контрастная") return schemeTheme(seed, Variant::LightContrast);
    if (name == "тёмная контрастная") return schemeTheme(seed, Variant::DarkContrast);
    if (name == "как в системе")
        return schemeTheme(seed, systemDark ? Variant::Dark : Variant::Light);
    return schemeTheme(seed, Variant::Light);
}

}  // namespace

Theme buildTheme(const std::string& name, Color seed, bool systemDark,
                 const std::vector<ThemeFile>& files) {
    for (const ThemeFile& f : files) {
        if (netToLower(f.name) != netToLower(name)) continue;
        // База — встроенная тема; файл не наследует от файла, чтобы не
        // ходить по кругу.
        Theme t = builtinTheme(f.base, f.hasSeed ? f.seed : seed, systemDark);
        for (const auto& [key, val] : f.values) {
            if (key == "shape.corners") {
                t.radius = val == "square" ? 0 : val == "pixel" ? 2 : 6;
                continue;
            }
            Color c;
            if (Color Theme::*field = roleField(key); field != nullptr && parseHexColor(val, c)) {
                t.*field = c;
                if (key == "primary_container") t.fieldSelection = c;
            }
        }
        if (t.modern) {
            t.hover = mix(t.back, t.text, 20);
            t.shadow = t.window;
        }
        t.name = intern(f.name);
        return t;
    }
    return builtinTheme(name, seed, systemDark);
}

}  // namespace tjtm::theme
