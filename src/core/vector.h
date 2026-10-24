// Векторные фигуры (этап 6 RoadToProLevel, раздел 24 плана): растр и
// попадание мышью. Без состояния редактора: Model держит фигуры, здесь —
// как они ложатся в точки.
//
// Фигура ложится чистыми точками, как карандаш: контур — отрезками
// Брезенхэма между точками пути с уборкой углов «Г» (пиксель-перфект),
// заливка — по центрам точек правилом чёт-нечет. Овал — по уравнению
// внутри и кольцо края по четырём соседям: так у пиксель-арта и рисуют
// окружности. Кривая через точки — сплайн Катмулла — Рома: она проходит
// через каждую поставленную точку, и править её проще, чем рычаги Безье.
#pragma once

#include <vector>

#include "core/grid.h"
#include "core/sprite.h"

namespace tjtm::vec {

/// Путь фигуры ломаной: кривые разбиты на отрезки короче точки. closed —
/// путь замкнут.
std::vector<VecPoint> shapePath(const VecShape& s, bool& closed);

/// Фигура поверх картинки: заливка, затем контур.
void rasterize(const VecShape& s, Grid<Color>& img);
/// Все фигуры по порядку на прозрачном холсте w x h.
Grid<Color> rasterizeAll(const std::vector<VecShape>& shapes, int w, int h);

/// Ближайшая вершина в радиусе r: false — нет такой. Перебор с последней
/// фигуры: она лежит сверху.
bool hitVertex(const std::vector<VecShape>& shapes, double x, double y, double r, int& shape,
               int& vertex);
/// Фигура, чей путь проходит в радиусе r от точки или чья заливка её
/// накрывает; -1 — никакой.
int hitShape(const std::vector<VecShape>& shapes, double x, double y, double r);

/// Сдвиг всех точек фигуры.
void translate(VecShape& s, double dx, double dy);

std::string shapeName(int type);

}  // namespace tjtm::vec
