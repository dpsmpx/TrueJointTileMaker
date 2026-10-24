// Экспорт анимации (этап 4 RoadToProLevel): лист спрайтов с описанием и
// подготовка кадров для GIF, APNG и отдельных PNG.
//
// Описание листа — JSON в виде, который выдаёт Aseprite («массив» кадров
// и frameTags в meta): его уже умеют читать Godot, Phaser, LÖVE-библиотеки
// и прочие движки, и игре не нужен свой разборщик.
#pragma once

#include <string>
#include <vector>

#include "core/png.h"
#include "core/sprite.h"

namespace tjtm {

/// Раскладка листа: строкой, столбцом или сеткой.
inline constexpr int SHEET_ROW = 0, SHEET_COLUMN = 1, SHEET_GRID = 2;

struct SheetOptions {
    int layout = SHEET_ROW;
    /// Столбцов сетки; 0 — почти квадрат (потолок корня из числа кадров).
    int columns = 0;
    /// Поле вокруг всего листа.
    int margin = 0;
    /// Промежуток между кадрами.
    int spacing = 0;
    /// Увеличение ближайшим соседом: пиксель-арт не размывается.
    int scale = 1;
    /// Отступ внутри клетки вокруг каждого кадра (этап 5): движки с
    /// фильтрацией текстур не тянут цвет соседнего кадра.
    int padding = 0;
    /// Заливка всего листа; прозрачная — по умолчанию.
    Color background = argb(0, 0, 0, 0);
};

/// Место кадра на листе — сама картинка, без внутреннего отступа.
struct SheetCell {
    int x = 0, y = 0, w = 0, h = 0;
    int durationMs = 100;
};

struct SpriteSheet {
    Grid<Color> image;
    std::vector<SheetCell> cells;
};

/// Число столбцов и строк сетки для n кадров.
void sheetGrid(int n, const SheetOptions& o, int& cols, int& rows);
SpriteSheet buildSheet(const std::vector<AnimFrame>& frames, const SheetOptions& o);

/// Описание листа. tags — метки уже в номерах выгружаемых кадров;
/// frameName — основа имён кадров («hero» → «hero 0.png» как у Aseprite).
std::string sheetJson(const SpriteSheet& s, const std::string& imageFile,
                      const std::string& frameName, const std::vector<AnimTag>& tags);

/// Кадры, увеличенные в scale раз.
std::vector<AnimFrame> scaleFrames(const std::vector<AnimFrame>& frames, int scale);

/// Строка JSON в кавычках со всеми нужными экранированиями.
std::string jsonString(const std::string& s);

}  // namespace tjtm
