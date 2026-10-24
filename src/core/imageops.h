// Операции над картинкой и масками выделения (этап 1 RoadToProLevel).
//
// Всё здесь — чистые функции над сетками, без состояния редактора: Model
// собирает из них команды, а тесты проверяют их по отдельности. Геометрия
// — шаблоны: та же перестановка пикселей применяется и к цветам, и к
// привязкам палитры, и к маске выделения, и они не расходятся.
//
// Главное правило пиксель-арта: у каждой операции предсказуемое
// пиксельное представление. Масштаб — только ближайшим соседом, повороты —
// только на четверти оборота, никакого сглаживания.
#pragma once

#include <cstdint>
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
