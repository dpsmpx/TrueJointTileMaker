#include "core/imageops.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>

namespace tjtm::img {

namespace {

/// Узор: '#' — первый цвет, '.' — второй. Строки одной длины.
struct Pattern {
    const char* name;
    std::vector<const char*> rows;
};

const std::vector<Pattern>& patterns() {
    // Первый — шахматка оригинала: (x + y) mod 2 = 0 даёт первый цвет.
    static const std::vector<Pattern> kPatterns = {
        {"Шахматка 50%", {"#.", ".#"}},
        {"Точки 25%", {"#.", ".."}},
        {"Точки 75%", {"##", "#."}},
        {"Редкие 12%", {"#...", "....", "..#.", "...."}},
        {"Частые 88%", {".###", "####", "##.#", "####"}},
        {"Строки", {"#", "."}},
        {"Столбцы", {"#."}},
        {"Диагональ \\", {"#...", ".#..", "..#.", "...#"}},
        {"Диагональ /", {"...#", "..#.", ".#..", "#..."}},
        {"Клетка 2×2", {"##..", "##..", "..##", "..##"}},
        {"Клетка 3×3", {"###...", "###...", "###...", "...###", "...###", "...###"}},
        {"Клетка 4×4",
         {"####....", "####....", "####....", "####....", "....####", "....####", "....####",
          "....####"}},
        {"Кластер 50%", {".##.", "####", ".##.", "...."}},
    };
    return kPatterns;
}

}  // namespace

std::string selOpName(int op) {
    switch (op) {
        case SEL_ADD: return "добавить";
        case SEL_SUBTRACT: return "вычесть";
        case SEL_INTERSECT: return "пересечь";
        default: return "заменить";
    }
}

bool maskBounds(const Mask& m, int& x, int& y, int& w, int& h) {
    int x0 = m.w;
    int y0 = m.h;
    int x1 = -1;
    int y1 = -1;
    for (int j = 0; j < m.h; ++j) {
        for (int i = 0; i < m.w; ++i) {
            if (m.at(i, j) == 0) continue;
            x0 = std::min(x0, i);
            y0 = std::min(y0, j);
            x1 = std::max(x1, i);
            y1 = std::max(y1, j);
        }
    }
    if (x1 < 0) {
        x = y = w = h = 0;
        return false;
    }
    x = x0;
    y = y0;
    w = x1 - x0 + 1;
    h = y1 - y0 + 1;
    return true;
}

int maskCount(const Mask& m) {
    return static_cast<int>(
        std::count_if(m.d.begin(), m.d.end(), [](std::uint8_t v) { return v != 0; }));
}

bool maskIsRect(const Mask& m, int x, int y, int w, int h) {
    for (int j = 0; j < m.h; ++j) {
        for (int i = 0; i < m.w; ++i) {
            const bool in = i >= x && i < x + w && j >= y && j < y + h;
            if ((m.at(i, j) != 0) != in) return false;
        }
    }
    return true;
}

void combine(Mask& dst, const Mask& src, int op) {
    if (op == SEL_REPLACE || !dst.sameSize(src.w, src.h)) {
        dst = src;
        // Вычесть или пересечь с пустотой — ничего; так и выходит, если
        // прежнего выделения не было.
        if (op == SEL_SUBTRACT || op == SEL_INTERSECT) std::fill(dst.d.begin(), dst.d.end(), 0);
        return;
    }
    for (std::size_t k = 0; k < dst.d.size(); ++k) {
        const bool a = dst.d[k] != 0;
        const bool b = src.d[k] != 0;
        bool r = b;
        if (op == SEL_ADD) {
            r = a || b;
        } else if (op == SEL_SUBTRACT) {
            r = a && !b;
        } else if (op == SEL_INTERSECT) {
            r = a && b;
        }
        dst.d[k] = r ? 1 : 0;
    }
}

Mask invert(const Mask& m) {
    Mask t(m.w, m.h);
    for (std::size_t k = 0; k < m.d.size(); ++k)
        t.d[k] = m.d[k] != 0 ? 0 : 1;
    return t;
}

Mask grow(const Mask& m, int n) {
    Mask cur = m;
    const bool expand = n > 0;
    for (int step = 0; step < std::abs(n); ++step) {
        Mask next = cur;
        for (int y = 0; y < cur.h; ++y) {
            for (int x = 0; x < cur.w; ++x) {
                // Расширение включает точку, если рядом выделенная; сужение
                // снимает, если рядом невыделенная или край картинки.
                if (expand == (cur.at(x, y) != 0)) continue;
                bool hit = false;
                for (int dy = -1; dy <= 1 && !hit; ++dy) {
                    for (int dx = -1; dx <= 1 && !hit; ++dx) {
                        if (dx == 0 && dy == 0) continue;
                        const bool in = cur.inside(x + dx, y + dy);
                        const bool sel = in && cur.at(x + dx, y + dy) != 0;
                        hit = expand ? sel : !sel;
                    }
                }
                if (hit) next.at(x, y) = expand ? 1 : 0;
            }
        }
        cur = std::move(next);
    }
    return cur;
}

Mask maskRect(int w, int h, int x0, int y0, int x1, int y1) {
    Mask m(w, h, 0);
    const int lx = std::max(0, std::min(x0, x1));
    const int hx = std::min(w - 1, std::max(x0, x1));
    const int ly = std::max(0, std::min(y0, y1));
    const int hy = std::min(h - 1, std::max(y0, y1));
    for (int y = ly; y <= hy; ++y) {
        for (int x = lx; x <= hx; ++x)
            m.at(x, y) = 1;
    }
    return m;
}

Mask maskEllipse(int w, int h, int x0, int y0, int x1, int y1) {
    Mask m(w, h, 0);
    const int lx = std::min(x0, x1);
    const int hx = std::max(x0, x1);
    const int ly = std::min(y0, y1);
    const int hy = std::max(y0, y1);
    const double cx = (lx + hx) / 2.0;
    const double cy = (ly + hy) / 2.0;
    const double ra = (hx - lx) / 2.0 + 0.5;
    const double rb = (hy - ly) / 2.0 + 0.5;
    for (int y = std::max(0, ly); y <= std::min(h - 1, hy); ++y) {
        for (int x = std::max(0, lx); x <= std::min(w - 1, hx); ++x) {
            const double dx = (x - cx) / ra;
            const double dy = (y - cy) / rb;
            if (dx * dx + dy * dy <= 1.0) m.at(x, y) = 1;
        }
    }
    return m;
}

Mask maskPolygon(int w, int h, const std::vector<std::pair<int, int>>& pts) {
    Mask m(w, h, 0);
    if (pts.empty()) return m;
    // Внутренность: луч вправо от центра точки, чёт-нечет пересечений.
    const std::size_t n = pts.size();
    for (int y = 0; y < h; ++y) {
        const double py = y;
        std::vector<double> xs;
        for (std::size_t k = 0; k < n; ++k) {
            const auto [ax, ay] = pts[k];
            const auto [bx, by] = pts[(k + 1) % n];
            if ((ay > py) == (by > py)) continue;
            xs.push_back(ax + (py - ay) * (bx - ax) / static_cast<double>(by - ay));
        }
        std::sort(xs.begin(), xs.end());
        for (std::size_t k = 0; k + 1 < xs.size(); k += 2) {
            const int from = std::max(0, static_cast<int>(std::ceil(xs[k])));
            const int to = std::min(w - 1, static_cast<int>(std::floor(xs[k + 1])));
            for (int x = from; x <= to; ++x)
                m.at(x, y) = 1;
        }
    }
    // Сам путь — отрезками между точками: при быстром движении мыши точки
    // идут не подряд, а граница выделения должна быть сплошной.
    for (std::size_t k = 0; k < n; ++k) {
        int x0 = pts[k].first;
        int y0 = pts[k].second;
        const int x1 = pts[(k + 1) % n].first;
        const int y1 = pts[(k + 1) % n].second;
        const int dx = std::abs(x1 - x0);
        const int dy = -std::abs(y1 - y0);
        const int sx = x0 < x1 ? 1 : -1;
        const int sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        while (true) {
            if (m.inside(x0, y0)) m.at(x0, y0) = 1;
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
    }
    return m;
}

Mask maskByColor(const Grid<Color>& g, int sx, int sy, bool contiguous) {
    Mask m(g.w, g.h, 0);
    if (!g.inside(sx, sy)) return m;
    const Color target = g.at(sx, sy);
    if (!contiguous) {
        for (std::size_t k = 0; k < g.d.size(); ++k)
            m.d[k] = sameColor(g.d[k], target) ? 1 : 0;
        return m;
    }
    std::vector<std::pair<int, int>> stack{{sx, sy}};
    m.at(sx, sy) = 1;
    while (!stack.empty()) {
        const auto [x, y] = stack.back();
        stack.pop_back();
        const int nb[4][2] = {{x + 1, y}, {x - 1, y}, {x, y + 1}, {x, y - 1}};
        for (const auto& p : nb) {
            if (!g.inside(p[0], p[1]) || m.at(p[0], p[1]) != 0) continue;
            if (!sameColor(g.at(p[0], p[1]), target)) continue;
            m.at(p[0], p[1]) = 1;
            stack.emplace_back(p[0], p[1]);
        }
    }
    return m;
}

Mask maskOpaque(const Grid<Color>& g) {
    Mask m(g.w, g.h, 0);
    for (std::size_t k = 0; k < g.d.size(); ++k)
        m.d[k] = g.d[k].a != 0 ? 1 : 0;
    return m;
}

bool contentBounds(const Grid<Color>& g, int& x, int& y, int& w, int& h) {
    return maskBounds(maskOpaque(g), x, y, w, h);
}

int outline(Grid<Color>& g, const Mask& within, Color c, bool eightWay, bool outside) {
    const bool all = within.empty();
    auto allowed = [&](int x, int y) {
        return all || (within.inside(x, y) && within.at(x, y) != 0);
    };
    auto solid = [&](int x, int y) { return g.inside(x, y) && g.at(x, y).a != 0; };
    std::vector<std::pair<int, int>> hits;
    for (int y = 0; y < g.h; ++y) {
        for (int x = 0; x < g.w; ++x) {
            if (!allowed(x, y)) continue;
            // Снаружи — прозрачная точка у непрозрачной; внутри —
            // непрозрачная у прозрачной или у края картинки.
            if (outside == solid(x, y)) continue;
            bool edge = false;
            for (int dy = -1; dy <= 1 && !edge; ++dy) {
                for (int dx = -1; dx <= 1 && !edge; ++dx) {
                    if ((dx == 0 && dy == 0) || (!eightWay && dx != 0 && dy != 0)) continue;
                    edge = outside ? solid(x + dx, y + dy) : !solid(x + dx, y + dy);
                }
            }
            if (edge) hits.emplace_back(x, y);
        }
    }
    // Сначала поиск, потом запись: иначе свежий контур сам бы стал
    // «непрозрачным соседом» и пополз дальше.
    for (const auto& [x, y] : hits)
        g.at(x, y) = c;
    return static_cast<int>(hits.size());
}

int removeIsolated(Grid<Color>& g, const Mask& within) {
    const bool all = within.empty();
    std::vector<std::pair<std::pair<int, int>, Color>> fixes;
    for (int y = 0; y < g.h; ++y) {
        for (int x = 0; x < g.w; ++x) {
            if (!all && (!within.inside(x, y) || within.at(x, y) == 0)) continue;
            bool have = false;
            bool same = true;
            Color ref{};
            for (int dy = -1; dy <= 1 && same; ++dy) {
                for (int dx = -1; dx <= 1 && same; ++dx) {
                    if ((dx == 0 && dy == 0) || !g.inside(x + dx, y + dy)) continue;
                    const Color n = g.at(x + dx, y + dy);
                    if (!have) {
                        ref = n;
                        have = true;
                    } else if (!sameColor(n, ref)) {
                        same = false;
                    }
                }
            }
            if (have && same && !sameColor(ref, g.at(x, y))) fixes.push_back({{x, y}, ref});
        }
    }
    for (const auto& [p, c] : fixes)
        g.at(p.first, p.second) = c;
    return static_cast<int>(fixes.size());
}

int ditherPatternCount() {
    return static_cast<int>(patterns().size());
}

std::string ditherPatternName(int i) {
    if (i < 0 || i >= ditherPatternCount()) return {};
    return patterns()[static_cast<std::size_t>(i)].name;
}

bool ditherFirst(int pattern, int x, int y) {
    const auto& ps = patterns();
    const Pattern& p =
        ps[static_cast<std::size_t>(std::clamp(pattern, 0, ditherPatternCount() - 1))];
    const int ph = static_cast<int>(p.rows.size());
    const int pw = static_cast<int>(std::char_traits<char>::length(p.rows[0]));
    const int px = ((x % pw) + pw) % pw;
    const int py = ((y % ph) + ph) % ph;
    return p.rows[static_cast<std::size_t>(py)][px] == '#';
}

bool isPixelPerfectCorner(int ax, int ay, int bx, int by, int cx, int cy) {
    // a и c — соседи по диагонали, b примыкает к обоим по стороне.
    if (std::abs(ax - cx) != 1 || std::abs(ay - cy) != 1) return false;
    return (bx == ax && by == cy) || (bx == cx && by == ay);
}

}  // namespace tjtm::img
