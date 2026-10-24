// Цвета интерфейса оригинала.
//
// Оттенки уведены в холодную сторону и разведены по светлоте: панель,
// кнопка и фон иначе сливались, а рамка, тёмнее подписи, перетягивала
// взгляд. Холст нейтрально серый: любой оттенок вокруг картинки врёт о
// её собственных цветах. Фон страницы и поле кнопки — разные тона нарочно,
// иначе ряд кнопок читается линейкой, а не рядом нажимаемых мест.
#pragma once

#include "core/color.h"

namespace tjtm::theme {

inline constexpr Color cWindow = rgb(240, 242, 246);  // фон окна: страница
inline constexpr Color cBack = rgb(253, 253, 254);    // поле кнопки: карточка
inline constexpr Color cFace = rgb(237, 239, 243);  // выключенное, подложки полей
inline constexpr Color cHover = rgb(231, 240, 253);  // под курсором — холодный отсвет
inline constexpr Color cBorder = rgb(205, 210, 218);  // обводка тише подписи
inline constexpr Color cText = rgb(28, 32, 38);
inline constexpr Color cTextDim = rgb(122, 130, 141);
inline constexpr Color cPanel = rgb(243, 245, 248);   // подложка панели
inline constexpr Color cCanvas = rgb(120, 122, 128);  // вокруг холста
inline constexpr Color cCheckA = rgb(214, 214, 218);
inline constexpr Color cCheckB = rgb(184, 184, 190);
inline constexpr Color cAccent = rgb(36, 104, 216);
inline constexpr Color cWarn = rgb(198, 58, 46);
/// Тень под кнопкой и тонкие разделители — отдельные цвета, не оттенки
/// обводки: обводка очерчивает, тень отделяет, линия делит.
inline constexpr Color cShadow = rgb(219, 222, 229);
inline constexpr Color cLine = rgb(230, 233, 238);

/// Скругление углов: одно на кнопки, поля и подложки.
inline constexpr int BTN_R = 7;

}  // namespace tjtm::theme
