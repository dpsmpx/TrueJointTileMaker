// Слой-тайлкарта (этап 6 RoadToProLevel, раздел 21 плана): картинка из
// клеток, в каждой — номер тайла из набора. Как tilemap-слои Aseprite:
// правка тайла меняет все его копии, а одинаковые куски хранятся раз.
//
// Тайл 0 — пустой: клетка без тайла прозрачна. Клетки идут от левого
// верхнего угла документа; крайние, не влезшие целиком, обрезаются.
#pragma once

#include "core/grid.h"
#include "core/sprite.h"

namespace tjtm::tmap {

/// Число клеток по стороне документа w x h для тайла tw x th.
int cols(int w, int tw);
int rows(int h, int th);

/// Отрисовка карты номеров в картинку w x h.
Grid<Color> render(const Tileset& ts, const Grid<int>& map, int w, int h);
/// Перерисовать одну клетку отрисовки.
void renderCell(const Tileset& ts, const Grid<int>& map, int cx, int cy, Grid<Color>& img);

/// Номер такого же тайла в наборе или новый в конце. Пустая (прозрачная)
/// клетка — тайл 0.
int findOrAdd(Tileset& ts, const Grid<Color>& tile);
/// Вырезать клетку (cx, cy) картинки как тайл набора.
Grid<Color> cellImage(const Grid<Color>& img, int cx, int cy, int tw, int th);

/// Картинка → карта номеров: одинаковые клетки — один тайл (набор
/// пополняется).
Grid<int> slice(const Grid<Color>& img, Tileset& ts);

/// Тайл стал пустым у всех ссылок — номера после него сдвигаются.
void removeTile(Tileset& ts, int index);

}  // namespace tjtm::tmap
