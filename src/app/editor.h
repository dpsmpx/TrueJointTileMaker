// Редактор целиком: окно, ввод, отрисовка и экраны — перенос TJTM.pas.
//
// Состояние документов и логика без окна живут в Model (core/model.h);
// здесь всё, что рисует и ждёт ввода. Устройство повторяет оригинал:
// главный цикл (appStep), экраны — вложенные циклы, которые сами разбирают
// ввод и сами рисуются, инструменты — циклы, пока нажата кнопка мыши.
// Имена процедур те же, что в TJTM.pas, — по ним перенос сличается с
// исходником.
#pragma once

#include <array>
#include <map>
#include <string>
#include <vector>

#include "core/config.h"
#include "core/grid.h"
#include "core/model.h"
#include "gfx/font.h"
#include "gfx/surface.h"
#include "platform/platform.h"

namespace tjtm {

class Editor {
public:
    Editor();

    /// Запуск: окно, настройки, документы. false — запуск не удался.
    bool appInit(int argc, char** argv);
    /// Один шаг главного цикла.
    void appStep();
    [[nodiscard]] bool running() const { return !quitNow_; }

    Model m;
    Config cfg;

private:
    // --- платформа и кадр ---
    Platform pf_;
    gfx::Surface fb_;
    gfx::Font font_;
    gfx::Font mono_;
    std::string configPath_;
    std::string baseDir_;
    bool quitNow_ = false;
    /// Режим без окна для тестов: --frames N.
    int framesLeft_ = -1;
    std::string screenshotPath_;

    // --- ввод на кадр ---
    bool frameClick_ = false;
    int frameClickX_ = 0, frameClickY_ = 0, frameClickButton_ = 0;
    int lastClickMs_ = 0;
    int lastClickCX_ = -100000;
    int lastClickCY_ = -100000;

    // --- кэши отрисовки ---
    Grid<Color> tileBmp_;
    bool tileBmpChecker_ = false;
    Grid<Color> bmpHSV_, bmpHue_, bmpAlpha_;
    int cachedHue_ = -1;
    /// Значки кнопок: пусто там, где файла нет — такая кнопка рисуется
    /// подписью. Сперва девять инструментов тайла, потом пять карты.
    std::vector<Grid<Color>> iconBmp_;
    bool iconsDirty_ = true;
    Grid<Color> bmpOver_;
    int overCacheW_ = -1, overCacheH_ = -1, overCacheMode_ = -1;
    int antsPhase_ = 0;
    int antsMs_ = 0;
    std::string lastTitle_;
    int lastStatX_ = -100000, lastStatY_ = -100000;

    // --- экран наборов тайлов ---
    std::vector<std::string> tsetFiles_, tsetLabels_;
    std::vector<Grid<Color>> tsetGrid_;
    std::vector<Grid<Color>> tsetBmp_;
    int tsetKind_ = TS_BASE;
    int tsetPage_ = 0;

    // --- утилиты (TJTM.pas: «Мелкие утилиты», «Ввод») ---
    void idle();
    void beginFrameInput();
    void dropInput();
    int takeKey();
    int takeWheel();
    void waitMouseRelease();
    [[nodiscard]] bool isKeyPressed(int key) const { return pf_.keyHeld(key); }
    static bool isMiddle(int b) { return b != MB_LEFT && b != MB_RIGHT; }
    int mouseX() { return pf_.input().mouseX; }
    int mouseY() { return pf_.input().mouseY; }
    bool mousePressed() { return pf_.input().mousePressed; }
    void redraw();
    /// Окно закрывают крестиком: оригинал выходил молча, перенос
    /// спрашивает о несохранённом, как пункт «Выход».
    void checkQuit();

    std::string resolvePath(const std::string& p) const;
    static std::string fileNameOnly(const std::string& p);
    void rememberFile(const std::string& p, bool isMap);
    void saveConfig();

    // --- примитивы интерфейса ---
    bool uiButton(int x, int y, int bw, int bh, const std::string& caption, bool enabled);
    bool uiToggle(int x, int y, int bw, int bh, const std::string& caption, bool on);
    bool uiButtonEx(int x, int y, int bw, int bh, const std::string& caption, bool enabled,
                    bool on);
    void uiLabel(int x, int y, int bw, int bh, const std::string& caption, Color c);
    /// Число со ступенями: подпись, −big, −1, значение, +1, +big. Ширина
    /// 358 точек, высота 30. true — значение изменилось.
    bool uiStepper(int x, int y, const std::string& label, int& v, int lo, int hi, int big,
                   const std::string& unit = "");
    void uiLabelLeft(int x, int y, int bh, const std::string& caption, Color c);
    void textCentered(int x0, int y0, int x1, int y1, const std::string& s, Color c);
    void textOut(int x, int y, const std::string& s, Color c);
    std::string fitText(const std::string& s, int maxW) const;
    int textWidth(const std::string& s) const { return font_.textWidth(s); }
    int textHeight() const { return font_.textHeight(); }
    std::vector<std::string> layoutText(const std::vector<std::string>& paragraphs,
                                        int maxWidth) const;

    // --- холст ---
    /// Холст в окне: от (X0, Y0) — слева и сверху могут стоять линейки.
    [[nodiscard]] gfx::Rect canvasRect() const { return gfx::Rect{m.X0, m.Y0, m.W, m.H}; }
    [[nodiscard]] bool overCanvas(int x, int y) const {
        return x >= m.X0 && x < m.W && y >= m.Y0 && y < m.H;
    }
    void drawTilePixel(int x, int y);
    void drawPreviewPixel(int x, int y);
    void ensureTileBmp();
    void drawTileCells();
    void drawMapCells();
    void drawAnts(int x0, int y0, int x1, int y1);
    void drawCanvas();

    // --- панель ---
    void ensureHSVBitmap();
    std::string toolLabel(int i) const;
    void drawToolsSection();
    void drawPaletteSection();
    void drawColorSection();
    void drawTerrainSection();
    void drawObjectKindSection();
    void buildOverview();
    void drawOverviewSection();
    void drawGridThumb(const Grid<Color>& g, int x0, int y0, int size);
    void drawInfoSection();
    void drawMenuButtonSection();
    void panelDivider(int y);
    void drawPanel();
    void repaint();
    void repaintCanvasOnly();
    void repaintCanvasAndOverview();
    void repaintColorOnly();

    // --- экраны ---
    void showTextScreen(const std::string& title, const std::vector<std::string>& paragraphs);
    void showMessage(const std::string& title, const std::string& text);
    int ask3(const std::string& title, const std::string& text, const std::string& yes,
             const std::string& no, const std::string& cancel);
    bool askYesNo(const std::string& title, const std::string& text, const std::string& yes,
                  const std::string& no);
    bool askNewSize(const std::string& title, int& nw, int& nh, int lo, int hi);
    void showColorScreen();
    bool askText(const std::string& title, const std::string& prompt, const std::string& initial,
                 std::string& value);
    void showColorOpsScreen();
    void showMapReport();
    void showMapPropertiesScreen();
    void showHelpScreen();
    void showSettingsScreen();
    void showMainMenu();
    bool showTilesetScreen();

    // --- файлы ---
    std::string askOpenFile(const std::string& title);
    std::string askSaveFile(const std::string& title, const std::string& suggested);
    std::string askOpenMap(const std::string& title);
    std::string askSaveMap(const std::string& title, const std::string& suggested);
    std::string mapsDir() const;
    bool readImageGrid(const std::string& path, Grid<Color>& g, bool quiet);
    bool loadTileFrom(const std::string& path, bool quiet, bool remember);
    bool saveProjectTo(const std::string& path);
    bool saveTileTo(const std::string& path);
    bool saveGridTo(const Grid<Color>& g, const std::string& path);
    bool loadPaletteFrom(const std::string& path, bool quiet);
    void paletteIO();
    bool loadMapFrom(const std::string& path, bool quiet, bool remember);
    bool saveMapTo(const std::string& path);
    void editObject(int idx);

    // --- команды ---
    bool commandSaveAs();
    bool commandSave();
    bool confirmDiscard(const std::string& what);
    void commandNew();
    void commandOpen();
    void commandOpenLast();
    void commandSaveSelection();
    void quitApp();
    void commandExit();

    // --- наборы тайлов ---
    std::string tilesDir() const;
    std::string iconsDir() const;
    std::string tsetDirOf(int k) const;
    std::string tsetPathOf(int k, const std::string& base) const;
    void ensureIcons();
    void tsetFree();
    void tsetLoadOne(int i);
    void tsetBuild();
    int tsetPages() const;
    static int tsetCellX(int k);
    static int tsetCellY(int k);
    int tsetAt(int k) const;
    bool tsetHit(int px, int py, int& slot) const;

    // --- инструменты холста ---
    void doPaintStroke(int startX, int startY);
    void emitShape(int ax, int ay, int bx, int by);
    void doShapeTool(int ax, int ay);
    void defineSelection(int ax, int ay);
    void dragSelection(int mx, int my);
    void doSelectTool(int mx, int my);
    void dragObject(int idx);
    void doObjectTool(int mx, int my);
    void doMiddleDrag();
    bool openTileOfMapCell(int x, int y);
    void handleMapCanvas(int mx, int my);
    void handleCanvasClick();

    // --- панель инструментов ---
    void dragHSVSquare();
    void dragHueBar();
    void dragAlphaBar();
    void handlePaletteClick();
    void doOverviewDrag();
    void handlePanelClick();
    void handleEditorKey(int key);

    // --- этап 1 RoadToProLevel (pro.cpp) ---
    /// Три ячейки третьего ряда инструментов тайла: режим кисти,
    /// «пиксель-перфект», параметры. Номер 0..2 или -1.
    static int chipHit(int px, int py);
    void drawToolChips();
    void handleChipClick(int chip);
    void selectToolPro(int mx, int my);
    void defineSelectionPro(int ax, int ay, int op);
    void definePolygonSelection(int ax, int ay, int op);
    void moveSelectionPro(int mx, int my, bool copy);
    void drawSelectionMask();
    bool handleProKey(int key);
    void showToolOptions();
    void showImageScreen();
    void showRotateDialog();
    void showOutlineDialog();
    /// Два числа со ступенями ±1 и ±8; anchor — сетка якоря 3x3 (номер
    /// ay * 3 + ax) или nullptr.
    bool askTwoNumbers(const std::string& title, const std::string& hint, const std::string& label1,
                       int& v1, int lo1, int hi1, const std::string& label2, int& v2, int lo2,
                       int hi2, const std::string& okCaption, int* anchor);
    struct NumField {
        std::string label;
        int* v;
        int lo, hi;
        /// Шаг кнопок: ±step и ±8·step (длительности кадров — десятками мс).
        int step = 1;
    };
    bool askNumbers(const std::string& title, const std::string& hint,
                    const std::vector<NumField>& fields, const std::string& okCaption, int* anchor);

    // --- этап 2: палитра (palette_ui.cpp) ---
    void toggleIndexed();
    void showPaletteScreen();
    /// Палитра из файла: PNG (точка — ячейка), GPL, HEX, PAL. merge —
    /// добавить к текущей, а не заменить.
    bool loadPaletteFile(const std::string& path, bool quiet, bool merge);
    bool savePaletteFile(const std::string& path);
    int palSelA_ = 0;
    int palSelB_ = 0;
    /// Последняя разобранная клавиша с модификаторами в миг нажатия.
    KeyPress lastKey_;
    bool outlineEight_ = false;
    bool outlineOutside_ = true;
    int outlineThick_ = 1;
    int rotateDeg_ = 15;
    int rotateMethod_ = 1;
    bool rotateExpand_ = true;

    // --- этап 8: вне пиксель-арта (filters_ui.cpp) ---
    /// Параметры экрана «Фильтры и эффекты»: помнятся до конца сеанса,
    /// чтобы фильтр можно было повторить на другом кадре.
    struct FxParams {
        int kind = 0;
        bool wrap = false;
        int blurRadius = 2;
        int sharpAmount = 100;
        int sharpRadius = 2;
        int noiseAmount = 16;
        bool noiseMono = false;
        int noiseSeed = 1;
        int brightness = 0;
        int contrast = 0;
        int saturation = 0;
        int inLo = 0;
        int inHi = 255;
        int gammaPct = 100;
        int outLo = 0;
        int outHi = 255;
        /// Кривые: общая и по каналам, точки (вход, выход).
        std::array<std::vector<std::pair<int, int>>, 4> curves;
        int curveChannel = 0;
        int skewX = 15;
        int skewY = 0;
        /// Углы перспективы в долях рамки (0..1), по часовой с левого
        /// верхнего: так они не зависят от размера картинки.
        std::array<double, 8> quad{0, 0, 1, 0, 1, 1, 0, 1};
        bool operator==(const FxParams&) const = default;
    };
    FxParams fx_;
    void showFiltersScreen();
    /// Фильтр с параметрами экрана; boxed — он видит рамку выделения.
    Grid<Color> runFilter(const Grid<Color>& g) const;
    bool filterBoxed() const;
    void doAirbrushStroke(int startX, int startY);

    // --- этап 9: инструменты пиксель-арта (art_ui.cpp) ---
    /// Карандаш и его виды: чистые точки, аэрограф, кисть из выделения,
    /// затенение. Правая кнопка по карандашу.
    void showBrushOptions();
    /// Заливка цветом или градиентом узором. Правая кнопка по заливке.
    void showFillOptions();
    void doGradientTool(int ax, int ay);
    bool brushGhostShown();
    /// Призрак был на холсте в прошлом кадре: уйдя с холста, его надо стереть.
    bool ghostShown_ = false;
    void drawBrushGhost();
    void drawChecker(int x0, int y0, int x1, int y1);

    // --- этап 7: связь с игрой (gameset_ui.cpp) ---
    /// Картинки тайлов набора игры по имени файла без расширения; пустая —
    /// файла нет. Сбрасывается, когда в набор что-то записали.
    std::map<std::string, Grid<Color>> gameTiles_;
    bool gameTilesDirty_ = true;
    /// Какое по счёту место местности на карте показывает окружение тайла.
    int mapCtxIndex_ = 0;
    const Grid<Color>* gameTile(const std::string& name);
    /// Картинка клетки карты: вариант стыка, если он нарисован, иначе тайл.
    const Grid<Color>* mapCellImage(int x, int y);
    /// Местность, которую рисует открытый тайл (слот 0..6), или -1.
    int editedTerrain() const;
    /// Клетка карты, вокруг которой показывать тайл; false — такой нет.
    bool mapContextCell(int& cx, int& cy, int& count);
    void drawMapContext(int ox, int oy, int sw, int sh);
    void drawMapContextLabel();
    void toggleMapContext();
    /// true — открыт новый документ (заготовка стыков): экран наборов
    /// тоже закрывается.
    bool showGameSetScreen();
    void exportGameSheet();
    void exportGameAtlas();
    void sliceGameSheetAsk();
    bool autotileTemplate(int terrain);
    void autotileSlice();

    // --- этап 6: векторные инструменты и тайлы (vector_ui.cpp) ---
    /// Последняя тронутая фигура векторного слоя: её удаляет Delete.
    int vecSel_ = -1;
    static std::string lineKindName(int k);
    VecShape styledShape(int type) const;
    void previewShape(const VecShape& s);
    void drawVectorHandles();
    void doVectorTool(int mx, int my);
    void showShapeOptions();
    void showTilePickerScreen();
    void importGameTiles();

    // --- этап 5: линейки и направляющие (guides.cpp) ---
    void drawGuides();
    void drawRadialAxes();
    void drawRulers();
    void handleRulerPress();
    bool handleGuideKey(int key);
    void showGridScreen();

    // --- этап 4: лента слоёв и кадров (timeline.cpp) ---
    /// Лента под холстом. Видна только в режиме тайла; включается J или
    /// сама, когда открыт документ со слоями или кадрами.
    bool timelineOn_ = false;
    /// Первый видимый кадр и первая видимая строка слоёв (сверху — верхний).
    int tlFirstFrame_ = 0;
    int tlFirstRow_ = 0;
    /// Активные кадр и слой, которые лента уже показала: прокрутка к ним
    /// только при смене, чтобы ролик мог отлистать ленту.
    int tlSeenFrame_ = -1;
    int tlSeenLayer_ = -1;
    bool playing_ = false;
    /// Метка, которую проигрывают, или -1 — все кадры.
    int playTag_ = -1;
    int playPing_ = 0;
    int playNextMs_ = 0;
    /// Двойной щелчок по имени слоя — переименование.
    int tlLastClickMs_ = -100000;
    int tlLastClickLayer_ = -1;
    [[nodiscard]] bool timelineShown() const { return timelineOn_ && m.docMode == DOC_TILE; }
    /// Высота холста по ленте: m.H — холст, окно — WIN_H.
    void applyLayout();
    void drawTimeline();
    /// Слои строк ленты сверху вниз, без детей свёрнутых групп.
    std::vector<int> timelineRows() const;
    void drawLayerKindMark(int x, int cy, const Layer& layer);
    void handleTimelineClick();
    bool handleTimelineKey(int key);
    void timelineWheel(int wheel);
    void togglePlayback();
    void stopPlayback();
    void stepPlayback();
    void renameLayerAsk(int l);
    void editFrameDuration(int f);
    /// Метка: i < 0 — новая на активном кадре.
    void editTag(int i);
    void showAnimationScreen();
    // --- экспорт и импорт анимации ---
    /// 1 — открыта анимация, 0 — файл не анимация, -1 — ошибка (сказано).
    int tryLoadAnimation(const std::string& path, bool quiet);
    /// Кадры from..to в файл; формат — по расширению. asDocument — файл
    /// становится документом, если в нём ничего не потеряно.
    bool saveAnimationTo(const std::string& path, int from, int to, int scale, int plays,
                         bool asDocument);
    /// Форматы картинок по расширению (этап 5): PNG и APNG, GIF, BMP, TIFF.
    enum { FMT_PNG, FMT_GIF, FMT_BMP, FMT_TIFF };
    static int imageFormatOf(const std::string& path);
    bool writeImageFile(const std::string& path, const std::vector<AnimFrame>& frames, int plays);
    /// Диалог сохранения выгрузки с заданным расширением.
    std::string askExportFile(const std::string& title, const std::string& suggested,
                              const std::string& ext, const std::string& filterName);
    void exportSheetAsk();
    void exportAnimationAsk(bool gif);
    void exportFramesAsk();
    /// Первый кадр сведённым PNG — в слот набора тайлов из метаданных.
    void exportToTilesetSlot();
    /// Настройки листа спрайтов живут до конца работы: лист обычно
    /// выгружают раз за разом с теми же полями.
    int sheetCols_ = 0;
    int sheetMargin_ = 0;
    int sheetSpacing_ = 0;
    int sheetPadding_ = 0;
    int sheetScale_ = 1;
    int sheetBg_ = 0;
    /// Кадры для выгрузки: метка проигрывания или все.
    void exportRange(int& from, int& to) const;

    // --- запуск ---
    void initColorBars();
    void syncTitle();
    void finishFrame();
};

}  // namespace tjtm
