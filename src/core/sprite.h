// Документ спрайта: слои × кадры (этапы 3 и 4 RoadToProLevel).
//
// Как у Aseprite: слой — полоса во времени, кадр — момент, ячейка — картинка
// слоя в кадре. Картинка ячейки неизменяема и общая (shared_ptr): снимок
// истории копирует таблицу указателей, а не картинки, и правка одной
// ячейки не множит в памяти остальные.
//
// Картинка активной ячейки живёт в Model::tile — с ней работают все
// инструменты, как в 0.6.0. Здесь её копия может отставать, пока ячейку
// не сменили (Model::commitActiveCel).
#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "core/color.h"
#include "core/grid.h"

namespace tjtm {

/// Номер ячейки палитры. Шире байта оригинала: палитра этапа 2 может быть
/// длиннее 255 цветов.
using PalIndex = std::uint16_t;
/// Пиксель не связан ни с одной ячейкой палитры (IDX_NONE оригинала).
inline constexpr PalIndex IDX_NONE = 0xFFFF;

// --- этап 6: векторные фигуры и наборы тайлов -------------------------------

/// Вид векторной фигуры: ломаная, многоугольник, кривая через точки
/// (открытая и замкнутая), прямоугольник и овал по двум углам, звезда по
/// центру и вершине луча.
inline constexpr int VS_POLYLINE = 0, VS_POLYGON = 1, VS_CURVE = 2, VS_CLOSED_CURVE = 3,
                     VS_RECT = 4, VS_ELLIPSE = 5, VS_STAR = 6, VS_COUNT = 7;

struct VecPoint {
    double x = 0;
    double y = 0;
    bool operator==(const VecPoint&) const = default;
};

/// Фигура векторного слоя: остаётся правимой и ложится в точки при
/// каждом изменении. Точки — в координатах документа, центр точки (x, y) —
/// (x + ½, y + ½).
struct VecShape {
    int type = VS_POLYLINE;
    std::vector<VecPoint> pts;
    Color stroke = argb(255, 0, 0, 0);
    /// Прозрачная заливка — фигура без заливки.
    Color fill = argb(0, 0, 0, 0);
    int width = 1;
    int starPoints = 5;
    /// Внутренний радиус звезды в процентах от внешнего.
    int innerPct = 50;
    /// Сглаживание края (этап 8); по умолчанию — чистые точки.
    bool smooth = false;
    bool operator==(const VecShape&) const = default;
};

/// Набор тайлов слоя-тайлкарты. Тайл 0 — пустой: клетка без тайла.
struct Tileset {
    std::string name;
    int tw = 16;
    int th = 16;
    std::vector<Grid<Color>> tiles;
};
using TilesetPtr = std::shared_ptr<const Tileset>;

struct Cel {
    Grid<Color> img;
    Grid<PalIndex> idx;
    /// Векторный слой: фигуры; img — их растр, всегда в согласии с ними.
    std::vector<VecShape> shapes;
    /// Слой-тайлкарта: номера тайлов по клеткам; img — их отрисовка.
    Grid<int> tiles;
};
using CelPtr = std::shared_ptr<const Cel>;

/// Вид слоя: растр, группа, вектор, тайлкарта.
inline constexpr int LK_RASTER = 0, LK_GROUP = 1, LK_VECTOR = 2, LK_TILEMAP = 3;

struct Layer {
    std::string name;
    bool visible = true;
    /// Запертый слой не правится: кисть, заливка и команды его не трогают.
    bool locked = false;
    int opacity = 255;
    int kind = LK_RASTER;
    /// Вложенность (этап 6). Слой глубины d > 0 — в группе: ближайшем выше
    /// него слое глубины d - 1. Дети группы лежат сразу под ней сплошным
    /// рядом — так список остаётся плоским, а дерево из него читается.
    int depth = 0;
    /// Свёрнутая группа прячет на ленте своих детей.
    bool collapsed = false;
    /// Слой-тайлкарта: номер набора в SpriteDoc::tilesets.
    int tileset = -1;
};

struct Frame {
    int durationMs = 100;
};

/// Направление проигрывания метки.
inline constexpr int TAG_FORWARD = 0, TAG_REVERSE = 1, TAG_PINGPONG = 2;

/// Метка анимации: именованный отрезок кадров («бег», «прыжок»).
struct AnimTag {
    std::string name;
    int from = 0;
    int to = 0;
    int direction = TAG_FORWARD;
};

struct SpriteDoc {
    /// Снизу вверх: слой 0 рисуется первым.
    std::vector<Layer> layers;
    std::vector<Frame> frames;
    /// [слой][кадр]; nullptr — пустая ячейка.
    std::vector<std::vector<CelPtr>> cels;
    std::vector<AnimTag> tags;
    /// Наборы тайлов слоёв-тайлкарт; общие и неизменяемые, как ячейки.
    std::vector<TilesetPtr> tilesets;

    int layerCount() const { return static_cast<int>(layers.size()); }
    int frameCount() const { return static_cast<int>(frames.size()); }
    CelPtr cel(int l, int f) const {
        if (l < 0 || f < 0 || l >= layerCount() || f >= frameCount()) return nullptr;
        return cels[static_cast<std::size_t>(l)][static_cast<std::size_t>(f)];
    }
    void setCel(int l, int f, CelPtr c) {
        if (l < 0 || f < 0 || l >= layerCount() || f >= frameCount()) return;
        cels[static_cast<std::size_t>(l)][static_cast<std::size_t>(f)] = std::move(c);
    }
};

}  // namespace tjtm
