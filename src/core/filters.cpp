#include "core/filters.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "core/imageops.h"

namespace tjtm::fx {

namespace {

/// Цвет с домноженной прозрачностью, каналы — 0..255 в дробях.
struct P {
    double r = 0, g = 0, b = 0, a = 0;
    P& operator+=(const P& o) {
        r += o.r;
        g += o.g;
        b += o.b;
        a += o.a;
        return *this;
    }
    P operator*(double k) const { return {r * k, g * k, b * k, a * k}; }
};

P toP(Color c) {
    const double k = c.a / 255.0;
    return {c.r * k, c.g * k, c.b * k, static_cast<double>(c.a)};
}

std::uint8_t byteOf(double v) {
    return static_cast<std::uint8_t>(std::clamp(std::lround(v), 0L, 255L));
}

Color fromP(const P& p) {
    if (p.a < 0.5) return argb(0, 0, 0, 0);
    const double k = 255.0 / p.a;
    return Color(byteOf(p.a), byteOf(p.r * k), byteOf(p.g * k), byteOf(p.b * k));
}

enum Edge { EDGE_CLEAR, EDGE_CLAMP, EDGE_WRAP };

int wrapI(int v, int n) {
    return ((v % n) + n) % n;
}

/// Точка с правилом края: прозрачно, ближайшая точка края или по кругу.
P pixelAt(const std::vector<P>& d, int w, int h, int x, int y, Edge e) {
    if (x < 0 || y < 0 || x >= w || y >= h) {
        if (e == EDGE_CLEAR) return {};
        if (e == EDGE_WRAP) {
            x = wrapI(x, w);
            y = wrapI(y, h);
        } else {
            x = std::clamp(x, 0, w - 1);
            y = std::clamp(y, 0, h - 1);
        }
    }
    return d[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
             static_cast<std::size_t>(x)];
}

std::vector<P> toPs(const Grid<Color>& g) {
    std::vector<P> out(g.d.size());
    for (std::size_t k = 0; k < g.d.size(); ++k)
        out[k] = toP(g.d[k]);
    return out;
}

Grid<Color> fromPs(const std::vector<P>& d, int w, int h) {
    Grid<Color> out(w, h);
    for (std::size_t k = 0; k < out.d.size(); ++k)
        out.d[k] = fromP(d[k]);
    return out;
}

P bilinear(const std::vector<P>& d, int w, int h, double x, double y, Edge e) {
    const double fx = x - 0.5;
    const double fy = y - 0.5;
    const auto x0 = static_cast<int>(std::floor(fx));
    const auto y0 = static_cast<int>(std::floor(fy));
    const double tx = fx - x0;
    const double ty = fy - y0;
    P out = pixelAt(d, w, h, x0, y0, e) * ((1 - tx) * (1 - ty));
    out += pixelAt(d, w, h, x0 + 1, y0, e) * (tx * (1 - ty));
    out += pixelAt(d, w, h, x0, y0 + 1, e) * ((1 - tx) * ty);
    out += pixelAt(d, w, h, x0 + 1, y0 + 1, e) * (tx * ty);
    return out;
}

/// Свёртка ядром 3x3 по яркости с домноженной прозрачностью: прозрачное
/// считается чёрным, и край спрайта — тоже перепад.
template <class F>
Grid<Color> convolveLuma(const Grid<Color>& g, bool wrap, F&& f) {
    Grid<Color> out(g.w, g.h);
    if (g.empty()) return out;
    std::vector<double> lum(g.d.size());
    for (std::size_t k = 0; k < g.d.size(); ++k) {
        const Color c = g.d[k];
        lum[k] = (0.299 * c.r + 0.587 * c.g + 0.114 * c.b) * c.a / 255.0;
    }
    auto at = [&](int x, int y) {
        if (wrap) {
            x = wrapI(x, g.w);
            y = wrapI(y, g.h);
        } else {
            x = std::clamp(x, 0, g.w - 1);
            y = std::clamp(y, 0, g.h - 1);
        }
        return lum[static_cast<std::size_t>(y) * static_cast<std::size_t>(g.w) +
                   static_cast<std::size_t>(x)];
    };
    for (int y = 0; y < g.h; ++y) {
        for (int x = 0; x < g.w; ++x) {
            const Color src = g.at(x, y);
            if (src.a == 0) {
                out.at(x, y) = src;
                continue;
            }
            const std::uint8_t v = byteOf(f(at, x, y));
            out.at(x, y) = Color(src.a, v, v, v);
        }
    }
    return out;
}

template <class F>
Grid<Color> mapRgb(const Grid<Color>& g, F&& f) {
    Grid<Color> out = g;
    for (Color& c : out.d) {
        if (c.a != 0) c = f(c);
    }
    return out;
}

/// Хэш для шума: одинаковый на любой платформе и в любом порядке обхода.
std::uint32_t hash3(std::uint32_t x, std::uint32_t y, std::uint32_t z) {
    std::uint32_t h = x * 0x8DA6B343U ^ y * 0xD8163841U ^ z * 0xCB1AB31FU;
    h ^= h >> 16;
    h *= 0x7FEB352DU;
    h ^= h >> 15;
    h *= 0x846CA68BU;
    h ^= h >> 16;
    return h;
}

std::vector<double> gaussKernel(double radius) {
    const double sigma = std::max(0.3, radius / 2.0);
    const int n = std::max(1, static_cast<int>(std::ceil(radius * 1.5)));
    std::vector<double> k(static_cast<std::size_t>(2 * n + 1));
    double sum = 0;
    for (int i = -n; i <= n; ++i) {
        const double v = std::exp(-(i * i) / (2 * sigma * sigma));
        k[static_cast<std::size_t>(i + n)] = v;
        sum += v;
    }
    for (double& v : k)
        v /= sum;
    return k;
}

/// Размытие в домноженных цветах — общее для blur и sharpen.
std::vector<P> blurPs(const std::vector<P>& src, int w, int h, double radius, bool wrap) {
    if (radius <= 0 || w <= 0 || h <= 0) return src;
    const std::vector<double> k = gaussKernel(radius);
    const int n = static_cast<int>(k.size() / 2);
    const Edge e = wrap ? EDGE_WRAP : EDGE_CLAMP;
    std::vector<P> tmp(src.size());
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            P s;
            for (int i = -n; i <= n; ++i)
                s += pixelAt(src, w, h, x + i, y, e) * k[static_cast<std::size_t>(i + n)];
            tmp[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                static_cast<std::size_t>(x)] = s;
        }
    }
    std::vector<P> out(src.size());
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            P s;
            for (int i = -n; i <= n; ++i)
                s += pixelAt(tmp, w, h, x, y + i, e) * k[static_cast<std::size_t>(i + n)];
            out[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                static_cast<std::size_t>(x)] = s;
        }
    }
    return out;
}

/// Одна ось масштаба: уменьшение — средним по площади, увеличение —
/// билинейно с краем, продолженным наружу.
std::vector<P> resampleAxis(const std::vector<P>& src, int w, int h, int n, bool horizontal) {
    const int s = horizontal ? w : h;
    const int ow = horizontal ? n : w;
    const int oh = horizontal ? h : n;
    std::vector<P> out(static_cast<std::size_t>(ow) * static_cast<std::size_t>(oh));
    auto get = [&](int i, int line) {
        i = std::clamp(i, 0, s - 1);
        return horizontal ? src[static_cast<std::size_t>(line) * static_cast<std::size_t>(w) +
                                static_cast<std::size_t>(i)]
                          : src[static_cast<std::size_t>(i) * static_cast<std::size_t>(w) +
                                static_cast<std::size_t>(line)];
    };
    const int lines = horizontal ? h : w;
    const double scale = static_cast<double>(s) / n;
    for (int line = 0; line < lines; ++line) {
        for (int i = 0; i < n; ++i) {
            P acc;
            if (n < s) {
                const double a = i * scale;
                const double b = (i + 1) * scale;
                for (auto j = static_cast<int>(std::floor(a)); j < b; ++j) {
                    const double cover = std::min<double>(b, j + 1) - std::max<double>(a, j);
                    if (cover > 0) acc += get(j, line) * (cover / scale);
                }
            } else {
                const double c = (i + 0.5) * scale - 0.5;
                const auto j = static_cast<int>(std::floor(c));
                const double t = c - j;
                acc = get(j, line) * (1 - t);
                acc += get(j + 1, line) * t;
            }
            const std::size_t at =
                horizontal ? static_cast<std::size_t>(line) * static_cast<std::size_t>(ow) +
                                 static_cast<std::size_t>(i)
                           : static_cast<std::size_t>(i) * static_cast<std::size_t>(ow) +
                                 static_cast<std::size_t>(line);
            out[at] = acc;
        }
    }
    return out;
}

/// Решение линейной системы 8x8 методом Гаусса; false — вырожденная.
bool solve8(std::array<std::array<double, 9>, 8>& m, std::array<double, 8>& out) {
    for (int c = 0; c < 8; ++c) {
        int piv = c;
        for (int r = c + 1; r < 8; ++r) {
            if (std::fabs(m[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)]) >
                std::fabs(m[static_cast<std::size_t>(piv)][static_cast<std::size_t>(c)]))
                piv = r;
        }
        if (std::fabs(m[static_cast<std::size_t>(piv)][static_cast<std::size_t>(c)]) < 1e-12)
            return false;
        std::swap(m[static_cast<std::size_t>(c)], m[static_cast<std::size_t>(piv)]);
        for (int r = 0; r < 8; ++r) {
            if (r == c) continue;
            const double f = m[static_cast<std::size_t>(r)][static_cast<std::size_t>(c)] /
                             m[static_cast<std::size_t>(c)][static_cast<std::size_t>(c)];
            for (int k = c; k < 9; ++k)
                m[static_cast<std::size_t>(r)][static_cast<std::size_t>(k)] -=
                    f * m[static_cast<std::size_t>(c)][static_cast<std::size_t>(k)];
        }
    }
    for (int c = 0; c < 8; ++c)
        out[static_cast<std::size_t>(c)] =
            m[static_cast<std::size_t>(c)][8] /
            m[static_cast<std::size_t>(c)][static_cast<std::size_t>(c)];
    return true;
}

}  // namespace

// --- свёртки ------------------------------------------------------------------

Grid<Color> blur(const Grid<Color>& g, double radius, bool wrap) {
    if (g.empty() || radius <= 0) return g;
    return fromPs(blurPs(toPs(g), g.w, g.h, radius, wrap), g.w, g.h);
}

Grid<Color> sharpen(const Grid<Color>& g, int amountPct, double radius, bool wrap) {
    if (g.empty() || amountPct <= 0) return g;
    const std::vector<P> soft = blurPs(toPs(g), g.w, g.h, radius, wrap);
    const double k = amountPct / 100.0;
    Grid<Color> out = g;
    for (std::size_t i = 0; i < out.d.size(); ++i) {
        Color& c = out.d[i];
        if (c.a == 0 || soft[i].a < 0.5) continue;
        // Сравнение — с цветом размытой копии без домножения: иначе
        // прозрачный сосед затемнял бы край.
        const double inv = 255.0 / soft[i].a;
        c.r = byteOf(c.r + k * (c.r - soft[i].r * inv));
        c.g = byteOf(c.g + k * (c.g - soft[i].g * inv));
        c.b = byteOf(c.b + k * (c.b - soft[i].b * inv));
    }
    return out;
}

Grid<Color> edges(const Grid<Color>& g, bool wrap) {
    return convolveLuma(g, wrap, [](const auto& at, int x, int y) {
        const double gx = at(x + 1, y - 1) + 2 * at(x + 1, y) + at(x + 1, y + 1) -
                          at(x - 1, y - 1) - 2 * at(x - 1, y) - at(x - 1, y + 1);
        const double gy = at(x - 1, y + 1) + 2 * at(x, y + 1) + at(x + 1, y + 1) -
                          at(x - 1, y - 1) - 2 * at(x, y - 1) - at(x + 1, y - 1);
        return std::hypot(gx, gy) / 4.0;
    });
}

Grid<Color> emboss(const Grid<Color>& g, bool wrap) {
    return convolveLuma(g, wrap, [](const auto& at, int x, int y) {
        return 128.0 + at(x + 1, y + 1) + at(x + 1, y) + at(x, y + 1) - at(x - 1, y - 1) -
               at(x - 1, y) - at(x, y - 1);
    });
}

// --- точечные --------------------------------------------------------------------

Grid<Color> noise(const Grid<Color>& g, int amount, bool mono, std::uint32_t seed) {
    Grid<Color> out = g;
    if (amount <= 0) return out;
    const auto span = static_cast<std::uint32_t>(2 * amount + 1);
    for (int y = 0; y < g.h; ++y) {
        for (int x = 0; x < g.w; ++x) {
            Color& c = out.at(x, y);
            if (c.a == 0) continue;
            const auto ux = static_cast<std::uint32_t>(x);
            const auto uy = static_cast<std::uint32_t>(y);
            auto delta = [&](std::uint32_t ch) {
                return static_cast<int>(hash3(ux, uy, seed * 4 + ch) % span) - amount;
            };
            const int d0 = delta(0);
            c.r = byteOf(c.r + d0);
            c.g = byteOf(c.g + (mono ? d0 : delta(1)));
            c.b = byteOf(c.b + (mono ? d0 : delta(2)));
        }
    }
    return out;
}

Grid<Color> grayscale(const Grid<Color>& g) {
    return mapRgb(g, [](Color c) {
        const std::uint8_t v = byteOf((299 * c.r + 587 * c.g + 114 * c.b) / 1000.0);
        return Color(c.a, v, v, v);
    });
}

Grid<Color> sepia(const Grid<Color>& g) {
    return mapRgb(g, [](Color c) {
        return Color(c.a, byteOf(0.393 * c.r + 0.769 * c.g + 0.189 * c.b),
                     byteOf(0.349 * c.r + 0.686 * c.g + 0.168 * c.b),
                     byteOf(0.272 * c.r + 0.534 * c.g + 0.131 * c.b));
    });
}

Grid<Color> invertColors(const Grid<Color>& g) {
    return mapRgb(g, [](Color c) {
        return Color(c.a, static_cast<std::uint8_t>(255 - c.r),
                     static_cast<std::uint8_t>(255 - c.g), static_cast<std::uint8_t>(255 - c.b));
    });
}

Grid<Color> brightnessContrast(const Grid<Color>& g, int brightness, int contrast, int saturation) {
    const double add = std::clamp(brightness, -100, 100) * 1.275;
    // Контраст в квадрате: от серого (−100) до четырёхкратного (+100), ноль
    // — без изменений, шаг вблизи нуля мелкий.
    const double f = std::pow((100.0 + std::clamp(contrast, -100, 100)) / 100.0, 2.0);
    const double s = (100.0 + std::clamp(saturation, -100, 100)) / 100.0;
    return mapRgb(g, [&](Color c) {
        double ch[3] = {c.r + add, c.g + add, c.b + add};
        for (double& v : ch)
            v = (v - 127.5) * f + 127.5;
        const double l = 0.299 * ch[0] + 0.587 * ch[1] + 0.114 * ch[2];
        for (double& v : ch)
            v = l + (v - l) * s;
        return Color(c.a, byteOf(ch[0]), byteOf(ch[1]), byteOf(ch[2]));
    });
}

std::array<std::uint8_t, 256> levelsLut(const Levels& l) {
    std::array<std::uint8_t, 256> lut{};
    const int lo = std::clamp(l.inLo, 0, 254);
    const int hi = std::clamp(l.inHi, lo + 1, 255);
    const double gamma = std::clamp(l.gamma, 0.1, 10.0);
    for (int v = 0; v < 256; ++v) {
        double t = std::clamp(static_cast<double>(v - lo) / (hi - lo), 0.0, 1.0);
        t = std::pow(t, 1.0 / gamma);
        lut[static_cast<std::size_t>(v)] = byteOf(l.outLo + t * (l.outHi - l.outLo));
    }
    return lut;
}

Grid<Color> levels(const Grid<Color>& g, const Levels& l) {
    const auto lut = levelsLut(l);
    return mapRgb(g, [&](Color c) { return Color(c.a, lut[c.r], lut[c.g], lut[c.b]); });
}

std::array<std::uint8_t, 256> curveLut(const Curve& c) {
    std::array<std::uint8_t, 256> lut{};
    Curve p = c;
    std::sort(p.begin(), p.end());
    p.erase(std::unique(p.begin(), p.end(),
                        [](const auto& a, const auto& b) { return a.first == b.first; }),
            p.end());
    if (p.size() < 2) {
        for (int v = 0; v < 256; ++v)
            lut[static_cast<std::size_t>(v)] =
                p.empty() ? static_cast<std::uint8_t>(v) : byteOf(p[0].second);
        return lut;
    }
    // Монотонный кубический сплайн Фритча — Карлсона.
    const std::size_t n = p.size();
    std::vector<double> d(n - 1);
    std::vector<double> m(n);
    for (std::size_t k = 0; k + 1 < n; ++k)
        d[k] = static_cast<double>(p[k + 1].second - p[k].second) / (p[k + 1].first - p[k].first);
    m[0] = d[0];
    m[n - 1] = d[n - 2];
    for (std::size_t k = 1; k + 1 < n; ++k)
        m[k] = d[k - 1] * d[k] <= 0 ? 0.0 : (d[k - 1] + d[k]) / 2;
    for (std::size_t k = 0; k + 1 < n; ++k) {
        if (d[k] == 0) {
            m[k] = 0;
            m[k + 1] = 0;
            continue;
        }
        const double a = m[k] / d[k];
        const double b = m[k + 1] / d[k];
        const double s = a * a + b * b;
        if (s > 9) {
            const double t = 3 / std::sqrt(s);
            m[k] = t * a * d[k];
            m[k + 1] = t * b * d[k];
        }
    }
    for (int v = 0; v < 256; ++v) {
        double out = 0;
        if (v <= p.front().first) {
            out = p.front().second;
        } else if (v >= p.back().first) {
            out = p.back().second;
        } else {
            std::size_t k = 0;
            while (k + 2 < n && v > p[k + 1].first)
                ++k;
            const double hx = p[k + 1].first - p[k].first;
            const double t = (v - p[k].first) / hx;
            const double t2 = t * t;
            const double t3 = t2 * t;
            out = (2 * t3 - 3 * t2 + 1) * p[k].second + (t3 - 2 * t2 + t) * hx * m[k] +
                  (-2 * t3 + 3 * t2) * p[k + 1].second + (t3 - t2) * hx * m[k + 1];
        }
        lut[static_cast<std::size_t>(v)] = byteOf(out);
    }
    return lut;
}

Grid<Color> curves(const Grid<Color>& g, const Curve& all, const Curve& r, const Curve& gr,
                   const Curve& b) {
    const auto la = curveLut(all);
    const auto lr = curveLut(r);
    const auto lg = curveLut(gr);
    const auto lb = curveLut(b);
    return mapRgb(g, [&](Color c) { return Color(c.a, lr[la[c.r]], lg[la[c.g]], lb[la[c.b]]); });
}

std::array<int, 256> histogram(const Grid<Color>& g) {
    std::array<int, 256> h{};
    for (const Color c : g.d) {
        if (c.a == 0) continue;
        ++h[byteOf((299 * c.r + 587 * c.g + 114 * c.b) / 1000.0)];
    }
    return h;
}

// --- плавные преобразования --------------------------------------------------

Color sampleBilinear(const Grid<Color>& g, double x, double y, bool wrap) {
    if (g.empty()) return argb(0, 0, 0, 0);
    return fromP(bilinear(toPs(g), g.w, g.h, x, y, wrap ? EDGE_WRAP : EDGE_CLEAR));
}

Grid<Color> scaleSmooth(const Grid<Color>& g, int nw, int nh) {
    if (g.empty() || nw < 1 || nh < 1) return Grid<Color>(std::max(0, nw), std::max(0, nh));
    std::vector<P> d = toPs(g);
    if (nw != g.w) d = resampleAxis(d, g.w, g.h, nw, true);
    if (nh != g.h) d = resampleAxis(d, nw, g.h, nh, false);
    return fromPs(d, nw, nh);
}

Grid<Color> rotateSmooth(const Grid<Color>& g, double degrees, bool expand) {
    int nw = g.w;
    int nh = g.h;
    if (expand) img::rotatedSize(g.w, g.h, degrees, nw, nh);
    std::vector<P> out(static_cast<std::size_t>(nw) * static_cast<std::size_t>(nh));
    if (g.empty()) return fromPs(out, nw, nh);
    const std::vector<P> src = toPs(g);
    const double a = degrees * std::numbers::pi / 180.0;
    const double ca = std::cos(a);
    const double sa = std::sin(a);
    for (int y = 0; y < nh; ++y) {
        for (int x = 0; x < nw; ++x) {
            const double dx = x + 0.5 - nw / 2.0;
            const double dy = y + 0.5 - nh / 2.0;
            const double sx = g.w / 2.0 + dx * ca + dy * sa;
            const double sy = g.h / 2.0 - dx * sa + dy * ca;
            out[static_cast<std::size_t>(y) * static_cast<std::size_t>(nw) +
                static_cast<std::size_t>(x)] = bilinear(src, g.w, g.h, sx, sy, EDGE_CLEAR);
        }
    }
    return fromPs(out, nw, nh);
}

Grid<Color> skew(const Grid<Color>& g, double degX, double degY, bool wrap) {
    if (g.empty()) return g;
    const double tx = std::tan(std::clamp(degX, -80.0, 80.0) * std::numbers::pi / 180.0);
    const double ty = std::tan(std::clamp(degY, -80.0, 80.0) * std::numbers::pi / 180.0);
    const double det = 1 - tx * ty;
    if (std::fabs(det) < 1e-6) return g;
    const std::vector<P> src = toPs(g);
    std::vector<P> out(src.size());
    const double cx = g.w / 2.0;
    const double cy = g.h / 2.0;
    for (int y = 0; y < g.h; ++y) {
        for (int x = 0; x < g.w; ++x) {
            // Обратное отображение: какая точка источника легла сюда.
            const double dx = x + 0.5 - cx;
            const double dy = y + 0.5 - cy;
            const double sx = (dx - tx * dy) / det;
            const double sy = (dy - ty * dx) / det;
            out[static_cast<std::size_t>(y) * static_cast<std::size_t>(g.w) +
                static_cast<std::size_t>(x)] =
                bilinear(src, g.w, g.h, cx + sx, cy + sy, wrap ? EDGE_WRAP : EDGE_CLEAR);
        }
    }
    return fromPs(out, g.w, g.h);
}

Quad identityQuad(int w, int h) {
    Quad q;
    q.x = {0.0, static_cast<double>(w), static_cast<double>(w), 0.0};
    q.y = {0.0, 0.0, static_cast<double>(h), static_cast<double>(h)};
    return q;
}

Grid<Color> perspective(const Grid<Color>& g, const Quad& q) {
    if (g.empty()) return g;
    // Отображение приёмника в источник: углы четырёхугольника — в углы
    // картинки. Восемь неизвестных однородной матрицы 3x3, девятая — 1.
    const Quad src = identityQuad(g.w, g.h);
    std::array<std::array<double, 9>, 8> m{};
    for (std::size_t i = 0; i < 4; ++i) {
        const double x = q.x[i];
        const double y = q.y[i];
        const double u = src.x[i];
        const double v = src.y[i];
        m[2 * i] = {x, y, 1, 0, 0, 0, -u * x, -u * y, u};
        m[2 * i + 1] = {0, 0, 0, x, y, 1, -v * x, -v * y, v};
    }
    std::array<double, 8> h{};
    if (!solve8(m, h)) return Grid<Color>(g.w, g.h, argb(0, 0, 0, 0));
    const std::vector<P> s = toPs(g);
    std::vector<P> out(s.size());
    for (int y = 0; y < g.h; ++y) {
        for (int x = 0; x < g.w; ++x) {
            const double px = x + 0.5;
            const double py = y + 0.5;
            const double wv = h[6] * px + h[7] * py + 1;
            if (std::fabs(wv) < 1e-12) continue;
            const double u = (h[0] * px + h[1] * py + h[2]) / wv;
            const double v = (h[3] * px + h[4] * py + h[5]) / wv;
            if (u < 0 || v < 0 || u > g.w || v > g.h) continue;
            out[static_cast<std::size_t>(y) * static_cast<std::size_t>(g.w) +
                static_cast<std::size_t>(x)] = bilinear(s, g.w, g.h, u, v, EDGE_CLEAR);
        }
    }
    return fromPs(out, g.w, g.h);
}

// --- мягкая кисть ----------------------------------------------------------------

Grid<float> softDab(int size, int hardnessPct) {
    size = std::max(1, size);
    Grid<float> dab(size, size, 0.0F);
    const double r = size / 2.0;
    const double hard = std::clamp(hardnessPct, 0, 100) / 100.0;
    for (int y = 0; y < size; ++y) {
        for (int x = 0; x < size; ++x) {
            const double d = std::hypot(x + 0.5 - r, y + 0.5 - r) / r;
            double v = 0;
            if (size == 1 || d <= hard) {
                v = 1;
            } else if (d < 1) {
                const double t = (1 - d) / (1 - hard);
                v = t * t * (3 - 2 * t);
            }
            dab.at(x, y) = static_cast<float>(v);
        }
    }
    return dab;
}

}  // namespace tjtm::fx
