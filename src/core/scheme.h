// Схема оформления из одного цвета-акцента (план «Интерфейс», D8, ДС-5).
//
// У 1.9 светлая и тёмная темы — таблицы, рассчитанные для одного акцента
// (#2468D8, docs/ui-design-system.md, приложение А). Здесь тот же расчёт
// сделан кодом: акцент даёт оттенок и цветность в OKLCH, а каждая роль —
// тон, то есть L* CIELAB. Разница тонов держит контраст при любом акценте:
// 40 — не меньше 3:1, 50 — не меньше 4,5:1. Так получаются схемы для
// девяти акцентов на выбор и для любого своего, контрастные темы и темы из
// файлов (Resources/Themes, «ключ = значение»).
#pragma once

#include <span>
#include <string>
#include <utility>
#include <vector>

#include "core/theme.h"

namespace tjtm::theme {

/// Акцент 1.9: при нём светлая и тёмная — прежние таблицы точка в точку.
inline constexpr Color kDefaultSeed = rgb(0x24, 0x68, 0xD8);

/// Вид схемы.
enum class Variant { Light, Dark, LightContrast, DarkContrast };

/// Акценты на выбор (приложение Б): как набор GNOME.
struct Accent {
    const char* name = "";
    Color seed;
};
std::span<const Accent> accents();

/// Цвет с тоном tone (L* CIELAB, 0..100) и оттенком seed; цветность —
/// доля chroma от цветности seed, урезанная до границы sRGB.
Color toneOf(Color seed, double tone, double chroma = 1.0);
/// Нейтральный цвет того же оттенка: цветность c в OKLCH (0,006 — едва
/// холодный серый).
Color neutralTone(Color seed, double tone, double c);

/// Схема из акцента. При kDefaultSeed светлая и тёмная — lightTheme() и
/// darkTheme(): на них стоят эталоны.
Theme schemeTheme(Color seed, Variant v);

/// Контраст по WCAG 2.1: от 1 до 21.
double contrastRatio(Color a, Color b);

/// Встроенные темы по порядку экрана «Оформление»: «светлая», «тёмная»,
/// «светлая контрастная», «тёмная контрастная», «как в системе»,
/// «классика».
std::span<const char* const> builtinThemeNames();

/// Тема из файла: «ключ = значение», как Config.txt.
///   name = Сумерки
///   base = тёмная          # от какой встроенной
///   seed = #7445B4         # акцент
///   shape.corners = rounded | square | pixel
///   color.<роль> = #RRGGBB[AA]
struct ThemeFile {
    std::string name;
    std::string base = "светлая";
    Color seed = kDefaultSeed;
    bool hasSeed = false;
    std::vector<std::pair<std::string, std::string>> values;
    std::string path;
};
/// Разбирает текст файла темы. Ошибки (неизвестный ключ, не цвет) — в
/// problems, строка пропускается; без name файл не тема.
bool parseThemeFile(const std::string& text, ThemeFile& out, std::vector<std::string>& problems);

/// Собирает тему: встроенную по имени или из файла. seed — акцент из
/// настроек (у темы из файла свой seed главнее); systemDark — что сейчас у
/// системы, для «как в системе». Неизвестное имя — светлая.
Theme buildTheme(const std::string& name, Color seed, bool systemDark,
                 const std::vector<ThemeFile>& files);

/// «#RRGGBB» или «#RRGGBBAA» → цвет; false — не цвет.
bool parseHexColor(const std::string& s, Color& out);
/// Цвет → «#RRGGBB».
std::string hexColor(Color c);

}  // namespace tjtm::theme
