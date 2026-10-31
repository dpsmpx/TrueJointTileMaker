// Сочетания клавиш редактора одной таблицей (план «Интерфейс», этап B,
// UX-18 и UX-09). У 1.6 клавиши разбирали пять switch в четырёх файлах, а
// справка перечисляла их отдельно, от руки, и отставала. Теперь интерфейс
// 1.7+ разбирает клавиатуру по этой таблице, и из неё же строятся
// шпаргалка (F1, ?) и раздел клавиш в справке: что нажимается, то и
// написано. «Классика» (--classic) разбирает клавиши по-старому — на этом
// стоит сверка с оригиналом.
#pragma once

#include <span>
#include <string>

#include "core/keys.h"

namespace tjtm {

/// Модификаторы сочетания.
enum : int { MOD_SHIFT = 1, MOD_CTRL = 2, MOD_ALT = 4 };

/// В каком режиме работает сочетание.
enum class KeyWhere { Both, Tile, Map };

/// Команды, которые можно повесить на клавишу.
enum class Cmd {
    // Файл
    New,
    Open,
    Save,
    SaveAs,
    SaveSelection,
    // Правка
    Undo,
    Redo,
    PasteHere,
    EraseSelection,
    // Выделение
    SelectAll,
    Deselect,
    Reselect,
    InvertSelection,
    Copy,
    Cut,
    PasteClipboard,
    BrushFromSelection,
    // Вид
    ZoomIn,
    ZoomOut,
    ZoomFit,
    Zoom100,
    Hand,
    Pan,    // arg: 0 влево, 1 вправо, 2 вверх, 3 вниз
    Nudge,  // то же, +4 — на 8 точек
    Grid,
    Checker,
    Seamless,
    Wrap,
    Rulers,
    Guides,
    GridScreen,
    MapContext,
    NextMapPlace,
    MapTiles,
    // Инструменты и кисть
    Tool,
    BrushSmaller,
    BrushBigger,
    PatternPrev,
    PatternNext,
    PixelPerfect,
    BlendMode,
    SymX,
    SymY,
    ToolOptions,
    // Цвет и картинка
    SwapColors,
    ColorPicker,
    PaletteScreen,
    Indexed,
    ImageScreen,
    Filters,
    Tileset,
    TilePicker,
    // Анимация
    Timeline,
    Play,
    PrevFrame,
    NextFrame,
    FirstFrame,
    LastFrame,
    LayerUp,
    LayerDown,
    Onion,
    // Карта
    ObjectForm,
    MapFind,
    // Окна
    MainMenu,
    Help,
    Cheatsheet,
    SwitchMode,
};

struct Shortcut {
    int key = 0;
    int mods = 0;  // MOD_*
    KeyWhere where = KeyWhere::Both;
    Cmd cmd = Cmd::MainMenu;
    int arg = 0;  // номер инструмента у Cmd::Tool
    /// Раздел шпаргалки.
    const char* group = "";
    /// Что делает; nullptr — второе сочетание той же строки шпаргалки
    /// (цифры, стрелки, Num+): своей строки у него нет.
    const char* text = nullptr;
    /// Подпись клавиш в шпаргалке, если не собирать её из key и mods.
    const char* label = nullptr;
};

/// Раздел общих клавиш: шпаргалка ставит его строкой внизу.
inline constexpr const char* kWindowGroup = "Окна";

/// Вся таблица в порядке шпаргалки.
std::span<const Shortcut> shortcuts();

/// Сочетание для нажатия. Сначала — точное совпадение модификаторов; не
/// нашлось и зажат Shift — то же без Shift: Shift+G, как и в 1.6, — сетка.
/// nullptr — клавиша ничего не делает.
const Shortcut* findShortcut(int key, int mods, bool tile);

/// Имя клавиши для людей: «Ctrl+Shift+S», «F1», «Пробел».
std::string keyName(int key, int mods);

/// Подпись строки шпаргалки: label либо имя клавиши, а у сочетаний той же
/// команды, что идут следом без текста, — через запятую.
std::string shortcutKeys(std::span<const Shortcut> all, std::size_t i);

}  // namespace tjtm
