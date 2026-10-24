// Связь с игрой Andors-Love (этап 7 RoadToProLevel, раздел 21 плана):
// лист 8x8 в формате игры, атлас всего набора, стыки для автотайлинга.
//
// Лист 8x8 — договор с игрой: src/gfx/tiles.cpp режет его функцией
// tjtm_slice. Фон листа — служебный цвет (1, 2, 3, 255), клетка — по
// самому большому тайлу плюс точка-разделитель, тайл — с левого верхнего
// угла клетки; край тайла ищут по маркеру вправо по верхней строке и вниз
// по левому столбцу. Поэтому тайл не больше 63 точек по стороне, а маркер
// сравнивается по всем четырём каналам — прозрачная точка его не спутает.
//
// Атлас — все картинки папки набора одним листом с описанием в виде
// TexturePacker (JSON hash): «имя → прямоугольник». Его читают Phaser,
// Godot, LÖVE-библиотеки; игре он пригодится, когда тайлов станет много.
//
// Стыки — 16 вариантов тайла местности по соседям той же местности:
// север 1, восток 2, юг 4, запад 8; за краем карты — та же местность
// (стена у края не обрывается). Вариант — файл «слот_маска.png», например
// wall_5.png — стена с соседями сверху и снизу. Редактор показывает их на
// карте и помогает нарисовать; игра читает только основной тайл, пока не
// выучит это правило.
#pragma once

#include <string>
#include <vector>

#include "core/color.h"
#include "core/grid.h"
#include "core/mapdoc.h"

namespace tjtm::game {

inline constexpr int kSheetCols = 8;
inline constexpr int kSheetRows = 8;
inline constexpr int kSheetSlots = kSheetCols * kSheetRows;
/// Больший тайл не влезает в лист 512 точек вместе с разделителем.
inline constexpr int kSheetMaxTile = 63;

bool isMarker(Color c);
/// Лист 8x8 из тайлов по слотам; пустая картинка — пустой слот.
bool composeSheet(const std::vector<Grid<Color>>& slots, Grid<Color>& out, std::string& error);
/// Лист 8x8 → 64 тайла по правилам игры.
bool sliceSheet(const Grid<Color>& sheet, std::vector<Grid<Color>>& slots, std::string& error);

struct AtlasEntry {
    std::string name;
    Grid<Color> image;
};
struct AtlasPlace {
    std::string name;
    int x = 0, y = 0, w = 0, h = 0;
};
/// Полки по высоте: картинки по убыванию высоты рядами не шире maxWidth.
/// padding — пустые точки между картинками и по краю.
Grid<Color> packAtlas(const std::vector<AtlasEntry>& entries, int maxWidth, int padding,
                      std::vector<AtlasPlace>& places);
/// Описание атласа в виде TexturePacker JSON hash.
std::string atlasJson(const std::vector<AtlasPlace>& places, const std::string& imageFile, int w,
                      int h);

/// Маска стыков клетки (x, y): соседи той же местности.
int autotileMask(const MapDoc& map, int x, int y);
std::string autotileName(const std::string& slot, int mask);
/// Заготовка: 16 вариантов сеткой 4x4, вариант маски k — в клетке k.
Grid<Color> autotileSheet(const std::vector<Grid<Color>>& variants, int tw, int th);
/// Сетка 4x4 → 16 вариантов.
std::vector<Grid<Color>> sliceAutotileSheet(const Grid<Color>& sheet);

}  // namespace tjtm::game
