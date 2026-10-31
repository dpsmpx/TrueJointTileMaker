// Цвета и форма интерфейса — тема окна.
//
// Роли названы по смыслу, а не по цвету: «подпись на акценте» в другой теме
// может стать другой, а место в коде то же. Облик 1.6 — тема «Классика»: на
// ней сверяются сценарии оригинала (--classic), и её значения не меняются.
// «Светлая» (по умолчанию) и «Тёмная» — схемы Material 3 из одного акцента,
// рассчитанные и проверенные на контраст WCAG 2.1 в docs/ui-design-system.md
// (приложение А), с современным видом элементов (modern).
//
// Оттенки «Классики» уведены в холодную сторону и разведены по светлоте:
// панель, кнопка и фон иначе сливались, а рамка, тёмнее подписи,
// перетягивала взгляд. Холст нейтрально серый: любой оттенок вокруг картинки
// врёт о её собственных цветах. Фон страницы и поле кнопки — разные тона
// нарочно, иначе ряд кнопок читается линейкой, а не рядом нажимаемых мест.
#pragma once

#include <array>
#include <cstdint>
#include <string>

#include "core/color.h"

namespace tjtm::theme {

struct Theme {
    /// Имя в Config.txt: «классика», «светлая», «тёмная».
    const char* name;
    /// Вид элементов: кнопки по видам с прозрачным слоем состояния и
    /// выбранное — контейнером акцента (docs/ui-design-system.md,
    /// «Состояния»). false — как в 1.6.
    bool modern;

    // --- окно, кнопки, текст ---
    Color window;  // фон окна: страница
    Color back;    // поле кнопки: карточка
    Color face;    // выключенное, подложки полей
    Color hover;   // под курсором — холодный отсвет
    Color border;  // рамки подложек: тише подписи
    Color outline;  // обводка нажимаемого: кнопки, переключатели
    Color text;
    Color textDim;  // подсказки и вторичный текст
    Color panel;    // подложка панели
    Color accent;
    Color onAccent;  // подпись на залитом акцентом
    /// Выбранное: инструмент, сегмент, вкладка, включённый переключатель.
    Color accentContainer;
    Color onAccentContainer;
    /// Всплывающее поверх окна: подсказки и уведомления.
    Color inverseSurface;
    Color inverseOnSurface;
    /// Затемнение окна под диалогом.
    Color scrim;
    Color warn;
    /// Тень под кнопкой и тонкие разделители — отдельные цвета, не оттенки
    /// обводки: обводка очерчивает, тень отделяет, линия делит.
    Color shadow;
    Color line;
    /// Поле ввода и выделенный в нём текст.
    Color field;
    Color fieldSelection;
    /// Подложка подписи поверх картинки.
    Color noteBack;

    // --- холст ---
    Color canvas;  // вокруг холста
    Color checkA;
    Color checkB;
    /// Двойная линия, видная на любом фоне: «муравьи» выделения.
    Color antsLight;
    Color antsDark;
    /// Ручки фигур и углов перспективы.
    Color handleLight;
    Color handleDark;
    /// Непрозрачность подкраски выделения акцентом: тайла и карты.
    std::uint8_t selectionAlpha;
    std::uint8_t mapSelectionAlpha;
    /// Соседние копии бесшовного тайла притушены.
    Color seamDim;
    Color symAxis;
    Color radialAxis;
    Color guide;
    Color guideCenter;
    Color majorGrid;
    Color cellCenter;
    Color rulerMark;
    /// Объект карты без картинки: подложка и знак.
    Color mapLabelBack;
    Color mapLabelText;

    // --- панель ---
    /// Метки на квадрате цвета, полосе тона и полосе прозрачности.
    Color barMarker;
    Color alphaMarker;
    /// Рамка видимой части на обзоре.
    Color viewport;
    /// Подпись местности на тёмной и на светлой клетке.
    Color labelOnDark;
    Color labelOnLight;
    /// Ячейка палитры, из которой взят первый цвет.
    Color cellMark;

    // --- экраны ---
    /// Поле кривых и цвета каналов: общая, красный, зелёный, синий.
    Color curveBack;
    std::array<Color, 4> curveChannel;
    /// Метки анимации на ленте: различимы между собой, и тёмная подпись на
    /// них читается.
    std::array<Color, 6> tag;

    /// Скругление углов: одно на кнопки, поля и подложки.
    int radius;
};

/// Облик 1.6 точка в точку.
constexpr Theme classicTheme() {
    Theme t{};
    t.name = "классика";
    t.modern = false;
    t.window = rgb(240, 242, 246);
    t.back = rgb(253, 253, 254);
    t.face = rgb(237, 239, 243);
    t.hover = rgb(231, 240, 253);
    t.border = rgb(205, 210, 218);
    t.outline = rgb(205, 210, 218);
    t.text = rgb(28, 32, 38);
    t.textDim = rgb(122, 130, 141);
    t.panel = rgb(243, 245, 248);
    t.accent = rgb(36, 104, 216);
    t.onAccent = rgb(255, 255, 255);
    t.accentContainer = rgb(231, 240, 253);
    t.onAccentContainer = rgb(28, 32, 38);
    t.inverseSurface = rgb(47, 49, 52);
    t.inverseOnSurface = rgb(238, 241, 245);
    t.scrim = argb(82, 0, 0, 0);
    t.warn = rgb(198, 58, 46);
    t.shadow = rgb(219, 222, 229);
    t.line = rgb(230, 233, 238);
    t.field = rgb(255, 255, 255);
    t.fieldSelection = rgb(173, 214, 255);
    t.noteBack = argb(200, 255, 255, 255);

    t.canvas = rgb(120, 122, 128);
    t.checkA = rgb(214, 214, 218);
    t.checkB = rgb(184, 184, 190);
    t.antsLight = rgb(245, 245, 250);
    t.antsDark = rgb(20, 20, 24);
    t.handleLight = rgb(250, 250, 252);
    t.handleDark = rgb(30, 30, 36);
    t.selectionAlpha = 64;
    t.mapSelectionAlpha = 80;
    t.seamDim = argb(56, 0, 0, 0);
    t.symAxis = argb(90, 220, 60, 60);
    t.radialAxis = argb(110, 220, 60, 60);
    t.guide = rgb(0, 168, 214);
    t.guideCenter = rgb(214, 0, 150);
    t.majorGrid = argb(170, 40, 70, 180);
    t.cellCenter = argb(150, 214, 0, 150);
    t.rulerMark = rgb(0, 140, 190);
    t.mapLabelBack = argb(210, 250, 250, 210);
    t.mapLabelText = rgb(20, 20, 30);

    t.barMarker = argb(200, 0, 0, 0);
    t.alphaMarker = argb(200, 220, 40, 40);
    t.viewport = rgb(250, 240, 80);
    t.labelOnDark = rgb(245, 245, 245);
    t.labelOnLight = rgb(20, 20, 24);
    t.cellMark = rgb(255, 255, 255);

    t.curveBack = rgb(250, 250, 252);
    t.curveChannel = {rgb(40, 40, 48), rgb(210, 50, 50), rgb(40, 160, 60), rgb(50, 90, 220)};
    t.tag = {rgb(255, 214, 102), rgb(140, 206, 255), rgb(170, 230, 150),
             rgb(255, 170, 190), rgb(205, 180, 255), rgb(255, 190, 130)};
    t.radius = 7;
    return t;
}

/// Смесь a и b: доля b — share из 255. Слой состояния и приглушённое.
constexpr Color mix(Color a, Color b, int share) {
    auto ch = [share](int x, int y) { return (x * (255 - share) + y * share + 127) / 255; };
    return rgb(ch(a.r, b.r), ch(a.g, b.g), ch(a.b, b.b));
}

/// Светлая схема Material 3 из акцента #2468D8 (ui-design-system.md,
/// приложение А): тон поверхностей вместо теней, серый холст без оттенка.
constexpr Theme lightTheme() {
    Theme t = classicTheme();
    t.name = "светлая";
    t.modern = true;
    t.window = rgb(0xF7, 0xF9, 0xFE);   // surface
    t.back = rgb(0xFF, 0xFF, 0xFF);     // surface_container_lowest
    t.panel = rgb(0xF1, 0xF4, 0xF8);    // surface_container_low
    t.face = rgb(0xE6, 0xE8, 0xEC);     // surface_container_high
    t.text = rgb(0x1A, 0x1B, 0x1E);     // on_surface
    t.textDim = rgb(0x43, 0x47, 0x4D);  // on_surface_variant
    t.outline = rgb(0x73, 0x78, 0x7F);  // outline
    t.border = rgb(0xC2, 0xC6, 0xCE);   // outline_variant
    t.line = rgb(0xDD, 0xE1, 0xE6);
    t.accent = rgb(0x12, 0x59, 0xC7);  // primary
    t.onAccent = rgb(0xFF, 0xFF, 0xFF);
    t.accentContainer = rgb(0xD3, 0xE3, 0xFF);
    t.onAccentContainer = rgb(0x00, 0x18, 0x47);
    t.inverseSurface = rgb(0x2F, 0x31, 0x34);
    t.inverseOnSurface = rgb(0xEE, 0xF1, 0xF5);
    t.scrim = argb(82, 0, 0, 0);
    t.warn = rgb(0xB1, 0x2C, 0x28);     // error
    t.hover = mix(t.back, t.text, 20);  // слой наведения 8 %
    t.shadow = t.window;                // высота — тоном, не тенью
    t.field = t.back;
    t.fieldSelection = t.accentContainer;
    t.noteBack = argb(220, 0xF7, 0xF9, 0xFE);
    t.canvas = rgb(0x77, 0x77, 0x77);
    t.checkA = rgb(0xD4, 0xD4, 0xD4);
    t.checkB = rgb(0xB9, 0xB9, 0xB9);
    t.curveBack = t.back;
    t.radius = 6;
    return t;
}

/// Тёмная схема той же системы. Холст темнее, но по-прежнему серый: тон
/// вокруг картинки не должен врать о её цветах.
constexpr Theme darkTheme() {
    Theme t = lightTheme();
    t.name = "тёмная";
    t.window = rgb(0x12, 0x14, 0x16);
    t.back = rgb(0x1E, 0x20, 0x22);  // surface_container: лицо кнопки
    t.panel = rgb(0x1A, 0x1B, 0x1E);
    t.face = rgb(0x28, 0x2A, 0x2D);
    t.text = rgb(0xE0, 0xE3, 0xE6);
    t.textDim = rgb(0xC2, 0xC6, 0xCE);
    t.outline = rgb(0x8D, 0x91, 0x98);
    t.border = rgb(0x43, 0x47, 0x4D);
    t.line = rgb(0x33, 0x34, 0x38);
    t.accent = rgb(0xA7, 0xC8, 0xFF);
    t.onAccent = rgb(0x00, 0x2C, 0x72);
    t.accentContainer = rgb(0x01, 0x41, 0xA0);
    t.onAccentContainer = rgb(0xD3, 0xE3, 0xFF);
    t.inverseSurface = rgb(0xE0, 0xE3, 0xE6);
    t.inverseOnSurface = rgb(0x2F, 0x31, 0x34);
    t.scrim = argb(120, 0, 0, 0);
    t.warn = rgb(0xFF, 0xB4, 0xAB);
    t.hover = mix(t.back, t.text, 20);
    t.shadow = t.window;
    t.field = rgb(0x0D, 0x0E, 0x11);  // surface_container_lowest
    t.fieldSelection = t.accentContainer;
    t.noteBack = argb(220, 0x12, 0x14, 0x16);
    t.canvas = rgb(0x35, 0x35, 0x35);
    t.checkA = rgb(0x5E, 0x5E, 0x5E);
    t.checkB = rgb(0x4B, 0x4B, 0x4B);
    t.curveBack = t.field;
    t.curveChannel[0] = t.text;  // общая кривая светлой линией
    t.mapLabelBack = argb(210, 0x28, 0x2A, 0x2D);
    t.mapLabelText = t.text;
    return t;
}

/// Тема по имени из Config.txt; неизвестное имя — светлая.
inline Theme themeByName(const std::string& name) {
    if (name == "классика") return classicTheme();
    if (name == "тёмная") return darkTheme();
    return lightTheme();
}

/// Тема окна. Ставится при запуске (Editor::appInit) и в настройках; до
/// того — «Классика», чтобы всё, что рисуется без окна, выглядело как в 1.6.
inline Theme active = classicTheme();

/// Короткие имена главных ролей: ими написаны все экраны.
inline const Color& cWindow = active.window;
inline const Color& cBack = active.back;
inline const Color& cFace = active.face;
inline const Color& cHover = active.hover;
inline const Color& cBorder = active.border;
inline const Color& cOutline = active.outline;
inline const Color& cText = active.text;
inline const Color& cTextDim = active.textDim;
inline const Color& cPanel = active.panel;
inline const Color& cCanvas = active.canvas;
inline const Color& cCheckA = active.checkA;
inline const Color& cCheckB = active.checkB;
inline const Color& cAccent = active.accent;
inline const Color& cWarn = active.warn;
inline const Color& cShadow = active.shadow;
inline const Color& cLine = active.line;
inline const int& BTN_R = active.radius;

/// Акцент с непрозрачностью a: подкраска выделения и подобного.
inline Color accentAlpha(std::uint8_t a) {
    return argb(a, active.accent.r, active.accent.g, active.accent.b);
}

}  // namespace tjtm::theme
