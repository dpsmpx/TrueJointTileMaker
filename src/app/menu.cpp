// Справка, настройки, главное меню и наборы тайлов (TJTM.pas: «Справка,
// настройки, главное меню», «Наборы тайлов Andors-Love»).

#include <algorithm>

#include "app/editor.h"
#include "core/fsutil.h"
#include "core/pmath.h"
#include "core/theme.h"

namespace tjtm {

using namespace theme;

namespace {

constexpr int TSET_COLS = 8;
constexpr int TSET_ROWS = 3;
constexpr int TSET_PAGE = TSET_COLS * TSET_ROWS;
constexpr int TSET_CELL_W = 96;
constexpr int TSET_CELL_H = 92;
constexpr int TSET_THUMB = 48;
constexpr int TSET_X0 = (WIN_W - TSET_COLS * TSET_CELL_W) / 2;
constexpr int TSET_Y0 = 118;

Grid<Color> gridFromBgra(const std::vector<std::uint8_t>& raw, int size) {
    Grid<Color> g(size, size);
    for (std::size_t i = 0; i < g.d.size() && i * 4 + 3 < raw.size(); ++i) {
        g.d[i] = Color(raw[i * 4 + 3], raw[i * 4 + 2], raw[i * 4 + 1], raw[i * 4]);
    }
    return g;
}

}  // namespace

// ---------------------------------------------------------------------------
// Справка
// ---------------------------------------------------------------------------

void Editor::showHelpScreen() {
    std::vector<std::string> p;
    // Набираемый абзац: строки прозы склеиваются и переносятся потом по
    // ширине окна, а не по ширине набора в исходнике.
    std::string cur;
    auto flush = [&]() {
        if (cur.empty()) return;
        p.push_back(cur);
        cur.clear();
    };
    auto add = [&](const std::string& s) {
        flush();
        p.push_back(s);
    };
    auto par = [&](const std::string& s) { cur = cur.empty() ? s : cur + " " + s; };

    add(std::string(kAppTitle) + ", версия " + kAppVersion + ".");
    add("Редактор пиксельной графики, тайлсетов и карт локаций.");
    add("");
    add("ДВА РЕЖИМА");
    par("Тайл — рисование картинки. Карта — локация игры Andors-Love в формате");
    par("data/maps/*.map. Переключение через меню; оба документа живут");
    par("одновременно, переход туда и обратно ничего не теряет.");
    add("");
    add("ОБЩЕЕ ПРАВИЛО МЫШИ");
    add("Левая кнопка применяет: рисует, заливает, кладёт цвет, ставит объект.");
    add("Правая кнопка берёт: пипетка цвета или местности, убирает объект.");
    par("Ролик как кнопка: протяжка двигает изображение, щелчок без протяжки");
    par("меняет местами рабочие цвета.");
    add("Вращение ролика меняет масштаб вокруг курсора.");
    add("");
    add("ОБЗОРНАЯ ПАНЕЛЬ");
    par("Внизу панели справа документ показан целиком, жёлтая рамка отмечает");
    par("видимую на холсте часть. Протяжка средней кнопкой по обзору двигает");
    par("холст, а панорама холста двигает рамку: обзор и редактор всегда");
    par("смотрят на одно место документа, только в разных масштабах.");
    add("");
    add("ИНСТРУМЕНТЫ РЕЖИМА ТАЙЛА");
    add("Карандаш рисует первым цветом с учётом прозрачности.");
    add("Ластик стирает в прозрачность.");
    add("Заливка заполняет связную область одного цвета.");
    add("Пипетка берёт цвет пикселя.");
    par("Линия, прямоугольник и овал строятся протяжкой: пока кнопка нажата,");
    par("виден предпросмотр. Ctrl во время протяжки делает фигуру заполненной,");
    par("Shift приводит её к квадрату или кругу.");
    add("Дизеринг кладёт шахматку из первого и второго цветов.");
    add("Выделение: рамка бежит «муравьями», поэтому её видно на любом фоне.");
    par("Протяните рамку, чтобы выделить область и скопировать её");
    par("в буфер. Щелчок без протяжки вставляет буфер. Протяжка за выделенную");
    par("область копирует её на новое место.");
    add("");
    add("ИНСТРУМЕНТЫ РЕЖИМА КАРТЫ");
    par("Местность рисует выбранным видом клетки, правая кнопка берёт вид");
    par("из-под курсора. Заливка и прямоугольник работают тем же видом.");
    par("Объект: левая кнопка ставит объект выбранного вида или перетаскивает");
    par("уже стоящий, правая убирает, средняя открывает правку строки.");
    par("Строка правится полем ввода: работают выделение, Ctrl+C");
    par("и Ctrl+V, так что длинные строки переносятся между локациями целиком.");
    add("Выделение копирует и вставляет куски местности.");
    add("");
    add("НАБОРЫ ТАЙЛОВ");
    add("Клавиша T открывает наборы. Их четыре:");
    add("  Основные — семнадцать слотов игры: пол, стена, вода, герой и прочие.");
    add("  Жители и Враги — по картинке на существо: npc_elder, mob_wolf.");
    add("  Значки — картинки на кнопках инструментов самого редактора.");
    par("Правая кнопка открывает тайл в редакторе, левая записывает на его место");
    par("текущий. Открытый из набора тайл становится текущим файлом, поэтому S");
    par("сохраняет его обратно туда же.");
    add("");
    par("Имена основных слотов — часть договора с игрой: она ищет тайлы по имени");
    par("файла, и слот, названный иначе, она просто не найдёт. А вот состав");
    par("жителей и врагов редактор не хранит: он смотрит, какие файлы лежат");
    par("в папке. Папка и есть список — завели нового врага в игре, и он");
    par("появился здесь сам, без правки редактора.");
    add("");
    par("Пустое место — не ошибка: игра рисует такой тайл прежним способом,");
    par("заливкой и знаком шрифта, поэтому набор можно делать по одному.");
    add("То же и со значками: пока файла нет, кнопка выглядит подписью.");
    add("");
    add("ИЗ КАРТЫ ПРЯМО В ТАЙЛ");
    add("Двойной щелчок по клетке карты открывает тайл, которым она нарисована.");
    par("Под курсором житель — откроется его собственный тайл, враг из точки");
    par("появления — его; пусто — тайл самой местности. Если файла ещё нет,");
    par("редактор предложит завести его сразу с нужным именем, чтобы Сохранить");
    par("положило тайл туда, куда смотрит игра.");
    par("Второй щелчок пары обычного дела инструмента не делает: иначе двойной");
    par("щелчок инструментом объектов ставил бы два объекта разом.");
    add("");
    add("ИНДЕКСНАЯ ПАЛИТРА");
    add("Клавиша I связывает пиксели тайла с ячейками палитры того же цвета.");
    par("После этого правка ячейки левой кнопкой перекрашивает все связанные");
    par("с ней пиксели: набор тайлов перекрашивается сменой палитры, а не");
    par("перерисовкой. Включённый режим виден по рамке палитры цветом выделения,");
    par("а ячейка, из которой взят первый цвет, обведена ею же.");
    add("Правка ячейки и перекраска тайла отменяются вместе, одним Z.");
    par("Пиксель, не совпавший ни с одной ячейкой, остаётся свободным: чтобы");
    par("связать весь тайл, сведите его к цветам палитры в операциях с цветом.");
    par("Цвет, набранный ползунками, связи с палитрой не имеет, пока не совпадёт");
    par("с какой-нибудь ячейкой. Пипетка берёт цвет вместе с его связью.");
    add("");
    add("БЕСШОВНОСТЬ И СИММЕТРИЯ");
    par("Клавиша B включает бесшовный предпросмотр: тайл рисуется повторяющимся,");
    par("и швы видно прямо во время работы. Клавиша W включает заворот через");
    par("край: мазок, уходящий за правую границу, продолжается от левой.");
    add("Клавиши F и D включают вертикальную и горизонтальную оси симметрии.");
    add("");
    add("КЛАВИАТУРА");
    add("Escape — главное меню. H — эта справка.");
    add("N создать, L загрузить, S сохранить, A сохранить выделение.");
    add("Z отменить, Y повторить. V вставить буфер в позицию курсора.");
    add("X поменять местами цвета. P точный выбор цвета по каналам ARGB.");
    add("G сетка, C шахматка, B бесшовность, W заворот, F и D оси симметрии.");
    add("E режим замены вместо смешивания. R случайные цвета.");
    add("I индексная палитра. T наборы тайлов.");
    add("Delete очистить тайл. Tab переключить режим тайла и карты.");
    add("Цифры от 1 выбирают инструмент. Плюс и минус — масштаб.");
    add("Стрелки двигают изображение, пробел возвращает его в центр.");
    add("Alt, зажатый над холстом, временно включает пипетку.");
    add("");
    add("ФОРМАТ КАРТЫ");
    par("Сетка хранит только геометрию: пол, стена, вода, дерево, трава,");
    par("дорога, стоячая вода. Всё остальное описано объектами: npc, item,");
    par("exit, spawn, chest, note, bed, sign. Редактор разбирает у объекта");
    par("только вид и координаты, а хвост строки хранит как есть и пишет");
    par("обратно без изменений, поэтому условия перехода и содержимое сундуков");
    par("не теряются. Проверка локации в свойствах карты находит объекты на");
    par("непроходимых клетках и участки, до которых нельзя дойти.");
    add("");
    add("НАСТРОЙКИ");
    par("Все параметры хранятся в файле Config.txt рядом с программой в");
    par("кодировке UTF-8, по строке на параметр в виде «ключ = значение».");
    add("Файл можно править вручную или через пункт «Настройки» в меню.");
    par("При запуске программа пробует открыть стартовое изображение, и размер");
    par("тайла берётся из него. Размер из настроек нужен только тогда, когда");
    par("открыть картинку не удалось: тогда создаётся пустой тайл.");
    add("");
    add("БЛАГОДАРНОСТИ");
    add("Автор программы — DeadPixel, vk.com/deadpixel_programmer.");
    par("Команде PascalABC.NET за язык и библиотеку GraphABC: на них написана");
    par("первая версия редактора, и её облик сохранён в этой.");
    add("Авторам SDL за библиотеку, на которой построен нынешний редактор.");
    add("Игре Andors-Love за формат карт, ради которого сделан режим локаций.");
    add("Группе vk.com/programmer_ne_lamer за сообщество и поддержку.");
    add("Всем, кто пробовал редактор, присылал замечания и находил ошибки.");
    add("");
    add("Программа распространяется по лицензии GNU GPL версии 3.");
    flush();
    showTextScreen("Справка", p);
}

// ---------------------------------------------------------------------------
// Настройки
// ---------------------------------------------------------------------------

/// Экран настроек. Правки применяются к копии и попадают в Config.txt
/// только по кнопке «Сохранить».
void Editor::showSettingsScreen() {
    Config v = cfg;
    const int rowH = 30;
    const int path1X = 200;
    const int ctlX = 400;
    bool done = false;
    dropInput();
    pf_.input().keys.clear();

    auto toggleRow = [&](int x, int ry, const std::string& caption, bool value) {
        uiLabelLeft(x, ry, 30, caption, cText);
        return uiButton(x + 250, ry, 90, 30, ruBool(value), true);
    };
    auto stepRow = [&](int ry, const std::string& caption, int& value, int lo, int hi) {
        uiLabelLeft(60, ry, 30, caption, cText);
        if (uiButton(300, ry, 44, 30, "−", value > lo)) value = clampI(value - 1, lo, hi);
        uiLabel(348, ry, 70, 30, std::to_string(value), cText);
        if (uiButton(422, ry, 44, 30, "+", value < hi)) value = clampI(value + 1, lo, hi);
        if (uiButton(474, ry, 56, 30, "−8", value > lo + 7)) value = clampI(value - 8, lo, hi);
        if (uiButton(536, ry, 56, 30, "+8", value < hi - 7)) value = clampI(value + 8, lo, hi);
    };
    // Строка пути: поле, «Выбрать», «Очистить». pick — как выбрать путь.
    auto pathRow = [&](int y, const std::string& caption, std::string& value, auto pick) {
        uiLabelLeft(30, y, rowH, caption, cText);
        fb_.fillRoundRect(path1X, y, ctlX + 120, y + rowH, 4, 4, cFace);
        uiLabelLeft(path1X + 6, y, rowH, fitText(value, ctlX + 108 - path1X), cText);
        if (uiButton(ctlX + 130, y, 100, rowH, "Выбрать", true)) {
            const std::string path = pick();
            if (!path.empty()) value = path;
        }
        if (uiButton(ctlX + 236, y, 100, rowH, "Очистить", !value.empty())) value.clear();
    };

    while (!done && !quitNow_) {
        beginFrameInput();
        fb_.clear(cWindow);
        textCentered(0, 14, WIN_W, 46, "Настройки", cText);
        uiLabel(0, 46, WIN_W, 20, "Сохраняются в Config.txt рядом с программой", cTextDim);

        int y = 76;
        if (toggleRow(30, y, "Справка при запуске", v.showHelpOnStart))
            v.showHelpOnStart = !v.showHelpOnStart;
        if (toggleRow(410, y, "Меню при запуске", v.showMenuOnStart))
            v.showMenuOnStart = !v.showMenuOnStart;
        y += 36;
        if (toggleRow(30, y, "Сетка между пикселями", v.showGrid)) v.showGrid = !v.showGrid;
        if (toggleRow(410, y, "Шахматка прозрачности", v.showChecker))
            v.showChecker = !v.showChecker;
        y += 36;
        if (toggleRow(30, y, "Бесшовный предпросмотр", v.showSeamless))
            v.showSeamless = !v.showSeamless;
        if (toggleRow(410, y, "Открывать последний файл", v.autoLoadLast))
            v.autoLoadLast = !v.autoLoadLast;
        y += 36;
        if (toggleRow(30, y, "Запускаться в режиме карты", v.startInMapMode))
            v.startInMapMode = !v.startInMapMode;

        y += 40;
        stepRow(y, "Ширина нового тайла", v.defaultWidth, MIN_TILE, MAX_TILE);
        y += 36;
        stepRow(y, "Высота нового тайла", v.defaultHeight, MIN_TILE, MAX_TILE);
        y += 36;
        stepRow(y, "Ширина новой карты", v.defaultMapWidth, kMapMinSide, kMapMaxSide);
        y += 36;
        stepRow(y, "Высота новой карты", v.defaultMapHeight, kMapMinSide, kMapMaxSide);

        y += 38;
        pathRow(y, "Стартовое изображение", v.startupImage,
                [&] { return askOpenFile("Стартовое изображение"); });
        y += 32;
        pathRow(y, "Файл палитры", v.paletteFile, [&] { return askOpenFile("Файл палитры"); });
        y += 32;
        // Папка выбирается через любой файл в ней — так было в оригинале.
        pathRow(y, "Папка карт локаций", v.mapsFolder, [&] {
            const std::string path = askOpenMap("Выберите любую карту в нужной папке");
            return path.empty() ? std::string() : pathDirName(path);
        });
        y += 32;
        pathRow(y, "Папка набора тайлов", v.tilesFolder, [&] {
            const std::string path = askOpenFile("Выберите любой тайл в нужной папке");
            return path.empty() ? std::string() : pathDirName(path);
        });

        if (uiButton(WIN_W / 2 - 190, m.H - 52, 180, 36, "Сохранить", true)) {
            // Поля «заполняется программой» могли поменяться, пока экран был
            // открыт (выбор файла запоминает папку): берутся свежие.
            v.lastFile = cfg.lastFile;
            v.lastMapFile = cfg.lastMapFile;
            v.lastFolder = cfg.lastFolder;
            v.iconsFolder = cfg.iconsFolder;
            cfg = v;
            m.gridOn = v.showGrid;
            m.checkerOn = v.showChecker;
            m.seamlessOn = v.showSeamless;
            if (!configSave(configPath_, cfg)) {
                showMessage("Настройки не сохранены",
                            "Не удалось записать файл " + configPath_ +
                                ". Проверьте права на запись в папку программы.");
            }
            done = true;
        }
        if (uiButton(WIN_W / 2 + 10, m.H - 52, 180, 36, "Отмена", true)) done = true;
        redraw();
        if (takeKey() == KEY_ESC) done = true;
        idle();
    }
    waitMouseRelease();
    m.needRepaint = true;
}

// ---------------------------------------------------------------------------
// Главное меню
// ---------------------------------------------------------------------------

void Editor::showMainMenu() {
    bool done = false;
    dropInput();
    pf_.input().keys.clear();
    const int bw = 420;
    // Кнопок в режиме тайла двенадцать, и они должны уместиться над
    // подписью внизу окна: отсюда шаг мельче обычного.
    const int bh = 32;
    const int stp = 36;
    const int bx = (WIN_W - bw) / 2;

    while (!done && !quitNow_) {
        beginFrameInput();
        const bool isMap = m.docMode == DOC_MAP;
        std::string lastPath;
        std::string curName;
        std::string lastCaption;
        if (isMap) {
            lastPath = resolvePath(cfg.lastMapFile);
            curName = fileNameOnly(m.currentMapFile);
            if (curName.empty()) curName = "новая карта";
            lastCaption = cfg.lastMapFile.empty()
                              ? "Открыть последнюю карту — нет"
                              : "Открыть последнюю карту — " + fileNameOnly(cfg.lastMapFile);
        } else {
            lastPath = resolvePath(cfg.lastFile);
            curName = fileNameOnly(m.currentFile);
            if (curName.empty()) curName = "новый тайл";
            lastCaption = cfg.lastFile.empty()
                              ? "Открыть последний файл — нет"
                              : "Открыть последний файл — " + fileNameOnly(cfg.lastFile);
        }
        const bool lastOk = fileExists(lastPath);
        std::string info =
            curName + "   " + std::to_string(m.docW()) + " на " + std::to_string(m.docH());
        if (isMap) info += "   объектов " + std::to_string(m.map.objects.size());
        if (m.isModified()) info += "   есть несохранённые изменения";

        fb_.clear(cWindow);
        textCentered(0, 16, WIN_W, 52, kAppTitle, cText);
        uiLabel(0, 52, WIN_W, 22, isMap ? "режим карты локации" : "режим тайла", cAccent);
        uiLabel(0, 74, WIN_W, 22, info, cTextDim);

        int y = 100;
        if (uiButton(bx, y, bw, bh, "Продолжить редактирование", true)) done = true;
        y += stp;
        if (uiButton(bx, y, bw, bh, fitText(lastCaption, bw - 20), lastOk)) {
            commandOpenLast();
            done = true;
        }
        y += stp;
        if (uiButton(bx, y, bw, bh, isMap ? "Создать карту" : "Создать файл", true)) {
            commandNew();
            done = true;
        }
        y += stp;
        if (uiButton(bx, y, bw, bh, isMap ? "Загрузить карту" : "Загрузить файл", true)) {
            commandOpen();
            done = true;
        }
        y += stp;
        if (uiButton(bx, y, bw, bh, "Сохранить", true) && commandSave()) done = true;
        y += stp;
        if (uiButton(bx, y, bw, bh, "Сохранить как", true) && commandSaveAs()) done = true;
        y += stp;
        if (isMap) {
            if (uiButton(bx, y, bw, bh, "Свойства и проверка локации", true))
                showMapPropertiesScreen();
        } else if (uiButton(bx, y, bw, bh, "Наборы тайлов", true) && showTilesetScreen()) {
            done = true;
        }
        if (!isMap) {
            y += stp;
            if (uiButton(bx, y, bw, bh, "Операции с цветом", true)) showColorOpsScreen();
        }
        y += stp;
        if (uiButton(bx, y, bw, bh,
                     isMap ? "Перейти к редактору тайлов" : "Перейти к редактору карт", true)) {
            m.switchMode(isMap ? DOC_TILE : DOC_MAP);
            done = true;
        }
        y += stp;
        if (uiButton(bx, y, bw, bh, "Настройки", true)) showSettingsScreen();
        y += stp;
        if (uiButton(bx, y, bw, bh, "Справка и благодарности", true)) showHelpScreen();
        y += stp;
        if (uiButton(bx, y, bw, bh, "Выход", true)) commandExit();

        uiLabel(0, m.H - 26, WIN_W, 22,
                std::string("версия ") + kAppVersion +
                    "   ·   настройки в Config.txt   ·   Escape закрывает меню",
                cTextDim);
        redraw();
        if (takeKey() == KEY_ESC) done = true;
        idle();
    }
    waitMouseRelease();
    m.needRepaint = true;
}

// ---------------------------------------------------------------------------
// Наборы тайлов Andors-Love
//
// Игра ищет картинки по имени файла: data/tiles/wall.png и так далее.
// Семнадцать основных слотов названы так же и в том же порядке, что в
// src/gfx/tiles.cpp игры, — это договор между проектами. Сверх них у
// каждого жителя и врага может быть своя картинка, npc_elder.png. Их
// состав редактор не хранит: он смотрит, какие файлы лежат в папке, — папка
// и есть список, и вторая его копия здесь однажды отстала бы от игры.
// ---------------------------------------------------------------------------

std::string Editor::tilesDir() const {
    const std::string r = resolvePath(cfg.tilesFolder);
    return (!r.empty() && dirExists(r)) ? r : std::string();
}

/// Папка значков — своя, а не общая с игрой: значки — имущество редактора.
std::string Editor::iconsDir() const {
    const std::string r = resolvePath(cfg.iconsFolder);
    return (!r.empty() && dirExists(r)) ? r : std::string();
}

std::string Editor::tsetDirOf(int k) const {
    return k == TS_ICON ? iconsDir() : tilesDir();
}

std::string Editor::tsetPathOf(int k, const std::string& base) const {
    if (base.empty()) return {};
    const std::string dir = tsetDirOf(k);
    return dir.empty() ? std::string() : pathCombine(dir, base + ".png");
}

/// Перечитывает значки кнопок; отсутствующий файл оставляет место пустым.
void Editor::ensureIcons() {
    if (!iconsDirty_) return;
    iconsDirty_ = false;
    iconBmp_.assign(static_cast<std::size_t>(iconCount()), Grid<Color>{});
    for (int i = 0; i < iconCount(); ++i) {
        const std::string path = tsetPathOf(TS_ICON, iconFile(i));
        if (path.empty() || !fileExists(path)) continue;
        Grid<Color> g;
        if (!readImageGrid(path, g, true)) continue;
        iconBmp_[static_cast<std::size_t>(i)] =
            gridFromBgra(makeThumbAlphaRaw(g, ICON_PX), ICON_PX);
    }
    m.needRepaint = true;
}

void Editor::tsetFree() {
    tsetBmp_.clear();
    tsetGrid_.clear();
}

/// Перечитывает один тайл набора. Отсутствующий файл — не ошибка.
void Editor::tsetLoadOne(int i) {
    if (i < 0 || i >= static_cast<int>(tsetFiles_.size())) return;
    if (tsetBmp_.size() != tsetFiles_.size()) return;
    const auto k = static_cast<std::size_t>(i);
    tsetBmp_[k].clear();
    tsetGrid_[k].clear();
    const std::string path = tsetPathOf(tsetKind_, tsetFiles_[k]);
    if (path.empty() || !fileExists(path)) return;
    Grid<Color> g;
    if (!readImageGrid(path, g, true)) return;
    tsetBmp_[k] = gridFromBgra(makeThumbRaw(g, TSET_THUMB), TSET_THUMB);
    tsetGrid_[k] = std::move(g);
}

void Editor::tsetBuild() {
    tsetFree();
    tsetFiles_.clear();
    tsetLabels_.clear();
    if (tsetKind_ == TS_BASE) {
        for (int i = 0; i < TSET_BASE_COUNT; ++i) {
            tsetFiles_.push_back(tsetBaseFile(i));
            tsetLabels_.push_back(tsetBaseName(i));
        }
    } else if (tsetKind_ == TS_ICON) {
        for (int i = 0; i < iconCount(); ++i) {
            tsetFiles_.push_back(iconFile(i));
            tsetLabels_.push_back(iconName(i));
        }
    } else {
        const std::vector<std::string> found =
            scanPrefixed(tilesDir(), tsetKind_ == TS_NPC ? "npc_" : "mob_");
        for (const std::string& f : found) {
            tsetFiles_.push_back(f);
            // Подпись — id существа без приставки: она и так видна в имени
            // файла под миниатюрой.
            tsetLabels_.push_back(f.substr(4));
        }
    }
    tsetGrid_.assign(tsetFiles_.size(), Grid<Color>{});
    tsetBmp_.assign(tsetFiles_.size(), Grid<Color>{});
    for (int i = 0; i < static_cast<int>(tsetFiles_.size()); ++i)
        tsetLoadOne(i);
    if (tsetPage_ * TSET_PAGE >= static_cast<int>(tsetFiles_.size())) tsetPage_ = 0;
}

int Editor::tsetPages() const {
    return std::max(1, (static_cast<int>(tsetFiles_.size()) + TSET_PAGE - 1) / TSET_PAGE);
}

int Editor::tsetCellX(int k) {
    return TSET_X0 + (k % TSET_COLS) * TSET_CELL_W;
}

int Editor::tsetCellY(int k) {
    return TSET_Y0 + (k / TSET_COLS) * TSET_CELL_H;
}

int Editor::tsetAt(int k) const {
    const int r = tsetPage_ * TSET_PAGE + k;
    return (r < 0 || r >= static_cast<int>(tsetFiles_.size())) ? -1 : r;
}

bool Editor::tsetHit(int px, int py, int& slot) const {
    slot = -1;
    if (px < TSET_X0 || py < TSET_Y0) return false;
    const int col = (px - TSET_X0) / TSET_CELL_W;
    const int row = (py - TSET_Y0) / TSET_CELL_H;
    if (col < 0 || col >= TSET_COLS || row < 0 || row >= TSET_ROWS) return false;
    const int k = tsetAt(row * TSET_COLS + col);
    if (k < 0) return false;
    slot = k;
    return true;
}

/// Экран наборов. true — тайл открыт в редакторе, и вызывающему стоит уйти
/// из меню в редактор, как после обычной загрузки.
bool Editor::showTilesetScreen() {
    bool result = false;
    tsetBuild();
    bool done = false;
    std::string note;
    dropInput();
    pf_.input().keys.clear();

    while (!done && !quitNow_) {
        beginFrameInput();
        const std::string dir = tsetDirOf(tsetKind_);
        fb_.clear(cWindow);
        textCentered(0, 6, WIN_W, 40, "Наборы тайлов", cText);

        // Переключатель наборов. Открытый помечен точкой: кнопка не умеет
        // показывать нажатое состояние, а знать, где находишься, нужно.
        for (int i = 0; i < TS_KINDS; ++i) {
            const std::string cap = i == tsetKind_ ? "• " + tsetKindName(i) : tsetKindName(i);
            if (uiButton(TSET_X0 + i * 192, 46, 184, 28, cap, i != tsetKind_)) {
                tsetKind_ = i;
                tsetPage_ = 0;
                tsetBuild();
                note.clear();
            }
        }

        if (dir.empty()) {
            uiLabel(0, 78, WIN_W, 18,
                    tsetKind_ == TS_ICON ? "Папка значков не найдена — создайте её кнопкой внизу"
                                         : "Папка набора не задана — укажите data/tiles игры",
                    cWarn);
        } else {
            uiLabel(0, 78, WIN_W, 18, fitText(dir, WIN_W - 40), cTextDim);
        }
        uiLabel(0, 96, WIN_W, 18, "Правая кнопка открывает тайл, левая пишет на его место текущий",
                cTextDim);

        for (int i = 0; i < TSET_PAGE; ++i) {
            const int k = tsetAt(i);
            if (k < 0) continue;
            const auto kk = static_cast<std::size_t>(k);
            const int cx = tsetCellX(i);
            const int cy = tsetCellY(i);
            const int tx = cx + (TSET_CELL_W - TSET_THUMB) / 2;
            fb_.fillRect(tx, cy + 2, tx + TSET_THUMB, cy + 2 + TSET_THUMB, cFace);
            if (!tsetBmp_[kk].empty()) {
                fb_.drawImage(tsetBmp_[kk], tx, cy + 2);
            } else {
                uiLabel(cx, cy + 18, TSET_CELL_W, 16, "нет", cTextDim);
            }
            // Место под курсором обведено цветом выделения: попасть в нужное
            // из двух десятков мелких клеток иначе трудно.
            const bool over = mouseX() >= cx && mouseX() < cx + TSET_CELL_W && mouseY() >= cy &&
                              mouseY() < cy + TSET_CELL_H;
            fb_.frameRect(tx - 1, cy + 1, tx + TSET_THUMB + 1, cy + 3 + TSET_THUMB,
                          over ? cAccent : cBorder);
            const bool object = tsetKind_ == TS_BASE && !tsetBaseIsTerrain(k);
            uiLabel(cx, cy + 52, TSET_CELL_W, 16, fitText(tsetLabels_[kk], TSET_CELL_W - 4),
                    object ? cAccent : cText);
            uiLabel(cx, cy + 68, TSET_CELL_W, 16, fitText(tsetFiles_[kk] + ".png", TSET_CELL_W - 4),
                    cTextDim);
        }

        // Страницы показываются всегда, чтобы кнопки не прыгали.
        if (uiButton(TSET_X0, 398, 90, 28, "Назад", tsetPage_ > 0)) --tsetPage_;
        uiLabel(TSET_X0 + 96, 398, 200, 28,
                "страница " + std::to_string(tsetPage_ + 1) + " из " + std::to_string(tsetPages()) +
                    ",  тайлов " + std::to_string(tsetFiles_.size()),
                cTextDim);
        if (uiButton(TSET_X0 + 306, 398, 90, 28, "Дальше", tsetPage_ < tsetPages() - 1))
            ++tsetPage_;

        if (!note.empty()) uiLabel(0, 430, WIN_W, 18, note, cAccent);

        if (uiButton(WIN_W / 2 - 230, 456, 220, 32, "Папка набора…", true)) {
            const std::string path = askOpenFile("Выберите любой тайл в нужной папке");
            if (!path.empty()) {
                if (tsetKind_ == TS_ICON) {
                    cfg.iconsFolder = pathDirName(path);
                    iconsDirty_ = true;
                } else {
                    cfg.tilesFolder = pathDirName(path);
                }
                note.clear();
                saveConfig();
                tsetBuild();
            }
        }
        if (uiButton(WIN_W / 2 + 10, 456, 220, 32, "Закрыть", true)) done = true;

        uiLabel(0, m.H - 44, WIN_W, 18,
                "Пустое место игра рисует по-старому — набор можно делать по одному тайлу",
                cTextDim);
        uiLabel(0, m.H - 26, WIN_W, 18,
                "Состав жителей и врагов читается из самой папки: что в ней лежит, то и здесь",
                cTextDim);

        int slot = -1;
        if (frameClick_ && tsetHit(frameClickX_, frameClickY_, slot)) {
            frameClick_ = false;
            const auto ks = static_cast<std::size_t>(slot);
            const std::string path = tsetPathOf(tsetKind_, tsetFiles_[ks]);
            if (path.empty()) {
                note = "Сначала укажите папку";
            } else if (frameClickButton_ == MB_RIGHT) {
                if (!fileExists(path)) {
                    note = "Файла " + tsetFiles_[ks] + ".png ещё нет";
                } else if (confirmDiscard("Открыть тайл " + tsetFiles_[ks] + "?") &&
                           loadTileFrom(path, false, true)) {
                    result = true;
                    done = true;
                }
            } else if (frameClickButton_ == MB_LEFT) {
                if (m.docMode != DOC_TILE) {
                    note = "Записать можно только тайл, а сейчас открыта карта";
                } else if (fileExists(path) &&
                           !askYesNo("Заменить тайл",
                                     "Файл " + tsetFiles_[ks] +
                                         ".png уже есть. Заменить его текущим тайлом " +
                                         std::to_string(m.tile.w) + " на " +
                                         std::to_string(m.tile.h) + "?",
                                     "Заменить", "Отмена")) {
                    note.clear();
                } else if (saveTileTo(path)) {
                    tsetLoadOne(slot);
                    note = "Записано: " + tsetFiles_[ks] + ".png";
                    // Значок на кнопке должен показать нарисованное сразу,
                    // иначе непонятно, получилось ли.
                    if (tsetKind_ == TS_ICON) iconsDirty_ = true;
                }
            }
        }
        redraw();
        if (takeKey() == KEY_ESC) done = true;
        idle();
    }
    tsetFree();
    waitMouseRelease();
    m.needRepaint = true;
    return result;
}

}  // namespace tjtm
