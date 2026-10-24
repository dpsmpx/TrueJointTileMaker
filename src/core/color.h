// Цвет и его преобразования: перенос TJTMColor.
//
// Модуль не знает ничего о состоянии редактора, только математику цвета.
// Функции пересчёта координаты в значение и обратно живут здесь же, чтобы
// отрисовка полос выбора цвета и разбор щелчков по ним пользовались одной
// формулой и не могли разъехаться — как и в оригинале.
#pragma once

#include <cstdint>
#include <vector>

namespace tjtm {

/// Цвет ARGB, по байту на канал, альфа прямая (не домноженная) — так
/// хранит System.Drawing.Color, которым пользовался оригинал.
struct Color {
    std::uint8_t a = 0, r = 0, g = 0, b = 0;

    constexpr Color() = default;
    constexpr Color(std::uint8_t aa, std::uint8_t rr, std::uint8_t gg, std::uint8_t bb)
        : a(aa), r(rr), g(gg), b(bb) {}

    /// Значение ToArgb: 0xAARRGGBB.
    constexpr std::uint32_t argb() const {
        return (std::uint32_t{a} << 24) | (std::uint32_t{r} << 16) | (std::uint32_t{g} << 8) |
               std::uint32_t{b};
    }
    static constexpr Color fromArgb(std::uint32_t v) {
        return Color(static_cast<std::uint8_t>(v >> 24), static_cast<std::uint8_t>(v >> 16),
                     static_cast<std::uint8_t>(v >> 8), static_cast<std::uint8_t>(v));
    }

    /// Сравнение по значению ARGB — SameColor оригинала. Штатное «=» для
    /// Color в .NET различало именованные цвета; здесь такого понятия нет.
    friend constexpr bool operator==(Color x, Color y) { return x.argb() == y.argb(); }
    friend constexpr bool operator!=(Color x, Color y) { return !(x == y); }
};

/// Непрозрачный цвет: RGB(r, g, b) из GraphABC.
constexpr Color rgb(int r, int g, int b) {
    return Color(255, static_cast<std::uint8_t>(r), static_cast<std::uint8_t>(g),
                 static_cast<std::uint8_t>(b));
}

/// ARGB(a, r, g, b) из GraphABC.
constexpr Color argb(int a, int r, int g, int b) {
    return Color(static_cast<std::uint8_t>(a), static_cast<std::uint8_t>(r),
                 static_cast<std::uint8_t>(g), static_cast<std::uint8_t>(b));
}

/// Цвет в модели HSV: H 0..359, S и V 0..100.
struct HSVColor {
    int h = 0, s = 0, v = 0;
};

bool sameColor(Color a, Color b);
/// Совпадение цвета без учёта прозрачности.
bool sameRGB(Color a, Color b);

/// HSV -> RGB. h приводится к 0..359, s и v ограничиваются 0..100, альфа 255.
Color hsvToRgb(int h, int s, int v);
/// RGB -> HSV. Возвращаемый H всегда в 0..359.
HSVColor rgbToHsv(int r, int g, int b);
HSVColor colorToHsv(Color c);

/// Наложение src поверх dst по правилу source-over с альфой обоих цветов.
Color blendOver(Color src, Color dst);

/// Координата вдоль оси (0..size-1) -> процент 0..100.
int axisToPercent(int pos, int size);
int percentToAxis(int percent, int size);
/// Вертикаль -> яркость: вверху ярко, внизу темно.
int axisToValue(int pos, int size);
int valueToAxis(int v, int size);
int axisToHue(int pos, int size);
int hueToAxis(int h, int size);
int axisToByte(int pos, int size);
int byteToAxis(int v, int size);

/// Полосы выбора цвета. Буфер — BGRA по 4 байта на пиксель, высота
/// выводится из длины буфера, как в оригинале.
void renderHSVPlane(std::vector<std::uint8_t>& bytes, int width, int tone);
void renderHuePlane(std::vector<std::uint8_t>& bytes, int width);
void renderAlphaPlane(std::vector<std::uint8_t>& bytes, int width);

}  // namespace tjtm
