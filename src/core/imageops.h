// Операции над картинкой и масками выделения (этап 1 RoadToProLevel).
//
// Всё здесь — чистые функции над сетками, без состояния редактора: Model
// собирает из них команды, а тесты проверяют их по отдельности. Геометрия
// — шаблоны: та же перестановка пикселей применяется и к цветам, и к
// привязкам палитры, и к маске выделения, и они не расходятся.
//
// Главное правило пиксель-арта: у каждой операции предсказуемое
// пиксельное представление. Масштаб — только ближайшим соседом; поворот на
// любой угол (этап 5) — ближайшим соседом или RotSprite, без новых цветов.
#pragma once

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <string>
#include <vector>

#include "core/color.h"
#include "core/grid.h"

namespace tjtm::img {

using Mask = Grid<std::uint8_t>;

// --- геометрия --------------------------------------------------------------

template <class T>
Grid<T> flipH(const Grid<T>& g) {
    Grid<T> t(g.w, g.h);
    for (int y = 0; y < g.h; ++y) {
        for (int x = 0; x < g.w; ++x)
            t.at(x, y) = g.at(g.w - 1 - x, y);
    }
    return t;
}

template <class T>
Grid<T> flipV(const Grid<T>& g) {
    Grid<T> t(g.w, g.h);
    for (int y = 0; y < g.h; ++y) {
        for (int x = 0; x < g.w; ++x)
            t.at(x, y) = g.at(x, g.h - 1 - y);
    }
    return t;
}

/// Четверть оборота по часовой стрелке: стороны меняются местами.
template <class T>
Grid<T> rotate90(const Grid<T>& g) {
    Grid<T> t(g.h, g.w);
    for (int y = 0; y < g.h; ++y) {
        for (int x = 0; x < g.w; ++x)
            t.at(g.h - 1 - y, x) = g.at(x, y);
    }
    return t;
}

template <class T>
Grid<T> rotate180(const Grid<T>& g) {
    Grid<T> t(g.w, g.h);
    for (int y = 0; y < g.h; ++y) {
        for (int x = 0; x < g.w; ++x)
            t.at(g.w - 1 - x, g.h - 1 - y) = g.at(x, y);
    }
    return t;
}

/// Четверть оборота против часовой стрелки (270° по часовой).
template <class T>
Grid<T> rotate270(const Grid<T>& g) {
    Grid<T> t(g.h, g.w);
    for (int y = 0; y < g.h; ++y) {
        for (int x = 0; x < g.w; ++x)
            t.at(y, g.w - 1 - x) = g.at(x, y);
    }
    return t;
}

/// Вырезка (x, y, w, h); то, что за краем исходной, заполняется fill.
template <class T>
Grid<T> crop(const Grid<T>& g, int x0, int y0, int w, int h, const T& fill) {
    Grid<T> t(w, h, fill);
    for (int y = 0; y < t.h; ++y) {
        for (int x = 0; x < t.w; ++x) {
            if (g.inside(x0 + x, y0 + y)) t.at(x, y) = g.at(x0 + x, y0 + y);
        }
    }
    return t;
}

/// Место старой картинки на новом холсте по якорю: 0 — к началу, 1 — по
/// центру, 2 — к концу. При нечётной разнице центр смещён к началу, как
/// у Aseprite и Pixelorama.
inline int anchorOffset(int oldSize, int newSize, int anchor) {
    if (anchor <= 0) return 0;
    if (anchor >= 2) return newSize - oldSize;
    return (newSize - oldSize) / 2;
}

/// Новый размер холста; картинка ставится по якорю и не масштабируется.
template <class T>
Grid<T> resizeCanvas(const Grid<T>& g, int nw, int nh, int anchorX, int anchorY, const T& fill) {
    const int ox = anchorOffset(g.w, nw, anchorX);
    const int oy = anchorOffset(g.h, nh, anchorY);
    return crop(g, -ox, -oy, nw, nh, fill);
}

/// Сдвиг с переносом: ушедшее за край выходит с другой стороны. Так
/// проверяют шов тайла — сдвигом на половину стороны.
template <class T>
Grid<T> offsetWrap(const Grid<T>& g, int dx, int dy) {
    Grid<T> t(g.w, g.h);
    if (g.empty()) return t;
    for (int y = 0; y < g.h; ++y) {
        for (int x = 0; x < g.w; ++x) {
            const int nx = ((x + dx) % g.w + g.w) % g.w;
            const int ny = ((y + dy) % g.h + g.h) % g.h;
            t.at(nx, ny) = g.at(x, y);
        }
    }
    return t;
}

/// Масштаб ближайшим соседом по центрам точек приёмника. При целом
/// увеличении каждая точка становится ровным квадратом, при целом
/// уменьшении берётся точка из середины квадрата.
template <class T>
Grid<T> scaleNearest(const Grid<T>& g, int nw, int nh) {
    Grid<T> t(nw, nh);
    if (g.empty() || t.empty()) return t;
    for (int y = 0; y < nh; ++y) {
        const auto sy = static_cast<int>((static_cast<long long>(y) * g.h * 2 + g.h) / (2LL * nh));
        for (int x = 0; x < nw; ++x) {
            const auto sx =
                static_cast<int>((static_cast<long long>(x) * g.w * 2 + g.w) / (2LL * nw));
            t.at(x, y) = g.at(sx, sy);
        }
    }
    return t;
}

// --- поворот на любой угол (этап 5) -------------------------------------------

/// Способ поворота: ближайший сосед или RotSprite — увеличение в 8 раз по
/// Scale2x, поворот ближайшим соседом и обратное уменьшение. RotSprite
/// держит линии пиксель-арта целыми, но, как и ближайший, не вводит ни
/// одного нового цвета.
inline constexpr int ROT_NEAREST = 0, ROT_SPRITE = 1;

namespace detail {

/// Точка (X, Y) картинки, увеличенной Scale2x Level раз, без самой
/// увеличенной картинки: 512x512 в 8 раз — 16 миллионов точек, а точке
/// поворота нужны лишь её соседи. Уровень — параметр шаблона: глубина
/// известна при сборке, и каждая ступень — своя функция.
template <int Level, class T>
T scale2xAt(const Grid<T>& g, int X, int Y) {
    X = std::clamp(X, 0, (g.w << Level) - 1);
    Y = std::clamp(Y, 0, (g.h << Level) - 1);
    if constexpr (Level == 0) {
        return g.at(X, Y);
    } else {
        const int bx = X >> 1;
        const int by = Y >> 1;
        const T P = scale2xAt<Level - 1>(g, bx, by);
        const T A = scale2xAt<Level - 1>(g, bx, by - 1);
        const T B = scale2xAt<Level - 1>(g, bx + 1, by);
        const T C = scale2xAt<Level - 1>(g, bx - 1, by);
        const T D = scale2xAt<Level - 1>(g, bx, by + 1);
        const bool right = (X & 1) != 0;
        const bool down = (Y & 1) != 0;
        if (!right && !down) return (C == A && C != D && A != B) ? A : P;
        if (right && !down) return (A == B && A != C && B != D) ? B : P;
        if (!right) return (D == C && D != B && C != A) ? C : P;
        return (B == D && B != A && D != C) ? D : P;
    }
}

}  // namespace detail

/// Размер картинки w x h, повёрнутой на degrees, если холст растёт до
/// описанного прямоугольника.
inline void rotatedSize(int w, int h, double degrees, int& nw, int& nh) {
    const double a = degrees * std::numbers::pi / 180.0;
    const double ca = std::fabs(std::cos(a));
    const double sa = std::fabs(std::sin(a));
    nw = std::max(1, static_cast<int>(std::ceil(w * ca + h * sa - 1e-6)));
    nh = std::max(1, static_cast<int>(std::ceil(w * sa + h * ca - 1e-6)));
}

/// Поворот на degrees по часовой вокруг середины. expand — холст растёт до
/// описанного прямоугольника, иначе размер прежний и углы срезаются.
/// Точка приёмника берёт точку источника под своим центром: на четверти
/// оборота это в точности rotate90 и прочие.
template <class T>
Grid<T> rotateAny(const Grid<T>& g, double degrees, int method, bool expand, const T& clear) {
    int nw = g.w;
    int nh = g.h;
    if (expand) rotatedSize(g.w, g.h, degrees, nw, nh);
    Grid<T> t(nw, nh, clear);
    if (g.empty()) return t;
    const double a = degrees * std::numbers::pi / 180.0;
    const double ca = std::cos(a);
    const double sa = std::sin(a);
    const double scx = g.w / 2.0;
    const double scy = g.h / 2.0;
    const double dcx = nw / 2.0;
    const double dcy = nh / 2.0;
    for (int y = 0; y < nh; ++y) {
        for (int x = 0; x < nw; ++x) {
            // Обратный поворот центра точки приёмника (ось y смотрит вниз).
            const double dx = x + 0.5 - dcx;
            const double dy = y + 0.5 - dcy;
            const double sx = scx + dx * ca + dy * sa;
            const double sy = scy - dx * sa + dy * ca;
            if (sx < 0 || sy < 0 || sx >= g.w || sy >= g.h) continue;
            if (method == ROT_SPRITE) {
                t.at(x, y) = detail::scale2xAt<3>(g, static_cast<int>(std::floor(sx * 8)),
                                                  static_cast<int>(std::floor(sy * 8)));
            } else {
                t.at(x, y) = g.at(std::min(g.w - 1, static_cast<int>(std::floor(sx))),
                                  std::min(g.h - 1, static_cast<int>(std::floor(sy))));
            }
        }
    }
    return t;
}

// --- маски ------------------------------------------------------------------

/// Как новая область сочетается с прежним выделением.
enum SelOp : int { SEL_REPLACE = 0, SEL_ADD = 1, SEL_SUBTRACT = 2, SEL_INTERSECT = 3 };
std::string selOpName(int op);

/// Рамка непустых точек маски; false — маска пуста.
bool maskBounds(const Mask& m, int& x, int& y, int& w, int& h);
int maskCount(const Mask& m);
/// Маска целиком совпадает с прямоугольником (x, y, w, h).
bool maskIsRect(const Mask& m, int x, int y, int w, int h);
void combine(Mask& dst, const Mask& src, int op);
Mask invert(const Mask& m);
/// Расширение (n > 0) или сужение (n < 0) на n точек по восьми соседям.
Mask grow(const Mask& m, int n);

Mask maskRect(int w, int h, int x0, int y0, int x1, int y1);
/// Овал, вписанный в рамку, — по тому же уравнению, что заливка овала.
Mask maskEllipse(int w, int h, int x0, int y0, int x1, int y1);
/// Лассо: замкнутый многоугольник через центры точек пути. Сам путь входит
/// в маску целиком, внутренность — по правилу чёт-нечет.
Mask maskPolygon(int w, int h, const std::vector<std::pair<int, int>>& pts);
/// Точки того же цвета: связная область от (sx, sy) по четырём соседям
/// (волшебная палочка) или все такие точки картинки.
Mask maskByColor(const Grid<Color>& g, int sx, int sy, bool contiguous);
/// Все точки, где есть хоть какая-то непрозрачность.
Mask maskOpaque(const Grid<Color>& g);

// --- рамка содержимого --------------------------------------------------------

/// Рамка непрозрачных точек; false — картинка пуста.
bool contentBounds(const Grid<Color>& g, int& x, int& y, int& w, int& h);

// --- пиксель-арт --------------------------------------------------------------

/// Контур цвета c вокруг непрозрачного: снаружи — по прозрачным соседям
/// непрозрачных точек, внутри — по непрозрачным точкам у края. Только в
/// пределах маски (пустая — вся картинка). Возвращает число точек.
int outline(Grid<Color>& g, const Mask& within, Color c, bool eightWay, bool outside);
/// Контур толщиной в thickness точек: снаружи — кольцами наружу, внутри —
/// все непрозрачные точки не дальше thickness от края (по 4 или 8 соседям).
int outlineThick(Grid<Color>& g, const Mask& within, Color c, bool eightWay, bool outside,
                 int thickness);
/// Нарастить край на точку: прозрачная точка у непрозрачной берёт цвет
/// соседа (сначала слева, справа, сверху, снизу, потом по диагоналям).
int growEdge(Grid<Color>& g, const Mask& within, bool eightWay);
/// Срезать край на точку: непрозрачная точка у прозрачной или у края
/// картинки становится прозрачной.
int shrinkEdge(Grid<Color>& g, const Mask& within, bool eightWay);
/// Одиночная точка — та, все соседи которой (в пределах картинки) одного
/// цвета, и не её: она перекрашивается в этот цвет. Возвращает число.
int removeIsolated(Grid<Color>& g, const Mask& within);

// --- узоры дизеринга ----------------------------------------------------------

int ditherPatternCount();
std::string ditherPatternName(int i);
/// true — в точке (x, y) документа узор берёт первый цвет. Узор привязан к
/// началу документа: два мазка рядом сходятся без шва.
bool ditherFirst(int pattern, int x, int y);

// --- «пиксель-перфект» --------------------------------------------------------

/// Три последние точки мазка образуют угол «Г»: средняя лишняя — без неё
/// линия остаётся связной по диагонали и не получает ступеньки в две точки.
bool isPixelPerfectCorner(int ax, int ay, int bx, int by, int cx, int cy);

}  // namespace tjtm::img
