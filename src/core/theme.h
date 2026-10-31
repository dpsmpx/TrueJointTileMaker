// Цвета и форма интерфейса — тема окна.
//
// Роли названы по смыслу, а не по цвету: «подпись на акценте» в другой теме
// может стать другой, а место в коде то же. Нынешний облик — тема
// «Классика»: на ней сверяются сценарии оригинала (--classic), и её значения
// не меняются. Тема по умолчанию — «Светлая»: тот же облик, но подсказки,
// обводка кнопок и наведение — по контрасту WCAG 2.1 (docs/ui-ux-audit.md,
// UX-29; план тем — docs/ui-design-system.md).
//
// Оттенки «Классики» уведены в холодную сторону и разведены по светлоте:
// панель, кнопка и фон иначе сливались, а рамка, тёмнее подписи,
// перетягивала взгляд. Холст нейтрально серый: любой оттенок вокруг картинки
// врёт о её собственных цветах. Фон страницы и поле кнопки — разные тона
// нарочно, иначе ряд кнопок читается линейкой, а не рядом нажимаемых мест.
#pragma once

#include <array>
#include <cstdint>

#include "core/color.h"

namespace tjtm::theme {

struct Theme {
    /// Имя в Config.txt: «классика», «светлая».
    const char* name;

    // --- окно, кнопки, текст ---
    Color window;   // фон окна: страница
    Color back;     // поле кнопки: карточка
    Color face;     // выключенное, подложки полей
    Color hover;    // под курсором — холодный отсвет
    Color border;   // рамки подложек: тише подписи
    Color outline;  // обводка нажимаемого: кнопки, переключатели
    Color text;
    Color textDim;  // подсказки и вторичный текст
    Color panel;    // подложка панели
    Color accent;
    Color onAccent;  // подпись на залитом акцентом
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

/// «Классика» с контрастом по WCAG 2.1 (приложение А аудита): подсказки
/// 5:1 к фону окна вместо 3,5:1, обводка нажимаемого 3,2:1 вместо 1,4:1,
/// наведение заметнее. Рамки подложек остаются мягкими: они ничего не
/// нажимают, а тёмные они перетягивали бы взгляд с картинки.
constexpr Theme lightTheme() {
    Theme t = classicTheme();
    t.name = "светлая";
    t.textDim = rgb(96, 104, 116);
    t.outline = rgb(128, 136, 148);
    t.hover = rgb(219, 232, 252);
    return t;
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
