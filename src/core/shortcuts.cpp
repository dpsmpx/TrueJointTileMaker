#include "core/shortcuts.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <deque>

#include "core/text.h"

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
    Shortcut{KEY_E, kCtrlShift, kTile, Export, 0, kFile, "Экспорт"},
    Shortcut{KEY_E, MOD_CTRL | MOD_ALT, kTile, ExportAgain, 0, kFile, "Экспортировать снова"},
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
    // Полноэкранный — только F11 и кнопка в меню: Escape остаётся меню
    // (решение владельца).
    Shortcut{KEY_F11, 0, kBoth, Fullscreen, 0, kView, "Во весь экран"},
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
    Shortcut{KEY_DELETE, 0, kMap, DeleteObject, 0, kMapG, "Удалить объект под курсором"},
    Shortcut{KEY_B, kShift, kMap, MapTiles, 0, kMapG, "Картинками или клетками"},
    // Окна
    Shortcut{KEY_ESC, 0, kBoth, MainMenu, 0, kWin, "Главное меню"},
    Shortcut{KEY_M, 0, kBoth, MainMenu},
    Shortcut{KEY_TAB, 0, kBoth, SwitchMode, 0, kWin, "Тайл или карта"},
    // В других редакторах Tab прячет панели; здесь он — тайл и карта, а
    // Shift+Tab — базовый или полный режим (C4).
    Shortcut{KEY_TAB, kShift, kBoth, UiMode, 0, kView, "Базовый или полный режим"},
    // Строка меню с клавиатуры (D7, UX-19): стрелки ходят по меню и
    // пунктам, Enter выбирает.
    Shortcut{KEY_F10, 0, kBoth, MenuBar, 0, kView, "Строка меню"},
    Shortcut{KEY_F1, 0, kBoth, Cheatsheet, 0, kWin, "Эта шпаргалка"},
    Shortcut{KEY_SLASH, kShift, kBoth, Cheatsheet},
    Shortcut{KEY_H, 0, kBoth, Help, 0, kWin, "Справка"},
    Shortcut{KEY_K, kCtrl, kBoth, CommandSearch, 0, kWin, "Найти команду"},
};

constexpr const char* kMenuTitles[] = {"Файл",     "Правка", "Выделение", "Изображение",
                                       "Анимация", "Вид",    "Карта",     "Справка"};

// Порядок пунктов — как в редакторах растра: файл, правка, выделение…
// Пункты не своего режима не показываются.
constexpr std::array kMenu = {
    MenuItem{"Файл", "Новый…", New},
    MenuItem{"Файл", "Открыть…", Open},
    MenuItem{"Файл", "Недавние файлы…", Recent},
    MenuItem{"Файл"},
    MenuItem{"Файл", "Сохранить", Save},
    MenuItem{"Файл", "Сохранить как…", SaveAs},
    MenuItem{"Файл", "Экспорт…", Export, 0, kTile},
    MenuItem{"Файл", "Экспортировать снова", ExportAgain, 0, kTile},
    MenuItem{"Файл", "Выделение в файл…", SaveSelection, 0, kTile, true},
    MenuItem{"Файл"},
    MenuItem{"Файл", "Настройки…", Settings},
    MenuItem{"Файл"},
    MenuItem{"Файл", "Выход", Exit},
    MenuItem{"Правка", "Отменить", Undo},
    MenuItem{"Правка", "Повторить", Redo},
    MenuItem{"Правка"},
    MenuItem{"Правка", "Копировать", Copy, 0, kTile},
    MenuItem{"Правка", "Вырезать", Cut, 0, kTile},
    MenuItem{"Правка", "Вставить", PasteClipboard, 0, kTile},
    MenuItem{"Правка", "Вставить под курсор", PasteHere},
    MenuItem{"Правка", "Стереть выделенное", EraseSelection, 0, kTile},
    MenuItem{"Правка", "Найти и заменить…", MapFind, 0, kMap, true},
    MenuItem{"Выделение", "Выделить всё", SelectAll, 0, kTile},
    MenuItem{"Выделение", "Снять выделение", Deselect, 0, kTile},
    MenuItem{"Выделение", "Вернуть выделение", Reselect, 0, kTile},
    MenuItem{"Выделение", "Обратить выделение", InvertSelection, 0, kTile},
    MenuItem{"Выделение"},
    MenuItem{"Выделение", "Кисть из выделения", BrushFromSelection, 0, kTile, true},
    MenuItem{"Изображение", "Изображение и выделение…", ImageScreen, 0, kTile},
    MenuItem{"Изображение", "Фильтры и эффекты…", Filters, 0, kTile, true},
    MenuItem{"Изображение", "Операции с цветом…", ColorOps, 0, kTile, true},
    MenuItem{"Изображение"},
    MenuItem{"Изображение", "Выбор цвета…", ColorPicker, 0, kTile},
    MenuItem{"Изображение", "Палитра…", PaletteScreen, 0, kTile},
    MenuItem{"Изображение", "Индексная палитра", Indexed, 0, kTile, true},
    MenuItem{"Изображение", "Поменять цвета местами", SwapColors},
    MenuItem{"Изображение", "Чёрный и белый", ResetColors, 0, kTile},
    MenuItem{"Изображение"},
    MenuItem{"Изображение", "Наборы тайлов…", Tileset, 0, kTile, true},
    MenuItem{"Изображение", "Тайлы слоя-тайлкарты…", TilePicker, 0, kTile, true},
    MenuItem{"Изображение", "Набор для игры…", GameSet, 0, kTile, true},
    MenuItem{"Анимация", "Слои, кадры и экспорт…", LayersScreen, 0, kTile, true},
    MenuItem{"Анимация", "Лента кадров", Timeline, 0, kTile, true},
    MenuItem{"Анимация", "Проиграть", Play, 0, kTile, true},
    MenuItem{"Анимация", "Луковая кожа", Onion, 0, kTile, true},
    MenuItem{"Анимация"},
    MenuItem{"Анимация", "Кадр назад", PrevFrame, 0, kTile, true},
    MenuItem{"Анимация", "Кадр вперёд", NextFrame, 0, kTile, true},
    MenuItem{"Анимация", "Слой выше", LayerUp, 0, kTile, true},
    MenuItem{"Анимация", "Слой ниже", LayerDown, 0, kTile, true},
    MenuItem{"Вид", "Крупнее", ZoomIn},
    MenuItem{"Вид", "Мельче", ZoomOut},
    MenuItem{"Вид", "Вписать картинку", ZoomFit},
    MenuItem{"Вид", "Масштаб 100 %", Zoom100},
    MenuItem{"Вид", "Во весь экран", Fullscreen},
    MenuItem{"Вид", "Боковая панель", Dock, 0, kBoth, true},
    MenuItem{"Вид", "Базовый режим", UiMode},
    MenuItem{"Вид"},
    MenuItem{"Вид", "Сетка", Grid},
    MenuItem{"Вид", "Шахматка", Checker},
    MenuItem{"Вид", "Бесшовный предпросмотр", Seamless, 0, kBoth, true},
    MenuItem{"Вид", "Линейки", Rulers, 0, kBoth, true},
    MenuItem{"Вид", "Направляющие", Guides, 0, kBoth, true},
    MenuItem{"Вид", "Сетка и направляющие…", GridScreen, 0, kBoth, true},
    MenuItem{"Вид"},
    MenuItem{"Вид", "Тайл в окружении карты", MapContext, 0, kTile, true},
    MenuItem{"Вид", "Карта картинками набора", MapTiles, 0, kMap, true},
    MenuItem{"Вид"},
    MenuItem{"Вид", "Оформление…", Appearance},
    MenuItem{"Карта", "К редактору карт", SwitchMode, 0, kTile},
    MenuItem{"Карта", "К редактору тайлов", SwitchMode, 0, kMap},
    MenuItem{"Карта", "Свойства и проверка локации…", MapProperties, 0, kMap, true},
    MenuItem{"Карта", "Форма объекта под курсором", ObjectForm, 0, kMap},
    MenuItem{"Карта", "Удалить объект под курсором", DeleteObject, 0, kMap},
    MenuItem{"Справка", "Найти команду…", CommandSearch},
    MenuItem{"Справка", "Клавиши", Cheatsheet},
    MenuItem{"Справка", "Настроить клавиши…", KeysScreen},
    MenuItem{"Справка", "Справка", Help},
    MenuItem{"Справка", "Главное меню", MainMenu},
};

bool fits(const Shortcut& s, bool tile) {
    return s.where == KeyWhere::Both || (s.where == KeyWhere::Tile) == tile;
}

/// Действующая таблица: стандартная, пока applyKeymap не собрал другую.
std::vector<Shortcut>& live() {
    static std::vector<Shortcut> t(kTable.begin(), kTable.end());
    return t;
}

}  // namespace

std::span<const Shortcut> shortcuts() {
    return live();
}

std::span<const Shortcut> defaultShortcuts() {
    return kTable;
}

std::span<const char* const> menuTitles() {
    return kMenuTitles;
}

std::span<const MenuItem> menuItems() {
    return kMenu;
}

std::string commandTitle(Cmd c, int arg, bool tile) {
    for (const MenuItem& mi : kMenu) {
        if (mi.text == nullptr || mi.cmd != c || mi.arg != arg) continue;
        if (mi.where != KeyWhere::Both && (mi.where == KeyWhere::Tile) != tile) continue;
        std::string t = mi.text;
        const std::string dots = "…";
        if (t.size() > dots.size() && t.ends_with(dots)) t.resize(t.size() - dots.size());
        return t;
    }
    for (const Shortcut& s : kTable) {
        if (s.cmd == c && s.text != nullptr && fits(s, tile)) return s.text;
    }
    return "Правка";
}

std::string commandKeys(Cmd c, int arg, bool tile) {
    for (const Shortcut& s : live()) {
        if (s.cmd == c && s.arg == arg && s.key != 0 && fits(s, tile))
            return keyName(s.key, s.mods);
    }
    return {};
}

const Shortcut* findShortcut(int key, int mods, bool tile) {
    if (key == 0) return nullptr;
    auto find = [&](int want) -> const Shortcut* {
        for (const Shortcut& s : live()) {
            if (s.key == key && s.mods == want && fits(s, tile)) return &s;
        }
        return nullptr;
    };
    if (const Shortcut* s = find(mods)) return s;
    if ((mods & MOD_SHIFT) != 0) return find(mods & ~MOD_SHIFT);
    return nullptr;
}

std::string keyName(int key, int mods) {
    if (key == 0) return {};
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
        case KEY_BACK: return s + "Backspace";
        case KEY_INSERT: return s + "Insert";
        case KEY_QUOTE: return s + "'";
        case KEY_GRAVE: return s + "`";
        case KEY_BACKSLASH: return s + "\\";
        default: return s + "#" + std::to_string(key);
    }
}

std::string shortcutKeys(std::span<const Shortcut> all, std::size_t i) {
    const Shortcut& s = all[i];
    if (s.label != nullptr) return s.label;
    // Вся строка: в стандартной таблице за строкой без подписи идут только
    // сочетания той же команды, а у переназначенной подпись снята, и
    // строку собирают её клавиши как есть.
    std::string out;
    for (std::size_t j = i; j < all.size() && (j == i || all[j].text == nullptr); ++j) {
        if (all[j].key == 0) continue;
        if (!out.empty()) out += ", ";
        out += keyName(all[j].key, all[j].mods);
    }
    return out;
}

// ---------------------------------------------------------------------------
// Правка клавиш (D3)
// ---------------------------------------------------------------------------

namespace {

/// Строки, на которые ссылаются созданные здесь Shortcut: живут до конца.
const char* intern(const std::string& s) {
    static std::deque<std::string> pool;
    for (const std::string& p : pool) {
        if (p == s) return p.c_str();
    }
    pool.push_back(s);
    return pool.back().c_str();
}

/// Имена команд в Config.txt — по порядку перечисления Cmd.
constexpr std::array<const char*, static_cast<std::size_t>(Appearance) + 1> kIds = {
    "New",
    "Open",
    "Save",
    "SaveAs",
    "SaveSelection",
    "Undo",
    "Redo",
    "PasteHere",
    "EraseSelection",
    "SelectAll",
    "Deselect",
    "Reselect",
    "InvertSelection",
    "Copy",
    "Cut",
    "PasteClipboard",
    "BrushFromSelection",
    "ZoomIn",
    "ZoomOut",
    "ZoomFit",
    "Zoom100",
    "Fullscreen",
    "Hand",
    "Pan",
    "Nudge",
    "Grid",
    "Checker",
    "Seamless",
    "Wrap",
    "Rulers",
    "Guides",
    "GridScreen",
    "MapContext",
    "NextMapPlace",
    "MapTiles",
    "Tool",
    "BrushSmaller",
    "BrushBigger",
    "PatternPrev",
    "PatternNext",
    "PixelPerfect",
    "BlendMode",
    "SymX",
    "SymY",
    "ToolOptions",
    "SwapColors",
    "ResetColors",
    "ColorPicker",
    "PaletteScreen",
    "Indexed",
    "ImageScreen",
    "Filters",
    "Tileset",
    "TilePicker",
    "Timeline",
    "Play",
    "PrevFrame",
    "NextFrame",
    "FirstFrame",
    "LastFrame",
    "LayerUp",
    "LayerDown",
    "Onion",
    "ObjectForm",
    "MapFind",
    "MainMenu",
    "Help",
    "Cheatsheet",
    "SwitchMode",
    "Recent",
    "Settings",
    "Exit",
    "ColorOps",
    "LayersScreen",
    "GameSet",
    "MapProperties",
    "Dock",
    "UiMode",
    "CommandSearch",
    "DeleteObject",
    "Export",
    "ExportAgain",
    "KeysScreen",
    "MenuBar",
    "Appearance",
};

/// Названия действий, которые в шпаргалке делят строку: там «Кадр назад,
/// вперёд», а на экране клавиш у каждого — своя строка.
struct SplitName {
    Cmd cmd;
    int arg;
    const char* title;
};
constexpr std::array kSplit = {
    SplitName{ZoomIn, 0, "Крупнее"},
    SplitName{ZoomOut, 0, "Мельче"},
    SplitName{Nudge, 0, "Сдвинуть влево"},
    SplitName{Nudge, 1, "Сдвинуть вправо"},
    SplitName{Nudge, 2, "Сдвинуть вверх"},
    SplitName{Nudge, 3, "Сдвинуть вниз"},
    SplitName{Nudge, 4, "Сдвинуть влево на 8"},
    SplitName{Nudge, 5, "Сдвинуть вправо на 8"},
    SplitName{Nudge, 6, "Сдвинуть вверх на 8"},
    SplitName{Nudge, 7, "Сдвинуть вниз на 8"},
    SplitName{Pan, 0, "Холст влево"},
    SplitName{Pan, 1, "Холст вправо"},
    SplitName{Pan, 2, "Холст вверх"},
    SplitName{Pan, 3, "Холст вниз"},
    SplitName{BrushSmaller, 0, "Кисть меньше"},
    SplitName{BrushBigger, 0, "Кисть больше"},
    SplitName{PatternPrev, 0, "Узор назад"},
    SplitName{PatternNext, 0, "Узор вперёд"},
    SplitName{SymX, 0, "Симметрия по X"},
    SplitName{SymY, 0, "Симметрия по Y"},
    SplitName{PrevFrame, 0, "Кадр назад"},
    SplitName{NextFrame, 0, "Кадр вперёд"},
    SplitName{FirstFrame, 0, "Первый кадр"},
    SplitName{LastFrame, 0, "Последний кадр"},
    SplitName{LayerUp, 0, "Слой выше"},
    SplitName{LayerDown, 0, "Слой ниже"},
    SplitName{Cheatsheet, 0, "Шпаргалка клавиш"},
    SplitName{Hand, 0, "Рука (пока зажат пробел)"},
};

/// Раздел шпаргалки для команды, у которой клавиши не было: по меню.
const char* groupOfMenu(const char* menu) {
    if (std::strcmp(menu, "Изображение") == 0) return kColor;
    if (std::strcmp(menu, "Справка") == 0) return kWin;
    return menu;
}

/// Набор «как в Aseprite» — по data/gui.xml Aseprite: буквы инструментов,
/// + и − — размер кисти, 1 — масштаб 100 %, Tab — лента кадров, Ctrl+Tab —
/// соседний документ (у нас — тайл и карта), Ctrl+Space — поиск команды,
/// Ctrl+K — настройки. Чего у Aseprite нет, остаётся как в стандартном.
constexpr std::array<std::pair<const char*, const char*>, 27> kAseprite = {{
    {"Tool.0", "B"},
    {"Tool.1", "E"},
    {"Tool.2", "G"},
    {"Tool.3", "I"},
    {"Tool.4", "L"},
    {"Tool.5", "U"},
    {"Tool.6", "Shift+U"},
    {"Tool.8", "M"},
    {"Zoom100", "1 | Ctrl+1"},
    {"BrushBigger", "+ | Num + | ]"},
    {"BrushSmaller", "− | Num − | ["},
    {"ZoomIn", "Ctrl++"},
    {"ZoomOut", "Ctrl+−"},
    {"Grid", "Ctrl+'"},
    {"PaletteScreen", "F4"},
    {"Timeline", "Tab"},
    {"SwitchMode", "Ctrl+Tab"},
    {"CommandSearch", "Ctrl+Пробел"},
    {"Settings", "Ctrl+K"},
    {"Export", "Ctrl+E"},
    {"ExportAgain", "Ctrl+Shift+X"},
    {"BrushFromSelection", "Ctrl+B"},
    {"InvertSelection", "Ctrl+Shift+I"},
    {"Undo", "Ctrl+Z"},
    {"Redo", "Ctrl+Y | Ctrl+Shift+Z"},
    {"EraseSelection", "Delete | Backspace"},
    {"MainMenu", "Esc"},
}};

bool overlaps(KeyWhere a, KeyWhere b) {
    return a == KeyWhere::Both || b == KeyWhere::Both || a == b;
}

/// Ставит клавиши действию, не трогая других.
void placeKeys(std::vector<Shortcut>& t, const KeyAction& a, const std::vector<KeyCombo>& keys) {
    if (keysIn(t, a.cmd, a.arg) == keys) return;
    auto first = std::find_if(t.begin(), t.end(),
                              [&](const Shortcut& s) { return s.cmd == a.cmd && s.arg == a.arg; });
    Shortcut tmpl;
    std::size_t at = t.size();
    if (first == t.end()) {
        // Команда меню без клавиши: строкой в конец своего раздела.
        tmpl = Shortcut{0, 0, a.where, a.cmd, a.arg, intern(a.group), intern(a.title)};
        for (std::size_t i = 0; i < t.size(); ++i) {
            if (a.group == t[i].group) at = i + 1;
        }
    } else {
        at = static_cast<std::size_t>(first - t.begin());
        tmpl = *first;
        // Строка шпаргалки сменилась: её подпись («1 … 9») больше не верна.
        std::size_t head = at;
        while (head > 0 && t[head].text == nullptr)
            --head;
        t[head].label = nullptr;
        tmpl.label = nullptr;
        std::erase_if(t, [&](const Shortcut& s) { return s.cmd == a.cmd && s.arg == a.arg; });
    }
    std::vector<Shortcut> add;
    for (const KeyCombo& k : keys) {
        Shortcut s = tmpl;
        s.key = k.key;
        s.mods = k.mods;
        if (!add.empty()) s.text = nullptr;
        add.push_back(s);
    }
    // Без клавиш действие остаётся строкой с key = 0: у строки шпаргалки
    // сохраняется заголовок, а клавишу можно задать снова.
    if (add.empty()) add.push_back(tmpl);
    if (add.size() == 1 && keys.empty()) {
        add[0].key = 0;
        add[0].mods = 0;
    }
    t.insert(t.begin() + static_cast<std::ptrdiff_t>(std::min(at, t.size())), add.begin(),
             add.end());
}

std::vector<const KeyAction*> setIn(std::vector<Shortcut>& t, const KeyAction& a,
                                    const std::vector<KeyCombo>& keys) {
    std::vector<const KeyAction*> robbed;
    for (const KeyCombo& k : keys) {
        for (const KeyAction& b : keyActions()) {
            if ((b.cmd == a.cmd && b.arg == a.arg) || !overlaps(a.where, b.where)) continue;
            std::vector<KeyCombo> bk = keysIn(t, b.cmd, b.arg);
            if (std::erase(bk, k) == 0) continue;
            placeKeys(t, b, bk);
            robbed.push_back(&b);
        }
    }
    placeKeys(t, a, keys);
    return robbed;
}

bool isAseprite(const std::string& preset) {
    return netToLower(preset).find("aseprite") != std::string::npos;
}

}  // namespace

const std::vector<KeyAction>& keyActions() {
    static const std::vector<KeyAction> all = [] {
        std::vector<KeyAction> out;
        auto known = [&](Cmd c, int arg) {
            return std::any_of(out.begin(), out.end(),
                               [&](const KeyAction& a) { return a.cmd == c && a.arg == arg; });
        };
        const char* rowText = nullptr;
        for (const Shortcut& s : kTable) {
            if (s.text != nullptr) rowText = s.text;
            if (known(s.cmd, s.arg)) continue;
            KeyAction a{s.cmd, s.arg, s.where, s.group, s.text != nullptr ? s.text : ""};
            for (const SplitName& n : kSplit) {
                if (n.cmd == s.cmd && n.arg == s.arg) a.title = n.title;
            }
            if (a.title.empty() && s.cmd == Tool)
                a.title = "Инструмент " + std::to_string(s.arg + 1);
            if (a.title.empty() && rowText != nullptr) a.title = rowText;
            a.fixed = s.cmd == Hand;
            out.push_back(a);
        }
        for (const MenuItem& mi : kMenu) {
            if (mi.text == nullptr || known(mi.cmd, mi.arg)) continue;
            std::string t = mi.text;
            const std::string dots = "…";
            if (t.ends_with(dots)) t.resize(t.size() - dots.size());
            out.push_back(KeyAction{mi.cmd, mi.arg, mi.where, groupOfMenu(mi.menu), t});
        }
        return out;
    }();
    return all;
}

std::string actionId(const KeyAction& a) {
    std::string id = kIds[static_cast<std::size_t>(a.cmd)];
    const bool many = std::count_if(keyActions().begin(), keyActions().end(),
                                    [&](const KeyAction& b) { return b.cmd == a.cmd; }) > 1;
    if (many) id += "." + std::to_string(a.arg);
    return id;
}

const KeyAction* findAction(const std::string& id) {
    const std::string want = netToLower(netTrim(id));
    for (const KeyAction& a : keyActions()) {
        if (netToLower(actionId(a)) == want) return &a;
    }
    return nullptr;
}

bool parseKeyName(const std::string& s, KeyCombo& out) {
    std::string want = netToLower(netTrim(s));
    // Как пишут руками: дефис вместо минуса, английские имена.
    auto alias = [&](const std::string& from, const std::string& to) {
        if (want.ends_with(from) &&
            (want.size() == from.size() || want[want.size() - from.size() - 1] == '+'))
            want = want.substr(0, want.size() - from.size()) + to;
    };
    alias("space", "пробел");
    alias("plus", "+");
    alias("minus", "−");
    alias("-", "−");
    alias("del", "delete");
    alias("escape", "esc");
    alias("return", "enter");
    if (want.empty()) return false;
    for (int mods = 0; mods < 8; ++mods) {
        for (int key = 1; key <= KEY_CODE_MAX; ++key) {
            const std::string n = keyName(key, mods);
            if (n[0] != '#' && netToLower(n) == want) {
                out = {key, mods};
                return true;
            }
        }
    }
    return false;
}

std::string keysText(const std::vector<KeyCombo>& keys) {
    std::string out;
    for (const KeyCombo& k : keys) {
        if (!out.empty()) out += " | ";
        out += keyName(k.key, k.mods);
    }
    return out.empty() ? "нет" : out;
}

std::vector<KeyCombo> parseKeysText(const std::string& s) {
    std::vector<KeyCombo> out;
    std::size_t from = 0;
    while (from <= s.size()) {
        std::size_t bar = s.find('|', from);
        if (bar == std::string::npos) bar = s.size();
        KeyCombo k;
        if (parseKeyName(s.substr(from, bar - from), k) &&
            std::find(out.begin(), out.end(), k) == out.end())
            out.push_back(k);
        from = bar + 1;
    }
    return out;
}

std::vector<KeyCombo> keysIn(std::span<const Shortcut> table, Cmd c, int arg) {
    std::vector<KeyCombo> out;
    for (const Shortcut& s : table) {
        if (s.cmd == c && s.arg == arg && s.key != 0) out.push_back({s.key, s.mods});
    }
    return out;
}

std::vector<Shortcut> presetTable(const std::string& preset) {
    std::vector<Shortcut> t(kTable.begin(), kTable.end());
    if (isAseprite(preset)) {
        for (const auto& [id, keys] : kAseprite) {
            if (const KeyAction* a = findAction(id)) setIn(t, *a, parseKeysText(keys));
        }
    }
    return t;
}

std::vector<const KeyAction*> setActionKeys(const KeyAction& a, const std::vector<KeyCombo>& keys) {
    return setIn(live(), a, keys);
}

void applyKeymap(const std::string& preset,
                 const std::vector<std::pair<std::string, std::string>>& custom) {
    std::vector<Shortcut> t = presetTable(preset);
    for (const auto& [id, keys] : custom) {
        const KeyAction* a = findAction(id);
        if (a == nullptr || a->fixed) continue;
        setIn(t, *a, parseKeysText(keys));
    }
    live() = std::move(t);
}

std::vector<std::pair<std::string, std::string>> keymapDiff(const std::string& preset) {
    const std::vector<Shortcut> base = presetTable(preset);
    std::vector<std::pair<std::string, std::string>> out;
    for (const KeyAction& a : keyActions()) {
        const std::vector<KeyCombo> now = keysIn(live(), a.cmd, a.arg);
        if (now != keysIn(base, a.cmd, a.arg)) out.emplace_back(actionId(a), keysText(now));
    }
    return out;
}

}  // namespace tjtm
