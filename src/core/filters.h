// Вне пиксель-арта (этап 8 RoadToProLevel, раздел 24 плана — «что я бы НЕ
// добавлял»): размытие, резкость, шум, края, цветокоррекция, кривые,
// плавные преобразования, мягкая кисть.
//
// Всё это даёт новые цвета и полупрозрачность — то, от чего пиксель-арт
// бережётся. Поэтому здесь отдельный модуль, а в окне — отдельный экран с
// пометкой; в строгом индексном режиме Model сводит итог к палитре.
//
// Цвета смешиваются с учётом прозрачности (premultiplied): иначе цвет
// прозрачных точек, которого не видно, протёк бы в край размытия.
// Случайность шума — хэш координат и зерна, а не генератор библиотеки:
// итог одинаков на всех платформах, и эталонные снимки не плывут.
#pragma once

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

#include "core/color.h"
#include "core/grid.h"

namespace tjtm::fx {

// --- свёртки ------------------------------------------------------------------

/// Размытие по Гауссу радиусом radius (сигма — половина радиуса). wrap — край
/// продолжается с другой стороны: размытый тайл остаётся бесшовным.
Grid<Color> blur(const Grid<Color>& g, double radius, bool wrap);
/// Нерезкая маска: к точке добавляется amountPct процентов её отличия от
/// размытой копии. Прозрачность не меняется.
Grid<Color> sharpen(const Grid<Color>& g, int amountPct, double radius, bool wrap);
/// Края по Собелу: светлое — где цвет резко меняется.
Grid<Color> edges(const Grid<Color>& g, bool wrap);
/// Тиснение: свет слева сверху, плоское — серое.
Grid<Color> emboss(const Grid<Color>& g, bool wrap);

// --- точечные --------------------------------------------------------------------

/// Шум до ±amount по каналам; mono — одинаковый для всех каналов точки.
Grid<Color> noise(const Grid<Color>& g, int amount, bool mono, std::uint32_t seed);
/// Яркость по формуле BT.601, как у pal::luma.
Grid<Color> grayscale(const Grid<Color>& g);
Grid<Color> sepia(const Grid<Color>& g);
Grid<Color> invertColors(const Grid<Color>& g);
/// brightness, contrast и saturation — от −100 до 100, ноль — без изменений.
Grid<Color> brightnessContrast(const Grid<Color>& g, int brightness, int contrast, int saturation);

struct Levels {
    int inLo = 0;
    int inHi = 255;
    /// Гамма средних тонов: больше 1 — светлее.
    double gamma = 1.0;
    int outLo = 0;
    int outHi = 255;
};
std::array<std::uint8_t, 256> levelsLut(const Levels& l);
Grid<Color> levels(const Grid<Color>& g, const Levels& l);

/// Кривая — опорные точки (вход, выход) от 0 до 255 по возрастанию входа.
/// Между ними — монотонный кубический сплайн (Фритч — Карлсон): кривая не
/// выбрасывает лишних горбов, которых не ставили.
using Curve = std::vector<std::pair<int, int>>;
std::array<std::uint8_t, 256> curveLut(const Curve& c);
/// Общая кривая, затем кривые каналов.
Grid<Color> curves(const Grid<Color>& g, const Curve& all, const Curve& r, const Curve& gr,
                   const Curve& b);
/// Сколько точек каждой яркости — для гистограммы уровней.
std::array<int, 256> histogram(const Grid<Color>& g);

// --- плавные преобразования --------------------------------------------------

/// Цвет в точке (x, y) — билинейно между четырьмя соседями; центры точек —
/// в (i + 0.5, j + 0.5). За краем — прозрачно или, с wrap, по кругу.
Color sampleBilinear(const Grid<Color>& g, double x, double y, bool wrap);
/// Плавный масштаб: при уменьшении — средним по площади, при увеличении —
/// билинейно.
Grid<Color> scaleSmooth(const Grid<Color>& g, int nw, int nh);
/// Поворот по часовой с билинейной выборкой; expand — холст до
/// описанного прямоугольника, как у img::rotateAny.
Grid<Color> rotateSmooth(const Grid<Color>& g, double degrees, bool expand);
/// Наклон вокруг середины: по горизонтали на degX, по вертикали на degY.
Grid<Color> skew(const Grid<Color>& g, double degX, double degY, bool wrap);

/// Четырёхугольник — углы прямоугольника картинки после перспективы:
/// левый верхний, правый верхний, правый нижний, левый нижний.
struct Quad {
    std::array<double, 4> x{};
    std::array<double, 4> y{};
};
Quad identityQuad(int w, int h);
/// Перспектива: прямоугольник картинки встаёт в четырёхугольник q.
Grid<Color> perspective(const Grid<Color>& g, const Quad& q);

// --- мягкая кисть ----------------------------------------------------------------

/// Отпечаток аэрографа диаметром size: непрозрачность 0..1, жёсткость
/// hardnessPct — доля радиуса со сплошной серединой, дальше — плавный спад.
Grid<float> softDab(int size, int hardnessPct);

}  // namespace tjtm::fx
