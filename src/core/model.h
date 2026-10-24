// Состояние и логика редактора без окна: перенос всего, что в TJTM.pas
// не рисует и не ждёт ввода.
//
// Здесь документы обоих режимов, история отмен, кисть, фигуры, заливка,
// выделение и буферы обмена, палитра с привязкой пикселей, панорама и
// масштаб, геометрия панели и обзора. Отрисовка и экраны живут в app/ и
// опираются на эти поля и функции так же, как процедуры оригинала
// опирались на его глобальные переменные. Отделено это затем, чтобы
// сверять логику с оригиналом тестами, без окна и без SDL.
//
// Имена повторяют оригинал: PaintOne — paintOne, FloodFillTile —
// floodFillTile. По ним перенос сличается с исходником строка к строке.
#pragma once

#include <cstdint>
#include <functional>
#include <random>
#include <string>
#include <vector>

#include "core/color.h"
#include "core/grid.h"
#include "core/mapdoc.h"
#include "core/png.h"
#include "core/sprite.h"
#include "core/theme.h"

namespace tjtm {

inline constexpr const char* kAppTitle = "True Joint Tile Maker";
// 0.7 — перенос 0.6.0 на C++ с исправлениями из docs/fixes.md. Этапы
// RoadToProLevel поднимают младший номер; 1.0 — все четыре этапа плана,
// 1.1 и дальше — разделы плана сверх них (docs/roadmap.md).
inline constexpr const char* kAppVersion = "1.2.0";

// --- раскладка окна ---------------------------------------------------------

/// Холст редактирования и панель справа от него.
inline constexpr int CANVAS_W = 600;
inline constexpr int CANVAS_H = 560;
inline constexpr int PANEL_X = CANVAS_W;
inline constexpr int PANEL_W = 180;
inline constexpr int WIN_W = CANVAS_W + PANEL_W;
inline constexpr int WIN_H = CANVAS_H;

/// Ширина линеек слева и сверху холста (этап 5).
inline constexpr int RULER_SIZE = 20;

inline constexpr int MIN_TILE = 4;
inline constexpr int MAX_TILE = 512;

inline constexpr int PAL_COLS = 8;
inline constexpr int PAL_ROWS = 8;
inline constexpr int PAL_SIZE = PAL_COLS * PAL_ROWS;

/// Вертикальная разметка панели. Отрисовка и попадание мыши считают
/// границы по одним и тем же константам.
inline constexpr int TOOLS_Y0 = 4, TOOLS_Y1 = 112;
inline constexpr int SLOT1_Y0 = 118, SLOT1_Y1 = 246;
inline constexpr int SLOT2_Y0 = 250, SLOT2_Y1 = 350;
inline constexpr int HUE_Y0 = 352, HUE_Y1 = 364;
inline constexpr int ALPHA_Y0 = 366, ALPHA_Y1 = 378;
inline constexpr int OVER_Y0 = 384, OVER_Y1 = 480;
inline constexpr int INFO_Y0 = 484, INFO_Y1 = 528;
inline constexpr int MENU_Y0 = 532, MENU_Y1 = 556;

/// В режиме тайла SLOT1 занимает палитра, SLOT2 — квадрат насыщенности;
/// в режиме карты SLOT1 — виды местности, SLOT2 — виды объектов.
inline constexpr int PAL_Y0 = SLOT1_Y0, PAL_Y1 = SLOT1_Y1;
inline constexpr int HSV_Y0 = SLOT2_Y0, HSV_Y1 = SLOT2_Y1;

inline constexpr int TOOL_COLS = 4;
inline constexpr int TOOL_CW = 44;
inline constexpr int TOOL_CH = 36;
/// Сторона значка на кнопке инструмента: значок любого размера
/// показывается уменьшенным до неё.
inline constexpr int ICON_PX = 28;

inline constexpr int BAR_SIZE = 100;
inline constexpr int BAR_H = 12;

inline constexpr int OVER_W = PANEL_W - 4;
inline constexpr int OVER_H = OVER_Y1 - OVER_Y0;
inline constexpr int PAL_CELL = 16;
inline constexpr int PAL_X0 = PANEL_X + (PANEL_W - PAL_COLS * PAL_CELL) / 2;
inline constexpr int HSV_X0 = PANEL_X + (PANEL_W - BAR_SIZE) / 2;
/// Виды местности и объектов раскладываются в две колонки.
inline constexpr int MSEL_COLS = 2;
inline constexpr int MSEL_CW = 88;
inline constexpr int MTERR_CH = 32;
inline constexpr int MOBJ_CH = 25;

// --- инструменты и режимы ---------------------------------------------------

inline constexpr int T_PEN = 0, T_ERASER = 1, T_FILL = 2, T_PICK = 3, T_LINE = 4, T_RECT = 5,
                     T_ELLIPSE = 6, T_DITHER = 7, T_SELECT = 8, T_TILE_COUNT = 9;
inline constexpr int M_PAINT = 0, M_FILL = 1, M_RECT = 2, M_OBJ = 3, M_SELECT = 4, T_MAP_COUNT = 5;
inline constexpr int DOC_TILE = 0, DOC_MAP = 1;

/// Преобразования картинки или содержимого выделения (этап 1).
inline constexpr int TF_FLIP_H = 0, TF_FLIP_V = 1, TF_ROT90 = 2, TF_ROT180 = 3, TF_ROT270 = 4;
/// Вид линии инструмента «Линия» (этап 6).
inline constexpr int LINE_STRAIGHT = 0, LINE_POLY = 1, LINE_POLYGON = 2, LINE_CURVE = 3,
                     LINE_CLOSED_CURVE = 4, LINE_STAR = 5, LINE_KINDS = 6;
/// Вид выделения: рамка, овал, лассо, волшебная палочка, по цвету,
/// многоугольник по вершинам (этап 5).
inline constexpr int SS_RECT = 0, SS_ELLIPSE = 1, SS_LASSO = 2, SS_WAND = 3, SS_COLOR = 4,
                     SS_POLYGON = 5, SS_COUNT = 6;

inline constexpr int MIN_ZOOM = 1;
inline constexpr int MAX_ZOOM = 64;
/// Верхняя граница памяти под историю отмен.
inline constexpr int UNDO_BUDGET_BYTES = 6 * 1024 * 1024;
/// Шаг бега «муравьёв» по рамке выделения, мс.
inline constexpr int ANTS_STEP_MS = 90;
/// Промежуток, в который два щелчка по одной клетке — двойной.
inline constexpr int DOUBLE_CLICK_MS = 450;

// --- наборы тайлов ----------------------------------------------------------

/// Основных слотов игры. Их имена — часть договора с src/gfx/tiles.cpp игры.
inline constexpr int TSET_BASE_COUNT = 17;
inline constexpr int TS_BASE = 0, TS_NPC = 1, TS_MOB = 2, TS_ICON = 3, TS_KINDS = 4;

std::string tsetBaseFile(int i);
std::string tsetBaseName(int i);
/// Местность рисуется непрозрачной, объекты — с прозрачным фоном.
bool tsetBaseIsTerrain(int i);
std::string tsetKindName(int k);
int iconCount();
std::string iconFile(int i);
std::string iconName(int i);
/// Имена файлов каталога без расширения .png, начинающиеся с prefix.
std::vector<std::string> scanPrefixed(const std::string& dir, const std::string& prefix);

/// Цвет местности на карте.
Color mapTileColor(int code);
/// Знак, которым объект показан на карте; взяты обозначения игры.
std::string mapObjGlyph(const std::string& kind);

/// Разбивает строку на слова по пробелам.
std::vector<std::string> splitWords(const std::string& s);
/// Разбивает текст на непустые строки по CR и LF.
std::vector<std::string> splitLines(const std::string& s);

/// Миниатюры. Перебор по пикселям приёмника, ближайший сосед, BGRA.
/// С шахматкой — для набора тайлов, где объект с прозрачным фоном иначе
/// выглядел бы пустым; без неё — для значка, который ложится на кнопку.
std::vector<std::uint8_t> makeThumbRaw(const Grid<Color>& g, int size);
std::vector<std::uint8_t> makeThumbAlphaRaw(const Grid<Color>& g, int size);

// --- модель -----------------------------------------------------------------

/// Снимок документа тайла для истории. Привязки и палитра ходят по
/// истории вместе с цветами: в индексном режиме правка ячейки меняет тайл,
/// и отменять их порознь бессмысленно.
struct TileSnapshot {
    Grid<Color> tile;
    Grid<PalIndex> idx;
    std::vector<Color> palette;
    PalIndex transparentIdx = IDX_NONE;
    /// Слои и кадры: таблица общих картинок, копируется дёшево. Отмена
    /// возвращает и их, и активную ячейку — правку кадра 3 отменяют,
    /// стоя на кадре 5.
    SpriteDoc doc;
    int layer = 0;
    int frame = 0;
    /// Активная ячейка векторного слоя и тайлкарты (этап 6).
    std::vector<VecShape> shapes;
    Grid<int> tileMap;
};

/// Какая команда идёт по всем ячейкам (forEachCel): цветовая меняет
/// точки на месте, геометрическая — их положение.
inline constexpr int CEL_COLOR = 0, CEL_GEOMETRY = 1;

class Model {
public:
    Model();

    // Размеры холста. Переменные, а не константы: окно со временем
    // может стать больше.
    int W = CANVAS_W;
    int H = CANVAS_H;

    int docMode = DOC_TILE;

    // Документ режима тайла.
    Grid<Color> tile;
    /// Привязка пикселей к палитре: номер ячейки или IDX_NONE; того же
    /// размера, что tile.
    Grid<PalIndex> tileIdx;
    std::vector<Color> palette;
    /// Индексный режим: правка ячейки перекрашивает связанные пиксели.
    bool indexedOn = false;

    // Документ режима карты.
    MapDoc map;

    /// Буфер обмена цветов. Живёт отдельно от выделения и переживает смену тайла.
    Grid<Color> buffer;
    /// Буфер обмена клеток карты.
    Grid<std::uint8_t> mapBuffer;

    /// Прямоугольник выделения в координатах документа.
    int selX = 0, selY = 0, selW = 0, selH = 0;
    bool hasSelection = false;
    bool draggingSelection = false;

    // Панорама и масштаб, общие для обоих режимов.
    int pixelSize = 8;
    int viewX = 0, viewY = 0;
    bool gridOn = false;
    bool checkerOn = true;
    bool seamlessOn = false;
    bool wrapOn = false;
    bool symX = false, symY = false;
    /// Кисть пишет цвет поверх, не смешивая. Ластик всегда работает так.
    bool brushReplace = false;

    // --- этап 1 RoadToProLevel ---
    //
    // Значения по умолчанию — поведение оригинала: на них стоят эталонные
    // тесты 0.6.0. Окно включает новое при запуске (Editor::appInit).

    /// Маска выделения размером с документ. Пустая — выделение
    /// прямоугольное (selX, selY, selW, selH) или его нет: рамка остаётся
    /// рамкой, и путь оригинала для неё не меняется.
    Grid<std::uint8_t> selMask;
    /// Последнее снятое выделение — для «Вернуть выделение».
    Grid<std::uint8_t> lastSelMask;
    /// Кисть, фигуры и заливка не выходят за выделение. В оригинале
    /// выделение рисование не ограничивало.
    bool clipToSelection = false;
    /// «Новый» и «Очистить» дают прозрачный холст, а не белый.
    bool transparentBlank = false;
    /// Мазок без углов «Г»: карандаш, ластик и узор.
    bool pixelPerfect = false;
    /// Узор инструмента «Узор»; 0 — шахматка оригинала.
    int ditherPattern = 0;
    /// Оси симметрии в половинах точки: зеркало x — это axis - 1 - x.
    /// -1 — середина документа, как в оригинале.
    int symAxisX2 = -1, symAxisY2 = -1;
    /// Вид выделения и действие по умолчанию (img::SelOp).
    int selShape = SS_RECT;
    int selOp = 0;
    /// Вставленное «плавает»: пока картинку и выделение после вставки
    /// никто не трогал, повторное вынимание возвращает то, что было под
    /// вставкой, а не прозрачность. Проверка — сравнением с результатом.
    Grid<Color> floatUnder, floatResult;

    // --- этап 5: точность ---

    /// Радиальная симметрия: столько секторов вокруг точки пересечения
    /// осей. 0 и 1 — её нет.
    int symRadial = 0;
    /// Непрозрачность кисти, 1..255. Точка мазка ложится один раз, поэтому
    /// повторный проход по ней в том же мазке не сгущает цвет.
    int brushOpacity = 255;
    /// Начало холста в окне: справа и снизу его край — W и H, слева и
    /// сверху — линейки, когда они видны.
    int X0 = 0, Y0 = 0;
    bool rulersOn = false;
    bool guidesOn = true;
    /// Свои направляющие в половинах точки: 2·x — граница перед точкой x.
    std::vector<int> guidesX2, guidesY2;
    /// Направляющие через середину документа.
    bool guideCenter = false;
    /// Крупная сетка: клетка в точках, 0 — её нет. Для листа тайлов 16x16
    /// это границы тайлов.
    int majorGridW = 0, majorGridH = 0;
    /// Направляющие через середины клеток крупной сетки.
    bool guideCellCenters = false;
    /// Начало координат линеек и строки координат.
    int originX = 0, originY = 0;
    /// Пары «ключ = значение» документа: слот набора тайлов, откуда он
    /// открыт, и всё, что проект принёс и чего эта версия не знает.
    std::vector<std::pair<std::string, std::string>> docMeta;
    std::string metaValue(const std::string& key) const;
    void setMeta(const std::string& key, const std::string& value);
    Grid<PalIndex> floatUnderIdx;
    Grid<std::uint8_t> floatSel;

    // --- этап 2: палитра произвольного размера ---

    /// Прозрачный номер палитры: точки с ним прозрачны, ластик в индексном
    /// режиме пишет его. IDX_NONE — такого номера нет.
    PalIndex transparentIdx = IDX_NONE;
    /// Строгий индексный режим: при indexedOn каждая точка хранит номер, а
    /// цвет — из палитры. Оригинал лишь привязывал совпавшие точки.
    bool strictIndexed = false;
    /// Прокрутка палитры на панели, в рядах по PAL_COLS.
    int palScroll = 0;

    // --- этапы 3 и 4: слои и кадры ---

    /// Слои, кадры, метки. Картинка активной ячейки — tile.
    SpriteDoc doc;
    int activeLayer = 0;
    int activeFrame = 0;
    /// «Луковая кожа»: соседние кадры полупрозрачно под текущим.
    bool onionSkin = false;
    int onionPrev = 1;
    int onionNext = 1;

    Color firstColor = argb(255, 0, 0, 0);
    Color secondColor = argb(255, 255, 255, 255);
    /// Ячейки палитры, из которых взяты рабочие цвета: IDX_NONE или
    /// ячейка, чей цвет совпадает с рабочим.
    PalIndex firstIdx = IDX_NONE;
    PalIndex secondIdx = IDX_NONE;
    int hue = 0, sat = 0, val = 0;

    int toolTile = T_PEN;
    int toolMap = M_PAINT;
    int mapBrush = MT_GRASS;
    int mapObjKind = 0;

    std::string currentFile;
    std::string currentMapFile;
    /// Несохранённые правки — для каждого документа отдельно.
    bool tileModified = false;
    bool mapModified = false;

    std::vector<TileSnapshot> undoStack, redoStack;
    std::vector<MapDoc> mapUndo, mapRedo;

    // --- этап 6: активная ячейка векторного слоя и тайлкарты ---
    /// Фигуры активной ячейки векторного слоя; tile — их растр.
    std::vector<VecShape> tileShapes;
    /// Номера тайлов активной ячейки тайлкарты; tile — их отрисовка.
    Grid<int> tileMap;
    /// Тайлкарта правится точками: правка уходит в набор и меняет все копии
    /// тайла. Иначе кисть кладёт тайл tileBrush целиком.
    bool tilemapPixels = false;
    int tileBrush = 1;
    /// Вид линии: прямая (как в оригинале), ломаная, многоугольник, кривая
    /// через точки, замкнутая кривая, звезда (этап 6).
    int lineKind = 0;
    /// Толщина, заливка вторым цветом и звезда — у фигур и векторных слоёв.
    int shapeWidth = 1;
    bool shapeFill = false;
    int starPoints = 5;
    int starInner = 50;

    /// Маска текущего штриха: пиксель закрашивается не больше раза,
    /// иначе полупрозрачная кисть накладывалась бы сама на себя.
    Grid<std::uint8_t> strokeMask;
    /// Мазок «пиксель-перфект»: его точки по порядку и картинка до него —
    /// из неё возвращается точка, оказавшаяся углом «Г».
    std::vector<std::pair<int, int>> strokePts;
    Grid<Color> strokeBase;
    Grid<PalIndex> strokeBaseIdx;
    bool strokeActive = false;

    // Признаки для отрисовки: что пересобрать в следующем кадре.
    bool tileBmpDirty = true;
    bool overDirty = true;
    bool needRepaint = true;
    /// Штрих рисуется сразу на холсте, пиксель за пикселем.
    bool liveDraw = false;
    /// Фигура рисуется предпросмотром, а не в документ.
    bool shapePreview = false;

    /// Отрисовка одного пикселя при штрихе и клетки предпросмотра фигуры.
    /// Их ставит окно; в тестах они пустые.
    std::function<void(int, int)> drawTilePixel;
    std::function<void(int, int)> drawPreviewPixel;

    std::mt19937 rng{std::random_device{}()};

    // --- координаты документа ---
    int docW() const;
    int docH() const;
    bool isModified() const;
    void markModified();
    int tileToScreenX(int tx) const { return viewX + tx * pixelSize; }
    int tileToScreenY(int ty) const { return viewY + ty * pixelSize; }
    int screenToTileX(int sx) const;
    int screenToTileY(int sy) const;
    /// Единственная проверка границ, которой пользуются все операции.
    bool inDoc(int x, int y) const;
    int gridGap() const;
    void clampView();
    void fitScale();
    /// Масштаб вокруг точки экрана: клетка под курсором остаётся на месте.
    void zoomAt(int dir, int ax, int ay);
    void resetSelection();

    // --- палитра и привязка ---
    static PalIndex palIndexAt(int j, int i) { return static_cast<PalIndex>(i * PAL_COLS + j); }
    Color palColorOf(PalIndex idx) const;
    Color& palAt(int j, int i) { return palette[static_cast<std::size_t>(palIndexAt(j, i))]; }
    Color palAt(int j, int i) const { return palette[static_cast<std::size_t>(palIndexAt(j, i))]; }
    /// Первая ячейка того же цвета без учёта прозрачности или IDX_NONE.
    PalIndex indexOfColor(Color c) const;
    void ensureTileIdx();
    void setIdx(int x, int y, PalIndex v);
    int rebindTileToPalette();
    int recolorPaletteEntry(PalIndex idx, Color oldC, Color newC);
    int boundPixelCount() const;
    void setDefaultPalette();
    /// Заполняет палитру из картинки, растягивая её до 8x8.
    void paletteFromGrid(const Grid<Color>& g);

    // --- история ---
    int undoDepth() const;
    void clearTileHistory();
    void clearMapHistory();
    void clearHistory();
    void pushUndo();
    void doUndo();
    void doRedo();

    // --- документ ---
    void setTile(Grid<Color> newTile, const std::string& fileName);
    void newTileOfSize(int nw, int nh);
    void newMapOfSize(int w, int h);
    void switchMode(int mode);
    Color checkerColor(int x, int y) const;
    Color displayColor(int x, int y) const;

    // --- кисть ---
    Color brushColorAt(int x, int y) const;
    bool brushReplaces() const;
    PalIndex brushIndexAt(int x, int y) const;
    void paintOne(int x, int y);
    void paintPixel(int x, int y);
    void putMapOne(int x, int y);
    void putMapCell(int x, int y);
    void emitPixel(int x, int y);
    /// Точка мазка «пиксель-перфект»: ставится, и если с двумя прежними
    /// она образует угол «Г», средняя возвращается к тому, что было до мазка.
    void emitPixelPerfect(int x, int y);
    /// Начало и конец штриха: маска размером с документ.
    void beginStroke();
    void endStroke();

    // --- фигуры ---
    void shapeLine(int x0, int y0, int x1, int y1);
    void shapeRect(int x0, int y0, int x1, int y1, bool filled);
    void shapeEllipse(int x0, int y0, int x1, int y1, bool filled);
    static void squarifyTo(int ax, int ay, int& bx, int& by);

    // --- заливка ---
    void floodFillTile(int sx, int sy, Color newColor);
    void floodFillMap(int sx, int sy, int newCode);

    // --- выделение и буферы ---
    void copySelectionToBuffer();
    void pasteBufferAt(int px, int py);
    bool bufferFilled() const;
    void bufferFlipH();
    void bufferFlipV();
    void bufferRotate90();
    /// Возвращает буфер в выделение, если размеры совпали. Возвращает,
    /// применилось ли.
    bool applyBufferToSelection();

    // --- операции над тайлом ---
    void clearTile();
    void randomizeTile();
    int replaceColorEverywhere(Color fromC, Color toC);
    PalIndex nearestPaletteIndex(Color c) const;
    Color nearestPaletteColor(Color c) const;
    void quantizeTileToPalette();
    void flipTileH();
    void flipTileV();

    // --- рабочие цвета ---
    void syncHSVFromColor();
    void applyHSVToColor();
    void pickColor(Color c);
    void pickColorFromTile(int x, int y);
    void swapColors();

    // --- геометрия панели ---
    int toolCount() const;
    int curTool() const;
    void setTool(int i);
    static int toolCellX(int i);
    static int toolCellY(int i);
    bool toolHit(int px, int py, int& idx) const;
    static int palCellX(int j) { return PAL_X0 + j * PAL_CELL; }
    static int palCellY(int i) { return PAL_Y0 + i * PAL_CELL; }
    static bool palHit(int px, int py, int& cj, int& ci);
    static int mselX(int i) { return PANEL_X + 2 + (i % MSEL_COLS) * MSEL_CW; }
    static int terrCellY(int i) { return SLOT1_Y0 + (i / MSEL_COLS) * MTERR_CH; }
    static int objCellY(int i) { return SLOT2_Y0 + (i / MSEL_COLS) * MOBJ_CH; }
    static bool terrHit(int px, int py, int& idx);
    static bool objHit(int px, int py, int& idx);

    // --- обзор ---
    double overScale() const;
    int overPixW() const;
    int overPixH() const;
    int overOriginX() const;
    int overOriginY() const;
    void overviewCenterOn(int px, int py);
    /// Картинка обзора, BGRA, overPixW x overPixH.
    std::vector<std::uint8_t> buildOverviewRaw() const;
    /// Рамка видимой области на обзоре; false — рамки нет.
    bool overviewViewport(int& vx0, int& vy0, int& vx1, int& vy1) const;
    /// Картинка тайла с шахматкой под прозрачным, BGRA с прямой альфой.
    std::vector<std::uint8_t> tileBmpRaw() const;

    // --- палитра произвольного размера (этап 2) ---
    int palCount() const { return static_cast<int>(palette.size()); }
    /// Номер ячейки под клеткой (j, i) панели с учётом прокрутки; -1 — пусто.
    int palCellIndex(int j, int i) const;
    int palMaxScroll() const;
    void scrollPalette(int rows);
    /// Как номер выглядит на картинке: прозрачный — с нулевой альфой.
    Color effectiveColor(PalIndex idx) const;
    bool strictIndexedOn() const { return strictIndexed && indexedOn && docMode == DOC_TILE; }
    /// Номер для цвета c, взятого из ячейки bound: сама ячейка, если она
    /// есть, иначе ближайшая; прозрачный цвет — прозрачный номер.
    PalIndex resolveIndex(Color c, PalIndex bound) const;
    /// Цвет заливки: первый цвет, в строгом индексном режиме — его ячейки.
    Color fillColor() const;
    /// Новая палитра на место прежней; oldToNew[k] — новый номер прежней
    /// ячейки k или -1, если её больше нет. Привязки и цвета точек едут
    /// следом; без строгого режима точки удалённой ячейки отвязываются.
    void remapPalette(std::vector<Color> newPal, const std::vector<int>& oldToNew);
    /// Заменить палитру целиком: точки привязываются заново по цвету.
    void setPalette(std::vector<Color> p);
    int addPaletteColor(Color c, int at);
    bool deletePaletteEntry(int idx);
    bool duplicatePaletteEntry(int idx);
    bool movePaletteEntry(int idx, int to);
    void sortPalette(int key, int from, int to);
    /// Ряд из steps цветов от ячейки a до b: промежуточные встают после a.
    /// Возвращает число новых ячеек.
    int insertRamp(int a, int b, int steps, int hueShift);
    void adjustPaletteEntries(int from, int to, int dh, int ds, int dv);
    int extractPaletteFromTile(int maxColors);
    int removeDuplicateEntries();
    void setTransparentIndex(PalIndex idx);
    /// Включить строгий индексный режим. addMissing — цвета, которых нет в
    /// палитре, добавить в неё; иначе свести к ближайшим. Возвращает число
    /// точек, сменивших цвет.
    int enableIndexed(bool addMissing);
    /// Точки без номера в строгом режиме — к ближайшей ячейке. Зовётся
    /// после команд, которые кладут произвольные цвета.
    void enforceIndexed();
    /// То же для одной картинки; возвращает число точек, сменивших цвет.
    int enforceGrid(Grid<Color>& img, Grid<PalIndex>& idx) const;
    /// Сколько непрозрачных точек не совпадает ни с одной ячейкой.
    int unmatchedPixelCount() const;
    /// Тон, насыщенность, яркость точек выделения или всей картинки.
    void adjustImageHSV(int dh, int ds, int dv);

    // --- слои и кадры (этапы 3 и 4) ---
    /// Один слой, один кадр — документ из одной картинки, как в 0.6.0.
    void resetDoc();
    /// Записать tile в таблицу ячеек; пустая картинка — пустая ячейка.
    void commitActiveCel();
    /// Взять картинку активной ячейки в tile.
    void loadActiveCel();
    void setActiveCel(int layer, int frame);
    /// Больше одного слоя или кадра: показ идёт сведением.
    bool multiCel() const { return doc.layerCount() > 1 || doc.frameCount() > 1; }
    /// Активный слой виден и не заперт: на нём можно рисовать.
    bool activeLayerEditable() const;
    /// Применить правку ко всем ячейкам документа, включая активную. Так
    /// работают команды над всей картинкой: размер холста, поворот, палитра.
    /// Цветовая правка идёт и по цветам векторных фигур, и по тайлам наборов;
    /// геометрическая двигает точки фигур через shapeFn, а без него — и
    /// всегда у тайлкарты, чья сетка после неё не совпадёт, — переводит
    /// слой в растр.
    void forEachCel(const std::function<void(Grid<Color>&, Grid<PalIndex>&)>& fn,
                    int what = CEL_COLOR, const std::function<void(VecShape&)>& shapeFn = {});
    /// Пройти по картинкам всех ячеек, ничего не меняя.
    void forEachCelConst(const std::function<void(const Grid<Color>&)>& fn) const;
    /// Видимые слои кадра f, сведённые с учётом непрозрачности слоя.
    Grid<Color> compositeFrame(int f) const;
    Color compositeAt(int f, int x, int y) const;
    /// Сведение слоёв from..to кадра f с группами (этап 6).
    Color compositeLayers(int f, int from, int to, int x, int y) const;
    /// То, что видно на холсте в точке: сведение и «луковая кожа», без шахматки.
    Color viewColorAt(int x, int y) const;
    int addLayer();
    bool deleteLayer();
    int duplicateLayer();
    bool moveLayer(int dir);
    bool mergeLayerDown();
    void setLayerVisible(int l, bool v);
    void setLayerLocked(int l, bool v);
    void renameLayer(int l, const std::string& name);
    void setLayerOpacity(int l, int opacity);
    /// Новый кадр после активного: пустой или копия (ячейки общие).
    int addFrame(bool copy);
    bool deleteFrame();
    bool moveFrame(int dir);
    void setFrameDuration(int f, int ms);
    /// Длительность кадров from..to разом, одним шагом истории.
    void setFramesDuration(int from, int to, int ms);
    int addTag(const std::string& name, int from, int to, int direction);
    bool deleteTag(int i);
    bool setTag(int i, const AnimTag& t);
    /// Первая метка, в которую входит кадр f, или -1.
    int tagOfFrame(int f) const;
    /// Сведённые кадры from..to с их длительностями — для экспорта.
    std::vector<AnimFrame> animationFrames(int from, int to) const;
    /// Документ из кадров анимации (APNG, GIF): один слой, кадры с их
    /// длительностями. Размер — по первому кадру.
    void setAnimation(const std::vector<AnimFrame>& frames, const std::string& fileName);
    // --- этап 6: группы, вектор, тайлкарта (model_layers.cpp) ---
    int activeKind() const;
    /// Группа, в которой лежит слой, или -1.
    int layerParent(int l) const;
    /// Нижний слой поддерева группы; у прочих слоёв — сам слой.
    int subtreeStart(int l) const;
    /// Слой и все его группы видны / слой или одна из его групп заперты.
    bool layerShownDeep(int l) const;
    bool layerLockedDeep(int l) const;
    /// Строка слоя видна на ленте: ни одна его группа не свёрнута.
    bool layerRowVisible(int l) const;
    void setLayerCollapsed(int l, bool c);
    /// Сгруппировать активный слой с поддеревом; возвращает группу.
    int groupLayer();
    bool ungroupLayer();
    /// В соседнюю группу над слоем верхним ребёнком / из своей группы.
    bool indentLayer();
    bool outdentLayer();
    /// Группа → один растровый слой с её видом.
    bool flattenGroup();
    int addVectorLayer();
    /// Векторный слой или тайлкарта становятся растровым слоем.
    bool rasterizeLayer();
    /// Фигура на векторный слой — правимой, на растровый — точками.
    /// Возвращает номер фигуры или -1.
    int addVectorShape(const VecShape& s);
    /// Заменить фигуру без шага истории (его кладут в начале протяжки).
    void setVectorShape(int i, const VecShape& s);
    bool deleteVectorShape(int i);
    void rerenderActiveVector();
    int addTilemapLayer(int tw, int th);
    /// Растровый слой → тайлкарта: одинаковые клетки — один тайл.
    bool convertLayerToTilemap(int tw, int th);
    void rerenderTileset(int t);
    /// Правка тайлкарты точками уходит в набор (зовётся перед показом).
    void syncActiveTilemap();
    const Tileset* activeTileset() const;
    int addTileToActiveSet(const Grid<Color>& g);
    /// Тайлы разом, одним шагом истории; повторы не добавляются.
    int addTilesToActiveSet(const std::vector<Grid<Color>>& tiles);
    bool deleteTileFromActiveSet(int i);
    int tileIndexAt(int x, int y) const;
    void setTileCell(int cx, int cy, int v);
    void stampTileAt(int x, int y);
    void fillTilesAt(int x, int y);
    /// Следующий кадр проигрывания внутри метки tag (-1 — все кадры).
    /// pingDir — направление для «туда-обратно», меняется на краях.
    int nextPlayFrame(int f, int tag, int& pingDir) const;

    // --- симметрия ---
    int mirrorX(int x) const;
    int mirrorY(int y) const;
    /// Точка и все её отражения и повороты по включённой симметрии, в том
    /// порядке, в каком их рисовал оригинал; повороты — после.
    void forEachSymmetric(int x, int y, const std::function<void(int, int)>& fn) const;
    /// Ось там, где её рисовать на холсте, в половинах точки.
    int axisX2() const { return symAxisX2 < 0 ? docW() : symAxisX2; }
    int axisY2() const { return symAxisY2 < 0 ? docH() : symAxisY2; }

    // --- выделение произвольной формы (этап 1) ---
    bool isSelected(int x, int y) const;
    /// Маска выделения размером с документ, из рамки или из selMask.
    Grid<std::uint8_t> selectionMask() const;
    /// Принять маску как выделение: рамка пересчитывается, прямоугольная
    /// маска хранится рамкой. Пустая маска снимает выделение.
    void setSelectionMask(const Grid<std::uint8_t>& mask);
    /// Сочетать новую область с выделением (img::SelOp).
    void applySelection(const Grid<std::uint8_t>& mask, int op);
    void selectAll();
    /// Снять выделение, запомнив его для reselect.
    void deselect();
    void invertSelection();
    bool reselect();
    void growSelection(int n);
    void selectByColorAt(int x, int y, bool contiguous, int op);
    void selectOpaque(int op);
    /// Содержимое выделения в его рамке; невыделенное — прозрачное.
    Grid<Color> selectionContent() const;
    /// Скопировать содержимое выделения в буфер.
    bool copySelection();
    /// Выделенное — прозрачным (с отменой).
    bool eraseSelection();
    bool cutSelection();
    /// Вставка, после которой вставленное выделено и его можно двигать.
    void pasteAsSelection(int px, int py);
    /// Вынуть содержимое выделения: картинка и маска в его рамке; при erase
    /// исходное место становится прозрачным. Шаг истории — на вызывающем.
    void liftSelection(bool erase, Grid<Color>& content, Grid<std::uint8_t>& mask);
    /// Положить вынутое в (px, py) и выделить его. Прозрачное не затирает
    /// того, что под ним, — как вставка оригинала.
    void placeFloating(const Grid<Color>& content, const Grid<std::uint8_t>& mask, int px, int py);
    /// Залить выделение или, без выделения, всю картинку (с отменой).
    void fillSelectionOrAll(Color c);
    /// Цвет пустого холста: прозрачный или белый оригинала.
    Color blankColor() const;

    // --- преобразования (этап 1) ---
    /// Отражение или поворот: содержимого выделения, а без него — всей
    /// картинки. Поворот всей картинки меняет её стороны местами.
    void transformImage(int op);
    /// Масштаб ближайшим соседом в num/den: содержимого выделения от его
    /// левого верхнего угла или всей картинки. false — размер вне пределов.
    bool scaleBy(int num, int den);
    bool scaleImageTo(int nw, int nh);
    bool cropToSelection();
    bool cropToContent();
    bool resizeCanvas(int nw, int nh, int anchorX, int anchorY);
    void offsetImage(int dx, int dy);
    int outlineImage(Color c, bool eightWay, bool outside, int thickness = 1);
    int removeIsolatedPixels();
    /// Нарастить или срезать край непрозрачного на точку (этап 5).
    int growImageEdge(bool eightWay);
    int shrinkImageEdge(bool eightWay);
    /// Поворот на любой угол (img::ROT_*): выделенного или всех ячеек.
    /// false — повёрнутое не помещается в MAX_TILE.
    bool rotateImageBy(double degrees, int method, bool expand);
    /// Свести цвета выделенного или всего тайла к n медианным разрезом.
    int reduceImageColors(int n);

    // --- карта и набор тайлов ---
    /// Файл тайла, которым нарисована клетка карты, без расширения.
    std::string mapCellTileFile(int x, int y) const;
};

}  // namespace tjtm
