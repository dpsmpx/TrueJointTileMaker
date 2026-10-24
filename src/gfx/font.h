// Текст: SDL3_ttf поверх программного кадра, по правилам GraphABC.
//
// Оригинал писал шрифтом Arial 10 пунктов через GDI+. Здесь тот же размер
// при тех же 96 точках на дюйм; на Windows берётся сам Arial, в остальных
// системах — Liberation Sans из Resources/Fonts, у которого те же метрики:
// строки занимают ту же ширину, и раскладка экранов не плывёт.
//
// Строку раскладывает сам Font, знак за знаком, по целым ширинам знаков:
// так её мерил GraphABC, и по тем же ширинам она рисуется — центровка и
// перенос по словам выходят те же, что в оригинале. Сглаживание букв у
// FreeType и GDI+ разное, поэтому подписи с эталоном сравниваются по месту,
// а не попиксельно.
#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

#include "core/color.h"
#include "gfx/surface.h"

struct TTF_Font;

namespace tjtm::gfx {

class Font {
public:
    Font() = default;
    ~Font();
    Font(const Font&) = delete;
    Font& operator=(const Font&) = delete;

    /// Размер в пунктах при dpi точек на дюйм — так GDI считает Arial 10.
    bool open(const std::string& path, float points, int dpi = 96);
    [[nodiscard]] bool ok() const { return font_ != nullptr; }
    [[nodiscard]] const std::string& path() const { return path_; }

    /// Ширина строки по набору знаков, без запаса.
    int rawWidth(const std::string& s) const;
    /// TextWidth из GraphABC: ширина плюс точка («без неё рисует, занимая
    /// одну лишнюю точку», — сказано там же).
    int textWidth(const std::string& s) const { return rawWidth(s) + 1; }
    /// TextHeight из GraphABC: высота строки.
    int textHeight() const { return lineSkip_; }
    int ascent() const { return ascent_; }

    /// TextOut: левый верхний угол строки в (x, y).
    void draw(Surface& dst, int x, int y, const std::string& s, Color c) const;
    /// DrawTextCentered(x0, y0, x1, y1, s): по центру прямоугольника, с
    /// переносом по словам, если строка шире. Что не влезло по высоте —
    /// обрезается краем прямоугольника, как у GDI+.
    void drawCentered(Surface& dst, int x0, int y0, int x1, int y1, const std::string& s,
                      Color c) const;
    /// Раскладка строк DrawTextCentered — как у GDI+ в прямоугольнике.
    std::vector<std::string> layoutBox(const std::string& s, int maxWidth) const;

private:
    struct Glyph {
        int adv = 0;   // ширина знака, целая
        int minx = 0;  // левый край картинки от пера
        int maxy = 0;  // верх картинки над базовой линией
        int w = 0, h = 0;
        std::vector<std::uint8_t> a;  // покрытие 0..255
    };
    const Glyph& glyph(std::uint32_t ch) const;

    TTF_Font* font_ = nullptr;
    std::string path_;
    int lineSkip_ = 15;
    int ascent_ = 12;
    int height_ = 15;
    // Знаки, а не строки: подписи собираются из одних и тех же букв, и
    // растеризовать их заново незачем.
    mutable std::unordered_map<std::uint32_t, Glyph> glyphs_;
};

/// Путь к шрифту интерфейса: Arial на Windows, иначе Liberation Sans из
/// ресурсов или системы. Пусто — не найден.
std::string findUiFont(const std::string& resourcesDir);
/// Моноширинный для поля ввода: Courier New на Windows, иначе Liberation Mono.
std::string findMonoFont(const std::string& resourcesDir);

}  // namespace tjtm::gfx
