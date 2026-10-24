// PNG и APNG: чтение и запись без сторонних библиотек.
//
// Читается всё, что допускает формат: глубина 1, 2, 4, 8 и 16 бит, все пять
// типов цвета, прозрачность tRNS, чересстрочность Adam7. Результат — RGBA
// по байту на канал с прямой альфой, как Bitmap в 32bppARGB у оригинала.
// 16 бит сводятся к 8 старшим байтом. Гамма и цветовые профили не
// применяются: пиксель-арт хранит точные значения, и их надо получить как
// есть.
//
// Пишется 8-битный RGBA без чересстрочности — так сохранял тайлы GDI+.
// Фильтр строки выбирается по наименьшей сумме модулей, как в libpng.
//
// APNG — анимация поверх PNG: кадры читаются уже сведёнными на холст
// (с учётом способов очистки и наложения), а пишутся полными кадрами.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/color.h"
#include "core/grid.h"

namespace tjtm {

struct PngInfo {
    int width = 0;
    int height = 0;
    int bitDepth = 0;
    int colorType = 0;
    bool interlaced = false;
    /// Есть ли в файле анимация (кусок acTL).
    bool animated = false;
};

/// Читает PNG. false и текст ошибки, если файл испорчен или не PNG.
bool pngDecode(const std::vector<std::uint8_t>& file, Grid<Color>& out, std::string& error,
               PngInfo* info = nullptr);
/// Пишет 8-битный RGBA PNG.
std::vector<std::uint8_t> pngEncode(const Grid<Color>& img, int level = 9);

/// Свой кусок PNG: тип из четырёх латинских букв и данные.
struct PngChunk {
    std::string type;
    std::vector<std::uint8_t> data;
};
/// PNG с дополнительными кусками после картинки (перед IEND). Просмотрщики
/// пропускают незнакомые служебные куски, поэтому файл остаётся картинкой.
std::vector<std::uint8_t> pngEncodeWith(const Grid<Color>& img, const std::vector<PngChunk>& extra,
                                        int level = 9);
/// Данные первого куска такого типа с верной контрольной суммой.
bool pngFindChunk(const std::vector<std::uint8_t>& file, const std::string& type,
                  std::vector<std::uint8_t>& data);

bool pngLoad(const std::string& path, Grid<Color>& out, std::string& error);
bool pngSave(const std::string& path, const Grid<Color>& img, std::string& error);

/// Кадр анимации: картинка во весь холст и длительность в миллисекундах.
struct AnimFrame {
    Grid<Color> image;
    int durationMs = 100;
};

/// Читает APNG кадрами во весь холст. У обычного PNG — один кадр.
bool apngDecode(const std::vector<std::uint8_t>& file, std::vector<AnimFrame>& frames,
                std::string& error, int* loops = nullptr);
/// Пишет APNG. loops — сколько раз проиграть (num_plays), 0 — бесконечно.
std::vector<std::uint8_t> apngEncode(const std::vector<AnimFrame>& frames, int loops = 0,
                                     int level = 9);

}  // namespace tjtm
