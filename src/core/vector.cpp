#include "core/vector.h"

#include <algorithm>
#include <cmath>
#include <numbers>

#include "core/imageops.h"

namespace tjtm::vec {

namespace {

double dist(const VecPoint& a, const VecPoint& b) {
    return std::hypot(a.x - b.x, a.y - b.y);
}

/// Точка сплайна Катмулла — Рома между p1 и p2: кривая проходит через
/// каждую опорную точку, касательная в ней — по соседям.
VecPoint catmullRom(const VecPoint& p0, const VecPoint& p1, const VecPoint& p2, const VecPoint& p3,
                    double t) {
    const double t2 = t * t;
    const double t3 = t2 * t;
    auto f = [&](double a, double b, double c, double d) {
        return 0.5 * (2 * b + (-a + c) * t + (2 * a - 5 * b + 4 * c - d) * t2 +
                      (-a + 3 * b - 3 * c + d) * t3);
    };
    return {f(p0.x, p1.x, p2.x, p3.x), f(p0.y, p1.y, p2.y, p3.y)};
}

void curvePath(const std::vector<VecPoint>& pts, bool closed, std::vector<VecPoint>& out) {
    const std::size_t n = pts.size();
    if (n < 3) {
        out = pts;
        return;
    }
    const std::size_t segments = closed ? n : n - 1;
    for (std::size_t i = 0; i < segments; ++i) {
        const VecPoint& p1 = pts[i];
        const VecPoint& p2 = pts[(i + 1) % n];
        const VecPoint& p0 = closed ? pts[(i + n - 1) % n] : (i > 0 ? pts[i - 1] : p1);
        const VecPoint& p3 = closed ? pts[(i + 2) % n] : (i + 2 < n ? pts[i + 2] : p2);
        // Шаг короче половины точки: отрезки между выборками не перескакивают
        // через точки растра.
        const int steps = std::max(1, static_cast<int>(std::ceil(dist(p1, p2) * 2)));
        for (int s = 0; s < steps; ++s)
            out.push_back(catmullRom(p0, p1, p2, p3, static_cast<double>(s) / steps));
    }
    if (!closed) out.push_back(pts.back());
}

void ellipseBox(const VecShape& s, int& x0, int& y0, int& x1, int& y1) {
    x0 = static_cast<int>(std::floor(std::min(s.pts[0].x, s.pts[1].x)));
    x1 = static_cast<int>(std::floor(std::max(s.pts[0].x, s.pts[1].x)));
    y0 = static_cast<int>(std::floor(std::min(s.pts[0].y, s.pts[1].y)));
    y1 = static_cast<int>(std::floor(std::max(s.pts[0].y, s.pts[1].y)));
}

/// Точки отрезков пути подряд, без повторов; для толщины 1 — без углов «Г».
std::vector<std::pair<int, int>> pixelPath(const std::vector<VecPoint>& path, bool closed,
                                           bool perfect) {
    std::vector<std::pair<int, int>> seq;
    auto push = [&](int x, int y) {
        if (!seq.empty() && seq.back() == std::make_pair(x, y)) return;
        seq.emplace_back(x, y);
        // Угол «Г» убирается сразу: следующая точка его уже не увидит.
        if (perfect) {
            while (seq.size() >= 3) {
                const auto [ax, ay] = seq[seq.size() - 3];
                const auto [bx, by] = seq[seq.size() - 2];
                const auto [cx, cy] = seq[seq.size() - 1];
                if (!img::isPixelPerfectCorner(ax, ay, bx, by, cx, cy)) break;
                seq.erase(seq.end() - 2);
            }
        }
    };
    auto line = [&](int x0, int y0, int x1, int y1) {
        const int dx = std::abs(x1 - x0);
        const int dy = -std::abs(y1 - y0);
        const int sx = x0 < x1 ? 1 : -1;
        const int sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        while (true) {
            push(x0, y0);
            if (x0 == x1 && y0 == y1) break;
            const int e2 = 2 * err;
            if (e2 >= dy) {
                err += dy;
                x0 += sx;
            }
            if (e2 <= dx) {
                err += dx;
                y0 += sy;
            }
        }
    };
    auto fl = [](double v) { return static_cast<int>(std::floor(v)); };
    if (path.size() == 1) push(fl(path[0].x), fl(path[0].y));
    for (std::size_t k = 0; k + 1 < path.size(); ++k)
        line(fl(path[k].x), fl(path[k].y), fl(path[k + 1].x), fl(path[k + 1].y));
    if (closed && path.size() >= 2)
        line(fl(path.back().x), fl(path.back().y), fl(path.front().x), fl(path.front().y));
    return seq;
}

void blendAt(Grid<Color>& img, Grid<std::uint8_t>& done, int x, int y, Color c) {
    if (!img.inside(x, y) || done.at(x, y) != 0) return;
    done.at(x, y) = 1;
    img.at(x, y) = blendOver(c, img.at(x, y));
}

void stroke(const std::vector<std::pair<int, int>>& seq, int width, Color c, Grid<Color>& img) {
    Grid<std::uint8_t> done(img.w, img.h, 0);
    const int lo = -(width - 1) / 2;
    const int hi = width / 2;
    for (const auto& [x, y] : seq) {
        for (int dy = lo; dy <= hi; ++dy) {
            for (int dx = lo; dx <= hi; ++dx)
                blendAt(img, done, x + dx, y + dy, c);
        }
    }
}

/// Заливка многоугольника по центрам точек, чёт-нечет.
void fillPolygon(const std::vector<VecPoint>& path, Color c, Grid<Color>& img) {
    if (path.size() < 3) return;
    const auto [lo, hi] = std::minmax_element(
        path.begin(), path.end(), [](const VecPoint& a, const VecPoint& b) { return a.y < b.y; });
    const double minY = lo->y;
    const double maxY = hi->y;
    const int y0 = std::max(0, static_cast<int>(std::floor(minY)));
    const int y1 = std::min(img.h - 1, static_cast<int>(std::ceil(maxY)));
    std::vector<double> xs;
    for (int y = y0; y <= y1; ++y) {
        const double sy = y + 0.5;
        xs.clear();
        for (std::size_t k = 0; k < path.size(); ++k) {
            const VecPoint& a = path[k];
            const VecPoint& b = path[(k + 1) % path.size()];
            if ((a.y <= sy && sy < b.y) || (b.y <= sy && sy < a.y))
                xs.push_back(a.x + (sy - a.y) * (b.x - a.x) / (b.y - a.y));
        }
        std::sort(xs.begin(), xs.end());
        for (std::size_t k = 0; k + 1 < xs.size(); k += 2) {
            const int xa = std::max(0, static_cast<int>(std::ceil(xs[k] - 0.5)));
            const int xb = std::min(img.w - 1, static_cast<int>(std::floor(xs[k + 1] - 0.5)));
            for (int x = xa; x <= xb; ++x)
                img.at(x, y) = blendOver(c, img.at(x, y));
        }
    }
}

void rasterizeEllipse(const VecShape& s, Grid<Color>& img) {
    int x0 = 0;
    int y0 = 0;
    int x1 = 0;
    int y1 = 0;
    ellipseBox(s, x0, y0, x1, y1);
    const double cx = (x0 + x1 + 1) / 2.0;
    const double cy = (y0 + y1 + 1) / 2.0;
    const double rx = (x1 - x0 + 1) / 2.0;
    const double ry = (y1 - y0 + 1) / 2.0;
    const int w = x1 - x0 + 1;
    const int h = y1 - y0 + 1;
    // Внутренность — по центрам точек; край — кольцами по четырём соседям.
    Grid<std::uint8_t> in(w, h, 0);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const double dx = (x0 + x + 0.5 - cx) / rx;
            const double dy = (y0 + y + 0.5 - cy) / ry;
            in.at(x, y) = dx * dx + dy * dy <= 1.0 + 1e-9 ? 1 : 0;
        }
    }
    if (w <= 2 || h <= 2) in = Grid<std::uint8_t>(w, h, 1);  // кроха — сплошная
    const Color strokeC = s.stroke.a != 0 ? s.stroke : s.fill;
    const int layers = std::max(1, s.width);
    Grid<std::uint8_t> ring(w, h, 0);
    Grid<std::uint8_t> cur = in;
    for (int l = 0; l < layers; ++l) {
        std::vector<std::pair<int, int>> edge;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                if (cur.at(x, y) == 0) continue;
                const bool e = x == 0 || y == 0 || x == w - 1 || y == h - 1 ||
                               cur.at(x - 1, y) == 0 || cur.at(x + 1, y) == 0 ||
                               cur.at(x, y - 1) == 0 || cur.at(x, y + 1) == 0;
                if (e) edge.emplace_back(x, y);
            }
        }
        for (const auto& [x, y] : edge) {
            ring.at(x, y) = 1;
            cur.at(x, y) = 0;
        }
    }
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (!img.inside(x0 + x, y0 + y) || in.at(x, y) == 0) continue;
            Color& d = img.at(x0 + x, y0 + y);
            if (ring.at(x, y) != 0) {
                if (strokeC.a != 0) d = blendOver(strokeC, d);
            } else if (s.fill.a != 0) {
                d = blendOver(s.fill, d);
            }
        }
    }
}

}  // namespace

std::vector<VecPoint> shapePath(const VecShape& s, bool& closed) {
    closed = false;
    std::vector<VecPoint> out;
    const auto& p = s.pts;
    switch (s.type) {
        case VS_POLYLINE: return p;
        case VS_POLYGON: closed = p.size() >= 3; return p;
        case VS_CURVE: curvePath(p, false, out); return out;
        case VS_CLOSED_CURVE:
            closed = p.size() >= 3;
            curvePath(p, closed, out);
            return out;
        case VS_RECT:
            if (p.size() < 2) return p;
            closed = true;
            return {p[0], {p[1].x, p[0].y}, p[1], {p[0].x, p[1].y}};
        case VS_ELLIPSE: {
            if (p.size() < 2) return p;
            closed = true;
            const double cx = (p[0].x + p[1].x) / 2;
            const double cy = (p[0].y + p[1].y) / 2;
            const double rx = std::fabs(p[1].x - p[0].x) / 2 + 0.5;
            const double ry = std::fabs(p[1].y - p[0].y) / 2 + 0.5;
            const int steps = std::max(16, static_cast<int>(std::ceil((rx + ry) * 3)));
            for (int k = 0; k < steps; ++k) {
                const double a = 2 * std::numbers::pi * k / steps;
                out.push_back({cx + rx * std::cos(a), cy + ry * std::sin(a)});
            }
            return out;
        }
        case VS_STAR: {
            if (p.size() < 2) return p;
            closed = true;
            const double r = dist(p[0], p[1]);
            const double inner = r * std::clamp(s.innerPct, 5, 100) / 100.0;
            const int n = std::clamp(s.starPoints, 3, 32);
            const double a0 = std::atan2(p[1].y - p[0].y, p[1].x - p[0].x);
            for (int k = 0; k < 2 * n; ++k) {
                const double a = a0 + std::numbers::pi * k / n;
                const double rr = k % 2 == 0 ? r : inner;
                out.push_back({p[0].x + rr * std::cos(a), p[0].y + rr * std::sin(a)});
            }
            return out;
        }
        default: return p;
    }
}

void rasterize(const VecShape& s, Grid<Color>& img) {
    if (s.pts.empty()) return;
    const bool twoPoint = s.type == VS_RECT || s.type == VS_ELLIPSE || s.type == VS_STAR;
    if (twoPoint && s.pts.size() < 2) return;
    if (s.type == VS_ELLIPSE) {
        rasterizeEllipse(s, img);
        return;
    }
    bool closed = false;
    const std::vector<VecPoint> path = shapePath(s, closed);
    if (closed && s.fill.a != 0) fillPolygon(path, s.fill, img);
    // Без контура край ложится цветом заливки: залитая фигура пиксель-арта
    // включает свои граничные точки.
    const Color c = s.stroke.a != 0 ? s.stroke : (closed ? s.fill : Color{});
    if (c.a == 0) return;
    const int width = std::max(1, s.width);
    // Углы «Г» убираются только у кривых: у ломаной и многоугольника угол —
    // это вершина, которую поставили нарочно.
    const bool curve = s.type == VS_CURVE || s.type == VS_CLOSED_CURVE;
    stroke(pixelPath(path, closed, curve && width == 1), width, c, img);
}

Grid<Color> rasterizeAll(const std::vector<VecShape>& shapes, int w, int h) {
    Grid<Color> g(w, h, argb(0, 0, 0, 0));
    for (const VecShape& s : shapes)
        rasterize(s, g);
    return g;
}

bool hitVertex(const std::vector<VecShape>& shapes, double x, double y, double r, int& shape,
               int& vertex) {
    for (std::size_t i = shapes.size(); i-- > 0;) {
        const auto& pts = shapes[i].pts;
        for (std::size_t k = 0; k < pts.size(); ++k) {
            if (std::hypot(pts[k].x - x, pts[k].y - y) <= r) {
                shape = static_cast<int>(i);
                vertex = static_cast<int>(k);
                return true;
            }
        }
    }
    return false;
}

int hitShape(const std::vector<VecShape>& shapes, double x, double y, double r) {
    for (std::size_t i = shapes.size(); i-- > 0;) {
        bool closed = false;
        const std::vector<VecPoint> path = shapePath(shapes[i], closed);
        const std::size_t n = path.size();
        for (std::size_t k = 0; k + (closed ? 0 : 1) < n && n >= 2; ++k) {
            const VecPoint& a = path[k];
            const VecPoint& b = path[(k + 1) % n];
            const double vx = b.x - a.x;
            const double vy = b.y - a.y;
            const double len2 = vx * vx + vy * vy;
            const double t =
                len2 > 0 ? std::clamp(((x - a.x) * vx + (y - a.y) * vy) / len2, 0.0, 1.0) : 0.0;
            if (std::hypot(a.x + vx * t - x, a.y + vy * t - y) <= r) return static_cast<int>(i);
        }
        if (closed && shapes[i].fill.a != 0) {
            bool in = false;
            for (std::size_t k = 0, j = n - 1; k < n; j = k++) {
                if ((path[k].y > y) != (path[j].y > y) &&
                    x < (path[j].x - path[k].x) * (y - path[k].y) / (path[j].y - path[k].y) +
                            path[k].x)
                    in = !in;
            }
            if (in) return static_cast<int>(i);
        }
    }
    return -1;
}

void translate(VecShape& s, double dx, double dy) {
    for (VecPoint& p : s.pts) {
        p.x += dx;
        p.y += dy;
    }
}

std::string shapeName(int type) {
    switch (type) {
        case VS_POLYLINE: return "Ломаная";
        case VS_POLYGON: return "Многоугольник";
        case VS_CURVE: return "Кривая";
        case VS_CLOSED_CURVE: return "Замкнутая кривая";
        case VS_RECT: return "Прямоугольник";
        case VS_ELLIPSE: return "Овал";
        case VS_STAR: return "Звезда";
        default: return "Фигура";
    }
}

}  // namespace tjtm::vec
