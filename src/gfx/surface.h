// Программный кадр и примитивы рисования в духе GraphABC.
//
// Оригинал рисовал всё в буфер GraphABC (LockDrawing) и выводил его на экран
// разом (Redraw). Здесь то же: кадр — массив ARGB в памяти, примитивы пишут
// в него, окно показывает его целиком. Снимок кадра поэтому одинаков что в
// окне, что без экрана, и тесты сравнивают его попиксельно.
//
// Координаты — как в GraphABC: прямоугольник (x1, y1, x2, y2) закрывает
// точки от x1 до x2 - 1. Полупрозрачный цвет накладывается поверх, как
// кисть GDI+. Скругления сглажены по той же геометрии GraphicsPath, что
// строила GraphABC.
#pragma once

#include <cstdint>
#include <vector>

#include "core/color.h"
#include "core/grid.h"

namespace tjtm::gfx {

struct Rect {
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0;
    [[nodiscard]] bool empty() const { return x1 <= x0 || y1 <= y0; }
};

class Surface {
public:
    Surface() = default;
    Surface(int w, int h, Color fill = Color(255, 0, 0, 0));

    void resize(int w, int h, Color fill = Color(255, 0, 0, 0));
    [[nodiscard]] int width() const { return w_; }
    [[nodiscard]] int height() const { return h_; }
    /// Пиксели построчно, 0xAARRGGBB.
    [[nodiscard]] const std::uint32_t* data() const { return px_.data(); }
    std::uint32_t* data() { return px_.data(); }
    [[nodiscard]] Color pixel(int x, int y) const;
    void setPixel(int x, int y, Color c);

    /// Ограничение рисования прямоугольником; пустой — без ограничения.
    void setClip(Rect r);
    void resetClip();
    /// Текущее ограничение; пустое, если его нет.
    [[nodiscard]] Rect clip() const { return clipOn_ ? clip_ : Rect{}; }

    /// Залить весь кадр (ClearWindow).
    void clear(Color c);
    /// FillRect(x1, y1, x2, y2): точки x1..x2-1, y1..y2-1; концы можно
    /// перепутать — они упорядочиваются, как в GraphABC.
    void fillRect(int x1, int y1, int x2, int y2, Color c);
    /// Рамка из четырёх полосок толщиной в точку (FrameRect оригинала).
    void frameRect(int x0, int y0, int x1, int y1, Color c);
    /// FillRoundRect(x1, y1, x2, y2, w, h) из GraphABC: контур GraphicsPath
    /// с дугами w x h, сглаженный. Как и у GraphABC, правый и нижний край
    /// фигуры — x2 - 1 и y2 - 1.
    void fillRoundRect(int x1, int y1, int x2, int y2, int rw, int rh, Color c);
    /// Обводка того же контура толщиной width, без заливки: кнопке с рамкой
    /// в новой теме поле не нужно, под ней видно то, на чём она лежит.
    void strokeRoundRect(int x1, int y1, int x2, int y2, int rw, int rh, Color c, int width = 1);
    /// Линия толщиной в точку. Оригинал рисовал ими только сетку —
    /// вертикали и горизонтали, — им и место здесь.
    void line(int x1, int y1, int x2, int y2, Color c);
    /// Рамка пунктиром: штрих dash точек, промежуток dash точек, фаза
    /// сдвигает узор вдоль обхода по часовой стрелке («муравьи»).
    void dashedRect(int x0, int y0, int x1, int y1, Color on, int dash, int phase);

    /// Картинка с прямой альфой, растянутая ближайшим соседом в (x, y, w, h)
    /// и наложенная поверх. Пиксель-арт не сглаживается никогда.
    void drawImage(const Grid<Color>& img, int x, int y, int w, int h);
    void drawImage(const Grid<Color>& img, int x, int y) { drawImage(img, x, y, img.w, img.h); }
    /// То же для картинки в формате BGRA (как буферы оригинала).
    void drawBgra(const std::vector<std::uint8_t>& raw, int srcW, int srcH, int x, int y, int w,
                  int h);

    /// Наложение одной точки поверх (с учётом ограничения).
    void blendPixel(int x, int y, Color c);
    /// Наложение с покрытием 0..255 (сглаживание краёв).
    void blendPixelCoverage(int x, int y, Color c, int coverage);

    /// Копия кадра как картинка (снимок).
    [[nodiscard]] Grid<Color> toGrid() const;

private:
    int w_ = 0;
    int h_ = 0;
    std::vector<std::uint32_t> px_;
    Rect clip_{};
    bool clipOn_ = false;

    [[nodiscard]] Rect bounds() const;
};

}  // namespace tjtm::gfx
