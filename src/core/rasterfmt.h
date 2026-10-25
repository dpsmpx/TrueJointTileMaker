// BMP и TIFF без сторонних библиотек (этап 5 RoadToProLevel, раздел 17).
//
// BMP читает SDL_image; здесь — запись: непрозрачная картинка — 24 бита,
// с прозрачностью — 32 бита с заголовком V4 и маской альфы, который
// понимают и Windows, и GIMP.
//
// TIFF — свой и в обе стороны: SDL_image собран без libtiff, а тащить её
// ради одного формата на Windows незачем. Чтение берёт то, что на деле
// встречается у картинок: полосы и плитки; без сжатия, LZW, Deflate и
// PackBits; горизонтальный предсказатель; серый, палитру, RGB и RGBA по
// 1–16 бит; прозрачность прямую и умноженную. Страницы — кадры. Запись —
// RGB или RGBA по 8 бит, Deflate с предсказателем: так сохраняют GIMP и
// Photoshop, и такие файлы открывает всё.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include "core/color.h"
#include "core/grid.h"

namespace tjtm {

std::vector<std::uint8_t> bmpEncode(const Grid<Color>& img);

bool isTiff(const std::vector<std::uint8_t>& file);
/// Страницы TIFF. false и текст ошибки, если файл испорчен или его
/// разновидность не поддержана (JPEG внутри, CMYK, YCbCr).
bool tiffDecode(const std::vector<std::uint8_t>& file, std::vector<Grid<Color>>& pages,
                std::string& error);
/// Страницы одного размера или разного — каждая со своим заголовком.
std::vector<std::uint8_t> tiffEncode(const std::vector<Grid<Color>>& pages);

/// LZW по правилам TIFF (старший бит первым, ширина кода растёт на код
/// раньше, чем у GIF). Отдельно — для тестов.
std::vector<std::uint8_t> tiffLzwEncode(const std::vector<std::uint8_t>& data);
bool tiffLzwDecode(const std::uint8_t* data, std::size_t n, std::vector<std::uint8_t>& out,
                   std::size_t expected);

}  // namespace tjtm
