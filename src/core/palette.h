// Палитра произвольного размера (этап 2 RoadToProLevel): чистые функции.
//
// Палитра — список цветов любой длины до MAX_PALETTE. Здесь — то, что с ним
// делают, не зная о картинке: порядок сортировки, ряды между цветами, сдвиг
// тона, извлечение палитры из картинки, файлы палитр. Привязки точек к
// ячейкам переставляет Model по перестановкам, которые отсюда возвращаются.
#pragma once

#include <string>
#include <vector>

#include "core/color.h"
#include "core/grid.h"

namespace tjtm::pal {

/// Больше индексная картинка не держит: номер точки — байт.
inline constexpr int MAX_PALETTE = 256;

enum SortKey : int {
    BY_HUE,
    BY_SAT,
    BY_VAL,
    BY_LUMA,
    BY_RED,
    BY_GREEN,
    BY_BLUE,
    BY_ALPHA,
    SORT_KEYS
};
std::string sortKeyName(int key);

/// Порядок ячеек [from, to] после сортировки: order[k] — прежний номер
/// ячейки, которая встанет на место from + k. Сортировка устойчивая: равные
/// не перемешиваются.
std::vector<int> sortOrder(const std::vector<Color>& p, int key, int from, int to);

/// Ряд из steps цветов от a до b включительно. hueShift — изгиб тона в
/// градусах к середине ряда: тени холоднее, света теплее — так строят
/// рамп в пиксель-арте, а не прямой смесью.
std::vector<Color> ramp(Color a, Color b, int steps, int hueShift);

/// Сдвиг тона (градусы), насыщенности и яркости (проценты) с прежней
/// прозрачностью.
Color adjustHSV(Color c, int dh, int ds, int dv);

/// Цвета картинки: непрозрачные, без повторов, в порядке появления. Если
/// их больше maxColors — сводятся медианным разрезом к maxColors.
std::vector<Color> extract(const Grid<Color>& g, int maxColors);

/// Картинка-палитра: до MAX_PALETTE точек — каждая точка ячейка (так
/// устроены палитры Lospec и Palitra.png); больше — извлечение цветов.
std::vector<Color> fromImage(const Grid<Color>& g);

/// Номер ближайшего цвета палитры по RGB или -1.
int nearest(const std::vector<Color>& p, Color c);

// --- файлы палитр -------------------------------------------------------------

/// GIMP .gpl, строки «R G B имя».
bool parseGpl(const std::string& text, std::vector<Color>& out, std::string& error);
std::string writeGpl(const std::vector<Color>& p, const std::string& name);
/// .hex (Lospec): строка — RRGGBB или AARRGGBB, «#» необязателен.
bool parseHex(const std::string& text, std::vector<Color>& out, std::string& error);
std::string writeHex(const std::vector<Color>& p);
/// JASC-PAL (Paint Shop Pro, Aseprite).
bool parseJasc(const std::string& text, std::vector<Color>& out, std::string& error);
std::string writeJasc(const std::vector<Color>& p);

}  // namespace tjtm::pal
