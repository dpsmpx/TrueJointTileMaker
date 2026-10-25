// Запуск и главный цикл (TJTM.pas: «Запуск»).
//
//   tjtm [файл.png | карта.map] [--base КАТАЛОГ] [--frames N] [--screenshot кадр.png]
//        [--script СЦЕНАРИЙ [--shots КАТАЛОГ]]
//
// --base     где искать Config.txt, Resources/ и Картинки/ вместо каталога
//            программы;
// --frames   пройти N шагов главного цикла и закрыться — для тестов без
//            экрана (SDL_VIDEODRIVER=offscreen); справка и меню при запуске
//            тогда не показываются;
// --screenshot  сохранить последний кадр в PNG;
// --script   вести программу по сценарию tests/screens/*.txt (см.
//            platform/script.h) и закрыться в его конце; снимки «shot» —
//            в каталог --shots.

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <algorithm>
#include <cstdlib>
#include <string>

#include "app/editor.h"
#include "core/fsutil.h"
#include "core/png.h"

namespace tjtm {

Editor::Editor() {
    m.drawTilePixel = [this](int x, int y) { drawTilePixel(x, y); };
    m.drawPreviewPixel = [this](int x, int y) { drawPreviewPixel(x, y); };
}

void Editor::syncTitle() {
    std::string t = std::string(kAppTitle) + " " + kAppVersion + " — ";
    if (m.docMode == DOC_MAP) {
        t += m.currentMapFile.empty() ? "новая карта" : fileNameOnly(m.currentMapFile);
    } else {
        t += m.currentFile.empty() ? "новый тайл" : fileNameOnly(m.currentFile);
    }
    if (m.isModified()) t += " *";
    if (t != lastTitle_) {
        lastTitle_ = t;
        pf_.setTitle(t);
    }
}

bool Editor::appInit(int argc, char** argv) {
    std::string openPath;
    std::string base;
    std::string script;
    std::string shots;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const bool hasValue = i + 1 < argc;
        if (a == "--frames" && hasValue) {
            framesLeft_ = std::atoi(argv[++i]);
        } else if (a == "--screenshot" && hasValue) {
            screenshotPath_ = argv[++i];
        } else if (a == "--base" && hasValue) {
            base = argv[++i];
        } else if (a == "--script" && hasValue) {
            script = argv[++i];
        } else if (a == "--shots" && hasValue) {
            shots = argv[++i];
        } else if (!a.empty() && a[0] != '-') {
            openPath = a;
        }
    }

    std::string error;
    if (!pf_.init(std::string(kAppTitle) + " " + kAppVersion, WIN_W, WIN_H, error)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, kAppTitle,
                                 ("Не удалось открыть окно: " + error).c_str(), nullptr);
        return false;
    }
    baseDir_ = base.empty() ? pf_.baseDir() : base;
    if (!script.empty() && !pf_.runScript(script, shots, error)) {
        SDL_Log("сценарий %s: %s", script.c_str(), error.c_str());
        return false;
    }
    configPath_ = resolvePath("Config.txt");
    fb_.resize(WIN_W, WIN_H, theme::cWindow);

    if (!TTF_Init()) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, kAppTitle, SDL_GetError(), pf_.window());
        return false;
    }
    const std::string resources = resolvePath("Resources");
    const std::string ui = gfx::findUiFont(resources);
    const std::string mono = gfx::findMonoFont(resources);
    if (ui.empty() || !font_.open(ui, 10.0F)) {
        SDL_ShowSimpleMessageBox(
            SDL_MESSAGEBOX_ERROR, kAppTitle,
            "Не найден шрифт интерфейса: положите LiberationSans-Regular.ttf в Resources/Fonts.",
            pf_.window());
        return false;
    }
    if (mono.empty() || !mono_.open(mono, 11.0F)) mono_.open(ui, 11.0F);

    // Отсутствующий файл настроек — не ошибка: берутся значения по
    // умолчанию, и файл создаётся, чтобы его было легко найти и поправить.
    if (!configLoad(configPath_, cfg)) configSave(configPath_, cfg);
    m.gridOn = cfg.showGrid;
    m.checkerOn = cfg.showChecker;
    m.seamlessOn = cfg.showSeamless;
    m.firstColor = argb(255, 0, 0, 0);
    m.secondColor = argb(255, 255, 255, 255);
    // Этап 1 RoadToProLevel: карандаш пиксель-арта заменяет цвет, а не
    // смешивает; новый и очищенный тайл прозрачен; кисть и заливка не
    // выходят за выделение. Смешивание осталось — клавиша E.
    m.brushReplace = true;
    m.transparentBlank = true;
    m.clipToSelection = true;
    // Этап 2: индексный режим — настоящий: точка хранит номер ячейки.
    m.strictIndexed = true;
    // Этап 7: карта картинками набора подбирает варианты стыков; с этапа 10
    // карта картинками — сразу, как в игре (без папки набора — цветами).
    m.mapAutotile = true;
    m.mapTiles = true;
    m.syncHSVFromColor();
    m.setDefaultPalette();
    m.buffer.clear();
    m.mapBuffer.clear();
    m.clearHistory();
    initColorBars();
    if (!cfg.paletteFile.empty()) loadPaletteFrom(resolvePath(cfg.paletteFile), true);

    // Размер из настроек нужен, только когда открыть картинку не удалось.
    bool loaded = false;
    if (cfg.autoLoadLast && !cfg.lastFile.empty())
        loaded = loadTileFrom(resolvePath(cfg.lastFile), true, false);
    if (!loaded && !cfg.startupImage.empty())
        loaded = loadTileFrom(resolvePath(cfg.startupImage), true, false);
    if (!loaded) m.newTileOfSize(cfg.defaultWidth, cfg.defaultHeight);

    // Карта готовится сразу, чтобы переключение режима ничего не спрашивало.
    m.map.reset(cfg.defaultMapWidth, cfg.defaultMapHeight);
    m.mapModified = false;
    if (cfg.startInMapMode) {
        if (!cfg.lastMapFile.empty() && fileExists(resolvePath(cfg.lastMapFile))) {
            loadMapFrom(resolvePath(cfg.lastMapFile), true, false);
        } else {
            m.docMode = DOC_MAP;
        }
        m.fitScale();
    }

    // Файл из командной строки — как «Загрузить» из меню.
    if (!openPath.empty()) {
        if (pathExtLower(openPath) == ".map") {
            loadMapFrom(openPath, false, true);
        } else {
            loadTileFrom(openPath, false, true);
        }
    }

    ensureIcons();
    syncTitle();
    repaint();
    if (framesLeft_ < 0) {
        if (cfg.showHelpOnStart) showHelpScreen();
        if (cfg.showMenuOnStart) showMainMenu();
    }
    return true;
}

/// Один шаг главного цикла: разбор ввода и перерисовка по необходимости.
void Editor::appStep() {
    // Снимок ввода — первым делом, до перерисовки: перерисовка бывает
    // долгой, и щелчок во время неё не должен пропасть.
    beginFrameInput();

    // Значки перечитываются здесь, а не там, где их правят: правка приходит
    // из экрана наборов, а он в это время сам занимает окно.
    ensureIcons();
    applyLayout();
    stepPlayback();
    if (m.needRepaint) repaint();
    syncTitle();

    // Муравьи бегут, пока есть выделение: перерисовывается только холст.
    if (m.hasSelection && m.selW > 0 && m.selH > 0 &&
        pf_.milliseconds() - antsMs_ >= ANTS_STEP_MS) {
        antsMs_ = pf_.milliseconds();
        antsPhase_ = (antsPhase_ + 1) % 8;
        repaintCanvasOnly();
    }

    // Колесо могло провернуться на несколько щелчков между кадрами. Над
    // длинной палитрой оно её листает, а не меняет масштаб.
    int wheel = takeWheel();
    if (wheel != 0 && timelineShown() && mouseX() < m.W && mouseY() >= m.H) {
        timelineWheel(wheel);
        wheel = 0;
    }
    if (wheel != 0 && m.docMode == DOC_TILE && m.palMaxScroll() > 0 && mouseX() >= PANEL_X &&
        mouseY() >= PAL_Y0 && mouseY() < PAL_Y1) {
        m.scrollPalette(-wheel);
        wheel = 0;
    }
    for (; wheel > 0; --wheel)
        m.zoomAt(1, std::min(mouseX(), m.W - 1), mouseY());
    for (; wheel < 0; ++wheel)
        m.zoomAt(-1, std::min(mouseX(), m.W - 1), mouseY());

    const int key = takeKey();
    if (key != 0) handleEditorKey(key);

    if (frameClick_) {
        frameClick_ = false;
        if (frameClickX_ < m.W && timelineShown() && frameClickY_ >= m.H) {
            handleTimelineClick();
        } else if (frameClickX_ < m.W && (frameClickX_ < m.X0 || frameClickY_ < m.Y0)) {
            handleRulerPress();
        } else if (frameClickX_ < m.W) {
            // Рисовать по бегущим кадрам нельзя: щелчок по холсту
            // останавливает проигрывание.
            stopPlayback();
            handleCanvasClick();
        } else {
            handlePanelClick();
        }
    }

    // Координаты курсора обновляются точечно, без полной перерисовки окна.
    const int tx = m.screenToTileX(mouseX());
    const int ty = m.screenToTileY(mouseY());
    if (tx != lastStatX_ || ty != lastStatY_) {
        lastStatX_ = tx;
        lastStatY_ = ty;
        if (!m.needRepaint) {
            // Кисть из выделения (этап 9) идёт за курсором по холсту.
            if (brushGhostShown() || ghostShown_) drawCanvas();
            ghostShown_ = brushGhostShown();
            drawInfoSection();
            // Метка курсора на линейках идёт за мышью.
            drawRulers();
            redraw();
        }
    }
    finishFrame();
    idle();
}

void Editor::finishFrame() {
    if (framesLeft_ < 0) return;
    if (framesLeft_ > 0) --framesLeft_;
    if (framesLeft_ == 0) {
        if (m.needRepaint) repaint();
        if (!screenshotPath_.empty()) {
            std::string err;
            pngSave(screenshotPath_, fb_.toGrid(), err);
        }
        quitNow_ = true;
    }
}

}  // namespace tjtm

int main(int argc, char** argv) {
    int rc = 0;
    {
        // Редактор закрывает свои шрифты раньше, чем библиотека шрифтов
        // выключится: отсюда отдельная область видимости.
        tjtm::Editor editor;
        if (editor.appInit(argc, argv)) {
            while (editor.running())
                editor.appStep();
        } else {
            rc = 1;
        }
    }
    TTF_Quit();
    return rc;
}
