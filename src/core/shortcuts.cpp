#include "core/shortcuts.h"

#include <array>

namespace tjtm {

namespace {

using enum Cmd;
constexpr KeyWhere kBoth = KeyWhere::Both;
constexpr KeyWhere kTile = KeyWhere::Tile;
constexpr KeyWhere kMap = KeyWhere::Map;
constexpr int kCtrl = MOD_CTRL;
constexpr int kShift = MOD_SHIFT;
constexpr int kCtrlShift = MOD_CTRL | MOD_SHIFT;

constexpr const char* kFile = "Файл";
constexpr const char* kEdit = "Правка";
constexpr const char* kSel = "Выделение";
constexpr const char* kView = "Вид";
constexpr const char* kTools = "Инструменты и кисть";
constexpr const char* kColor = "Цвет и картинка";
constexpr const char* kAnim = "Анимация";
constexpr const char* kMapG = "Карта";
constexpr const char* kWin = kWindowGroup;

// Клавиши — те же, что у 1.7 до таблицы: сценарии переноса
// (tests/screens) нажимают их и сверяются со снимками.
constexpr std::array kTable = {
    // Файл
    Shortcut{KEY_N, kCtrl, kBoth, New, 0, kFile, "Новый документ"},
    Shortcut{KEY_O, kCtrl, kBoth, Open, 0, kFile, "Открыть"},
    Shortcut{KEY_S, kCtrl, kBoth, Save, 0, kFile, "Сохранить"},
    Shortcut{KEY_S, kCtrlShift, kBoth, SaveAs, 0, kFile, "Сохранить как"},
    Shortcut{KEY_A, 0, kBoth, SaveSelection, 0, kFile, "Выделение в файл"},
    // Правка
    Shortcut{KEY_Z, kCtrl, kBoth, Undo, 0, kEdit, "Отменить"},
    Shortcut{KEY_Z, 0, kBoth, Undo},
    Shortcut{KEY_Z, kCtrlShift, kBoth, Redo, 0, kEdit, "Повторить"},
    Shortcut{KEY_Y, kCtrl, kBoth, Redo},
    Shortcut{KEY_Y, 0, kBoth, Redo},
    Shortcut{KEY_V, 0, kBoth, PasteHere, 0, kEdit, "Вставить под курсор"},
    Shortcut{KEY_DELETE, 0, kTile, EraseSelection, 0, kEdit, "Стереть выделенное"},
    // Выделение
    Shortcut{KEY_A, kCtrl, kTile, SelectAll, 0, kSel, "Выделить всё"},
    Shortcut{KEY_D, kCtrl, kTile, Deselect, 0, kSel, "Снять выделение"},
    Shortcut{KEY_D, kCtrlShift, kTile, Reselect, 0, kSel, "Вернуть выделение"},
    Shortcut{KEY_I, kCtrl, kTile, InvertSelection, 0, kSel, "Обратить выделение"},
    Shortcut{KEY_C, kCtrl, kTile, Copy, 0, kSel, "Копировать"},
    Shortcut{KEY_X, kCtrl, kTile, Cut, 0, kSel, "Вырезать"},
    Shortcut{KEY_V, kCtrl, kTile, PasteClipboard, 0, kSel, "Вставить из буфера"},
    Shortcut{KEY_E, kCtrl, kTile, BrushFromSelection, 0, kSel, "Кисть из выделения"},
    Shortcut{KEY_LEFT, 0, kBoth, Nudge, 0, kSel, "Сдвинуть; без выделения — холст", "Стрелки"},
    Shortcut{KEY_RIGHT, 0, kBoth, Nudge, 1},
    Shortcut{KEY_UP, 0, kBoth, Nudge, 2},
    Shortcut{KEY_DOWN, 0, kBoth, Nudge, 3},
    Shortcut{KEY_LEFT, kShift, kBoth, Nudge, 4, kSel, "Сдвинуть на 8 точек", "Shift+стрелки"},
    Shortcut{KEY_RIGHT, kShift, kBoth, Nudge, 5},
    Shortcut{KEY_UP, kShift, kBoth, Nudge, 6},
    Shortcut{KEY_DOWN, kShift, kBoth, Nudge, 7},
    // Вид
    Shortcut{KEY_PLUS, 0, kBoth, ZoomIn, 0, kView, "Крупнее и мельче", "+  −"},
    Shortcut{KEY_NUMPLUS, 0, kBoth, ZoomIn},
    Shortcut{KEY_MINUS, 0, kBoth, ZoomOut},
    Shortcut{KEY_NUMMINUS, 0, kBoth, ZoomOut},
    Shortcut{KEY_PLUS, kCtrl, kBoth, ZoomIn},
    Shortcut{KEY_MINUS, kCtrl, kBoth, ZoomOut},
    Shortcut{KEY_0, kCtrl, kBoth, ZoomFit, 0, kView, "Вписать картинку"},
    Shortcut{KEY_1, kCtrl, kBoth, Zoom100, 0, kView, "Масштаб 100 %"},
    Shortcut{KEY_SPACE, 0, kBoth, Hand, 0, kView, "Рука; коротко — вписать"},
    // Стрелки по правилу владельца (#16): с инструментом выделения и
    // выделением они двигают выделенное, иначе — холст; Ctrl+стрелки —
    // всегда холст.
    Shortcut{KEY_LEFT, kCtrl, kBoth, Pan, 0, kView, "Двигать холст", "Ctrl+стрелки"},
    Shortcut{KEY_RIGHT, kCtrl, kBoth, Pan, 1},
    Shortcut{KEY_UP, kCtrl, kBoth, Pan, 2},
    Shortcut{KEY_DOWN, kCtrl, kBoth, Pan, 3},
    Shortcut{KEY_G, 0, kBoth, Grid, 0, kView, "Сетка"},
    Shortcut{KEY_C, 0, kBoth, Checker, 0, kView, "Шахматка"},
    Shortcut{KEY_B, 0, kBoth, Seamless, 0, kView, "Бесшовный предпросмотр"},
    Shortcut{KEY_W, 0, kBoth, Wrap, 0, kView, "Заворот через край"},
    Shortcut{KEY_R, kCtrl, kBoth, Rulers, 0, kView, "Линейки"},
    Shortcut{KEY_SEMICOLON, kCtrl, kBoth, Guides, 0, kView, "Направляющие"},
    Shortcut{KEY_G, kCtrl, kBoth, GridScreen, 0, kView, "Сетка и направляющие…"},
    Shortcut{KEY_B, kShift, kTile, MapContext, 0, kView, "Тайл в окружении карты"},
    Shortcut{KEY_B, kCtrl, kTile, NextMapPlace, 0, kView, "Следующее место на карте"},
    // Инструменты и кисть
    Shortcut{KEY_1, 0, kBoth, Tool, 0, kTools, "Инструменты панели", "1 … 9"},
    Shortcut{KEY_2, 0, kBoth, Tool, 1},
    Shortcut{KEY_3, 0, kBoth, Tool, 2},
    Shortcut{KEY_4, 0, kBoth, Tool, 3},
    Shortcut{KEY_5, 0, kBoth, Tool, 4},
    Shortcut{KEY_6, 0, kBoth, Tool, 5},
    Shortcut{KEY_7, 0, kBoth, Tool, 6},
    Shortcut{KEY_8, 0, kBoth, Tool, 7},
    Shortcut{KEY_9, 0, kBoth, Tool, 8},
    Shortcut{KEY_LBRACKET, 0, kTile, BrushSmaller, 0, kTools, "Размер кисти", "[  ]"},
    Shortcut{KEY_RBRACKET, 0, kTile, BrushBigger},
    Shortcut{KEY_LBRACKET, kShift, kTile, PatternPrev, 0, kTools, "Узор назад, вперёд",
             "Shift+[  ]"},
    Shortcut{KEY_RBRACKET, kShift, kTile, PatternNext},
    Shortcut{KEY_Q, 0, kTile, PixelPerfect, 0, kTools, "Пиксель-перфект"},
    Shortcut{KEY_E, 0, kBoth, BlendMode, 0, kTools, "Смешивание или замена"},
    Shortcut{KEY_F, 0, kBoth, SymX, 0, kTools, "Оси симметрии ↕ и ↔", "F  D"},
    Shortcut{KEY_D, 0, kBoth, SymY},
    Shortcut{KEY_O, 0, kTile, ToolOptions, 0, kTools, "Параметры инструментов"},
    // Цвет и картинка
    Shortcut{KEY_X, 0, kBoth, SwapColors, 0, kColor, "Поменять цвета местами"},
    Shortcut{KEY_X, kShift, kTile, ResetColors, 0, kColor, "Чёрный и белый"},
    Shortcut{KEY_P, 0, kTile, ColorPicker, 0, kColor, "Выбор цвета"},
    Shortcut{KEY_U, 0, kTile, PaletteScreen, 0, kColor, "Палитра"},
    Shortcut{KEY_I, 0, kTile, Indexed, 0, kColor, "Индексная палитра"},
    Shortcut{KEY_K, 0, kTile, ImageScreen, 0, kColor, "Изображение"},
    Shortcut{KEY_K, kShift, kTile, Filters, 0, kColor, "Фильтры и эффекты"},
    Shortcut{KEY_T, 0, kTile, Tileset, 0, kColor, "Наборы тайлов"},
    Shortcut{KEY_T, kCtrl, kTile, TilePicker, 0, kColor, "Тайлы слоя-тайлкарты"},
    // Анимация
    Shortcut{KEY_J, 0, kTile, Timeline, 0, kAnim, "Лента кадров"},
    Shortcut{KEY_ENTER, 0, kTile, Play, 0, kAnim, "Проиграть, остановить"},
    Shortcut{KEY_COMMA, 0, kTile, PrevFrame, 0, kAnim, "Кадр назад, вперёд", ",  ."},
    Shortcut{KEY_PERIOD, 0, kTile, NextFrame},
    Shortcut{KEY_HOME, 0, kTile, FirstFrame, 0, kAnim, "Первый, последний кадр", "Home  End"},
    Shortcut{KEY_END, 0, kTile, LastFrame},
    Shortcut{KEY_PGUP, 0, kTile, LayerUp, 0, kAnim, "Слой выше, ниже", "PgUp  PgDn"},
    Shortcut{KEY_PGDN, 0, kTile, LayerDown},
    Shortcut{KEY_F3, 0, kTile, Onion, 0, kAnim, "Луковая кожа"},
    // Карта
    Shortcut{KEY_ENTER, 0, kMap, ObjectForm, 0, kMapG, "Форма объекта"},
    Shortcut{KEY_F, kCtrl, kMap, MapFind, 0, kMapG, "Найти и заменить"},
    Shortcut{KEY_B, kShift, kMap, MapTiles, 0, kMapG, "Картинками или клетками"},
    // Окна
    Shortcut{KEY_ESC, 0, kBoth, MainMenu, 0, kWin, "Главное меню"},
    Shortcut{KEY_M, 0, kBoth, MainMenu},
    Shortcut{KEY_TAB, 0, kBoth, SwitchMode, 0, kWin, "Тайл или карта"},
    Shortcut{KEY_F1, 0, kBoth, Cheatsheet, 0, kWin, "Эта шпаргалка"},
    Shortcut{KEY_SLASH, kShift, kBoth, Cheatsheet},
    Shortcut{KEY_H, 0, kBoth, Help, 0, kWin, "Справка"},
};

bool fits(const Shortcut& s, bool tile) {
    return s.where == KeyWhere::Both || (s.where == KeyWhere::Tile) == tile;
}

}  // namespace

std::span<const Shortcut> shortcuts() {
    return kTable;
}

const Shortcut* findShortcut(int key, int mods, bool tile) {
    auto find = [&](int want) -> const Shortcut* {
        for (const Shortcut& s : kTable) {
            if (s.key == key && s.mods == want && fits(s, tile)) return &s;
        }
        return nullptr;
    };
    if (const Shortcut* s = find(mods)) return s;
    if ((mods & MOD_SHIFT) != 0) return find(mods & ~MOD_SHIFT);
    return nullptr;
}

std::string keyName(int key, int mods) {
    // «?» — это Shift с косой чертой; так его и ищут глазами.
    if (key == KEY_SLASH && mods == MOD_SHIFT) return "?";
    std::string s;
    if ((mods & MOD_CTRL) != 0) s += "Ctrl+";
    if ((mods & MOD_ALT) != 0) s += "Alt+";
    if ((mods & MOD_SHIFT) != 0) s += "Shift+";
    if (key >= KEY_A && key <= KEY_Z) return s + static_cast<char>(key);
    if (key >= KEY_0 && key <= KEY_9) return s + static_cast<char>(key);
    if (key >= KEY_F1 && key <= KEY_F12) return s + "F" + std::to_string(key - KEY_F1 + 1);
    switch (key) {
        case KEY_ENTER: return s + "Enter";
        case KEY_ESC: return s + "Esc";
        case KEY_TAB: return s + "Tab";
        case KEY_SPACE: return s + "Пробел";
        case KEY_DELETE: return s + "Delete";
        case KEY_HOME: return s + "Home";
        case KEY_END: return s + "End";
        case KEY_PGUP: return s + "PgUp";
        case KEY_PGDN: return s + "PgDn";
        case KEY_LEFT: return s + "←";
        case KEY_RIGHT: return s + "→";
        case KEY_UP: return s + "↑";
        case KEY_DOWN: return s + "↓";
        case KEY_PLUS: return s + "+";
        case KEY_MINUS: return s + "−";
        case KEY_NUMPLUS: return s + "Num +";
        case KEY_NUMMINUS: return s + "Num −";
        case KEY_COMMA: return s + ",";
        case KEY_PERIOD: return s + ".";
        case KEY_SEMICOLON: return s + ";";
        case KEY_SLASH: return s + "/";
        case KEY_LBRACKET: return s + "[";
        case KEY_RBRACKET: return s + "]";
        default: return s + "#" + std::to_string(key);
    }
}

std::string shortcutKeys(std::span<const Shortcut> all, std::size_t i) {
    const Shortcut& s = all[i];
    if (s.label != nullptr) return s.label;
    std::string out = keyName(s.key, s.mods);
    for (std::size_t j = i + 1; j < all.size() && all[j].text == nullptr && all[j].cmd == s.cmd;
         ++j)
        out += ", " + keyName(all[j].key, all[j].mods);
    return out;
}

}  // namespace tjtm
