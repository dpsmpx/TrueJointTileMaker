// Редактор целиком: окно, ввод, отрисовка и экраны — перенос TJTM.pas.
//
// Состояние документов и логика без окна живут в Model (core/model.h);
// здесь всё, что рисует и ждёт ввода. Устройство повторяет оригинал:
// главный цикл (appStep), экраны — вложенные циклы, которые сами разбирают
// ввод и сами рисуются, инструменты — циклы, пока нажата кнопка мыши.
// Имена процедур те же, что в TJTM.pas, — по ним перенос сличается с
// исходником.
#pragma once

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
    void uiLabelLeft(int x, int y, int bh, const std::string& caption, Color c);
    void textCentered(int x0, int y0, int x1, int y1, const std::string& s, Color c);
    void textOut(int x, int y, const std::string& s, Color c);
    std::string fitText(const std::string& s, int maxW) const;
    int textWidth(const std::string& s) const { return font_.textWidth(s); }
    int textHeight() const { return font_.textHeight(); }
    std::vector<std::string> layoutText(const std::vector<std::string>& paragraphs,
                                        int maxWidth) const;

    // --- холст ---
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
    void moveSelectionPro(int mx, int my, bool copy);
    void drawSelectionMask();
    bool handleProKey(int key);
    void showToolOptions();
    void showImageScreen();
    /// Два числа со ступенями ±1 и ±8; anchor — сетка якоря 3x3 (номер
    /// ay * 3 + ax) или nullptr.
    bool askTwoNumbers(const std::string& title, const std::string& hint, const std::string& label1,
                       int& v1, int lo1, int hi1, const std::string& label2, int& v2, int lo2,
                       int hi2, const std::string& okCaption, int* anchor);
    struct NumField {
        std::string label;
        int* v;
        int lo, hi;
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

    // --- запуск ---
    void initColorBars();
    void syncTitle();
    void finishFrame();
};

}  // namespace tjtm
