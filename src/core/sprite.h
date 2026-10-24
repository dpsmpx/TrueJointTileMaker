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

struct Cel {
    Grid<Color> img;
    Grid<PalIndex> idx;
};
using CelPtr = std::shared_ptr<const Cel>;

struct Layer {
    std::string name;
    bool visible = true;
    /// Запертый слой не правится: кисть, заливка и команды его не трогают.
    bool locked = false;
    int opacity = 255;
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
