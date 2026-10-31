// Векторные инструменты и выбор тайлов (этап 6 RoadToProLevel).
//
// Линия, прямоугольник и овал на векторном слое оставляют правимые
// фигуры; на растровом — ложатся точками сразу. Вид линии (ломаная,
// многоугольник, кривая через точки, звезда) — в параметрах фигур:
// правая кнопка по линии, прямоугольнику или овалу. На векторном слое
// протяжка за вершину двигает её, с Ctrl протяжка за фигуру двигает всю
// фигуру, Delete удаляет последнюю тронутую.

#include <algorithm>
#include <cmath>

#include "app/editor.h"
#include "core/fsutil.h"
#include "core/imageops.h"
#include "core/pmath.h"
#include "core/png.h"
#include "core/theme.h"
#include "core/tilemap.h"
#include "core/vector.h"

namespace tjtm {

using namespace theme;

namespace {

/// Центр точки (x, y) документа — в координатах фигур.
VecPoint center(int x, int y) {
    return {x + 0.5, y + 0.5};
}

}  // namespace

std::string Editor::lineKindName(int k) {
    switch (k) {
        case LINE_POLY: return "Ломаная";
        case LINE_POLYGON: return "Многоугольник";
        case LINE_CURVE: return "Кривая";
        case LINE_CLOSED_CURVE: return "Замкнутая кривая";
        case LINE_STAR: return "Звезда";
        default: return "Прямая";
    }
}

VecShape Editor::styledShape(int type) const {
    VecShape s;
    s.type = type;
    s.stroke = m.firstColor;
    s.fill = m.shapeFill ? m.secondColor : kClear;
    s.width = m.shapeWidth;
    s.starPoints = m.starPoints;
    s.innerPct = m.starInner;
    s.smooth = m.antialias;
    return s;
}

/// Фигура в работе — точками цвета выделения поверх холста.
void Editor::previewShape(const VecShape& s) {
    Grid<Color> g(m.docW(), m.docH(), kClear);
    VecShape p = s;
    p.stroke = kWhite;
    p.fill = s.fill.a != 0 ? kWhite : kClear;
    vec::rasterize(p, g);
    for (int y = 0; y < g.h; ++y) {
        for (int x = 0; x < g.w; ++x) {
            if (g.at(x, y).a != 0) drawPreviewPixel(x, y);
        }
    }
}

void Editor::drawVectorHandles() {
    if (m.docMode != DOC_TILE || m.activeKind() != LK_VECTOR) return;
    if (m.toolTile != T_LINE && m.toolTile != T_RECT && m.toolTile != T_ELLIPSE) return;
    // Вершины видны там, где их можно взять: квадратик белый с тёмной
    // обводкой читается на любом рисунке, у последней тронутой — цвета
    // выделения.
    fb_.setClip(canvasRect());
    const int ps = m.pixelSize;
    for (std::size_t i = 0; i < m.tileShapes.size(); ++i) {
        const bool sel = static_cast<int>(i) == vecSel_;
        for (const VecPoint& p : m.tileShapes[i].pts) {
            const int sx = m.tileToScreenX(0) + pround(p.x * ps);
            const int sy = m.tileToScreenY(0) + pround(p.y * ps);
            fb_.fillRect(sx - 3, sy - 3, sx + 4, sy + 4, sel ? cAccent : active.handleDark);
            fb_.fillRect(sx - 2, sy - 2, sx + 3, sy + 3, sel ? cHover : active.handleLight);
        }
    }
    fb_.resetClip();
}

void Editor::doVectorTool(int mx, int my) {
    const bool vectorLayer = m.activeKind() == LK_VECTOR;
    const int ps = std::max(1, m.pixelSize);
    auto docX = [&](int sx) { return static_cast<double>(sx - m.tileToScreenX(0)) / ps; };
    auto docY = [&](int sy) { return static_cast<double>(sy - m.tileToScreenY(0)) / ps; };
    const double r = std::max(0.6, 6.0 / ps);

    // --- правка готовых фигур векторного слоя ---
    if (vectorLayer) {
        int si = -1;
        int vi = -1;
        const double px = docX(frameClickX_);
        const double py = docY(frameClickY_);
        if (vec::hitVertex(m.tileShapes, px, py, r, si, vi)) {
            m.pushUndo();
            vecSel_ = si;
            VecShape s = m.tileShapes[static_cast<std::size_t>(si)];
            while (mousePressed() && !quitNow_) {
                // Вершина встаёт в центр точки: фигура остаётся пиксельной.
                const VecPoint np = center(m.screenToTileX(mouseX()), m.screenToTileY(mouseY()));
                if (!(np == s.pts[static_cast<std::size_t>(vi)])) {
                    s.pts[static_cast<std::size_t>(vi)] = np;
                    m.setVectorShape(si, s);
                    repaintCanvasOnly();
                }
                idle();
            }
            m.needRepaint = true;
            return;
        }
        if (isKeyPressed(KEY_CTRL)) {
            si = vec::hitShape(m.tileShapes, px, py, r);
            if (si >= 0) {
                m.pushUndo();
                vecSel_ = si;
                const VecShape base = m.tileShapes[static_cast<std::size_t>(si)];
                int lastDx = 0;
                int lastDy = 0;
                while (mousePressed() && !quitNow_) {
                    const int dx = m.screenToTileX(mouseX()) - mx;
                    const int dy = m.screenToTileY(mouseY()) - my;
                    if (dx != lastDx || dy != lastDy) {
                        VecShape s = base;
                        vec::translate(s, dx, dy);
                        m.setVectorShape(si, s);
                        lastDx = dx;
                        lastDy = dy;
                        repaintCanvasOnly();
                    }
                    idle();
                }
                m.needRepaint = true;
                return;
            }
        }
    }

    // --- новая фигура ---
    int type = VS_POLYLINE;
    bool twoPoint = true;
    if (m.toolTile == T_RECT) {
        type = VS_RECT;
    } else if (m.toolTile == T_ELLIPSE) {
        type = VS_ELLIPSE;
    } else {
        switch (m.lineKind) {
            case LINE_POLY:
                type = VS_POLYLINE;
                twoPoint = false;
                break;
            case LINE_POLYGON:
                type = VS_POLYGON;
                twoPoint = false;
                break;
            case LINE_CURVE:
                type = VS_CURVE;
                twoPoint = false;
                break;
            case LINE_CLOSED_CURVE:
                type = VS_CLOSED_CURVE;
                twoPoint = false;
                break;
            case LINE_STAR: type = VS_STAR; break;
            default: type = VS_POLYLINE; break;
        }
    }
    VecShape s = styledShape(type);
    if (twoPoint) {
        // Протяжка: от угла к углу, у звезды — от центра к вершине луча.
        s.pts = {center(mx, my), center(mx, my)};
        int bx = mx;
        int by = my;
        while (mousePressed() && !quitNow_) {
            int nx = m.screenToTileX(mouseX());
            int ny = m.screenToTileY(mouseY());
            if (isKeyPressed(KEY_SHIFT) && type != VS_STAR) Model::squarifyTo(mx, my, nx, ny);
            if (nx != bx || ny != by) {
                bx = nx;
                by = ny;
                s.pts[1] = center(nx, ny);
                drawCanvas();
                previewShape(s);
                redraw();
            }
            idle();
        }
        if (s.pts[0] == s.pts[1] && type != VS_POLYLINE) {
            m.needRepaint = true;
            return;
        }
    } else {
        // По точкам: щелчок — точка, двойной щелчок или Enter — конец,
        // у замкнутых — и щелчок по первой. Backspace снимает последнюю,
        // Escape и правая кнопка отменяют.
        s.pts = {center(mx, my)};
        waitMouseRelease();
        dropInput();
        pf_.input().keys.clear();
        const bool closedType = type == VS_POLYGON || type == VS_CLOSED_CURVE;
        bool finished = false;
        bool cancelled = false;
        int lastMs = pf_.milliseconds();
        while (!finished && !cancelled && !quitNow_) {
            beginFrameInput();
            const int cx = std::clamp(m.screenToTileX(mouseX()), 0, m.docW() - 1);
            const int cy = std::clamp(m.screenToTileY(mouseY()), 0, m.docH() - 1);
            VecShape show = s;
            show.pts.push_back(center(cx, cy));
            drawCanvas();
            previewShape(show);
            fb_.setClip(canvasRect());
            for (const VecPoint& p : s.pts) {
                const int sx = m.tileToScreenX(0) + pround(p.x * ps);
                const int sy = m.tileToScreenY(0) + pround(p.y * ps);
                fb_.frameRect(sx - 3, sy - 3, sx + 4, sy + 4, cAccent);
            }
            fb_.resetClip();
            redraw();
            if (frameClick_) {
                frameClick_ = false;
                const int px = std::clamp(m.screenToTileX(frameClickX_), 0, m.docW() - 1);
                const int py = std::clamp(m.screenToTileY(frameClickY_), 0, m.docH() - 1);
                const VecPoint p = center(px, py);
                const int now = pf_.milliseconds();
                if (frameClickButton_ == MB_RIGHT) {
                    cancelled = true;
                } else if (closedType && s.pts.size() >= 3 &&
                           std::hypot(p.x - s.pts.front().x, p.y - s.pts.front().y) <= 1.5) {
                    finished = true;
                } else if (p == s.pts.back() && now - lastMs <= DOUBLE_CLICK_MS) {
                    finished = s.pts.size() >= 2;
                } else {
                    s.pts.push_back(p);
                }
                lastMs = now;
            }
            switch (takeKey()) {
                case KEY_ENTER: finished = s.pts.size() >= 2; break;
                case KEY_ESC: cancelled = true; break;
                case KEY_BACK:
                    if (s.pts.size() > 1) s.pts.pop_back();
                    break;
                default: break;
            }
            idle();
        }
        waitMouseRelease();
        if (cancelled || !finished) {
            m.needRepaint = true;
            return;
        }
    }
    const int i = m.addVectorShape(s);
    if (i >= 0) vecSel_ = i;
    m.needRepaint = true;
}

// ---------------------------------------------------------------------------
// Параметры фигур
// ---------------------------------------------------------------------------

void Editor::showShapeOptions() {
    bool done = false;
    dropInput();
    pf_.input().keys.clear();
    constexpr int LX = 40;
    while (!done && !quitNow_) {
        beginFrameInput();
        fb_.clear(cWindow);
        textCentered(0, 12, WIN_W, 40, "Параметры фигур", cText);
        uiLabelLeft(LX, 46, 24, "Вид линии (инструмент «Линия»)", cTextDim);
        for (int k = 0; k < LINE_KINDS; ++k) {
            if (uiToggle(LX + (k % 3) * 236, 72 + (k / 3) * 38, 230, 30, lineKindName(k),
                         m.lineKind == k))
                m.lineKind = k;
        }
        uiLabelLeft(LX, 156, 24,
                    "Ломаную, многоугольник и кривую ставят по точкам: щелчок — точка, "
                    "двойной щелчок или Enter — конец",
                    cTextDim);
        uiStepper(LX, 196, "Толщина", m.shapeWidth, 1, 16, 4);
        if (uiToggle(LX + 400, 196, 300, 30, "Заливка вторым цветом", m.shapeFill))
            m.shapeFill = !m.shapeFill;
        uiStepper(LX, 240, "Лучей звезды", m.starPoints, 3, 32, 4);
        uiStepper(LX + 380, 240, "Впадины, %", m.starInner, 5, 100, 10, "%");
        uiLabelLeft(LX, 290, 24, "На векторном слое фигуры остаются правимыми:", cTextDim);
        uiLabelLeft(LX, 312, 24, "протяжка за вершину двигает её, с Ctrl — всю фигуру,", cTextDim);
        uiLabelLeft(LX, 334, 24, "Delete удаляет последнюю тронутую. На растровом слое", cTextDim);
        uiLabelLeft(LX, 356, 24, "фигура сразу ложится точками.", cTextDim);
        if (uiToggle(LX, 396, 460, 30, "Сглаживание края — не пиксель-арт", m.antialias))
            m.antialias = !m.antialias;
        uiLabelLeft(LX, 430, 24,
                    "Край — долями покрытия: новые цвета и полупрозрачность. В строгом "
                    "индексном режиме итог сводится к палитре.",
                    cWarn);
        if (uiButton(WIN_W / 2 - 90, WIN_H - 52, 180, 36, "Закрыть", true)) done = true;
        redraw();
        const int key = takeKey();
        if (key == KEY_ESC || key == KEY_ENTER) done = true;
        idle();
    }
    waitMouseRelease();
    m.needRepaint = true;
}

// ---------------------------------------------------------------------------
// Тайлы слоя-тайлкарты
// ---------------------------------------------------------------------------

void Editor::showTilePickerScreen() {
    if (m.activeTileset() == nullptr) {
        showMessage("Не тайлкарта",
                    "Тайлы выбирают для слоя-тайлкарты. Создайте его на экране слоёв и кадров: "
                    "«Новая тайлкарта» или «Слой → тайлкарта».");
        return;
    }
    bool done = false;
    int page = 0;
    dropInput();
    pf_.input().keys.clear();
    constexpr int CELL = 52;
    constexpr int COLS = 13;
    constexpr int ROWS = 6;
    constexpr int GX = 52;
    constexpr int GY = 70;
    while (!done && !quitNow_) {
        beginFrameInput();
        const Tileset& ts = *m.activeTileset();
        const int n = static_cast<int>(ts.tiles.size());
        const int pages = std::max(1, (n + COLS * ROWS - 1) / (COLS * ROWS));
        page = std::clamp(page, 0, pages - 1);
        fb_.clear(cWindow);
        textCentered(0, 12, WIN_W, 40, "Тайлы слоя «" + ts.name + "»", cText);
        uiLabel(0, 40, WIN_W, 22,
                std::to_string(n - 1) + " тайлов " + std::to_string(ts.tw) + "×" +
                    std::to_string(ts.th) + "   ·   щелчок выбирает, правая кнопка удаляет",
                cTextDim);
        for (int k = 0; k < COLS * ROWS; ++k) {
            const int i = page * COLS * ROWS + k;
            if (i >= n) break;
            const int x = GX + (k % COLS) * CELL;
            const int y = GY + (k / COLS) * CELL;
            const bool sel = i == m.tileBrush;
            fb_.fillRect(x, y, x + CELL - 4, y + CELL - 4, sel ? cAccent : cBorder);
            fb_.fillRect(x + 2, y + 2, x + CELL - 6, y + CELL - 6, cCheckA);
            if (i == 0) {
                uiLabel(x, y, CELL - 4, CELL - 4, "пусто", cTextDim);
            } else {
                drawGridThumb(ts.tiles[static_cast<std::size_t>(i)], x + 2, y + 2, CELL - 8);
            }
            if (frameClick_ && frameClickX_ >= x && frameClickX_ < x + CELL - 4 &&
                frameClickY_ >= y && frameClickY_ < y + CELL - 4) {
                frameClick_ = false;
                if (frameClickButton_ == MB_RIGHT && i > 0) {
                    if (askYesNo("Удалить тайл",
                                 "Клетки с этим тайлом станут пустыми во всех "
                                 "кадрах. Удалить тайл " +
                                     std::to_string(i) + "?",
                                 "Удалить", "Отмена"))
                        m.deleteTileFromActiveSet(i);
                } else {
                    m.tileBrush = i;
                    done = true;
                }
            }
        }
        const int by = GY + ROWS * CELL + 12;
        if (uiButton(GX, by, 120, 30, "◄", page > 0)) --page;
        uiLabel(GX + 126, by, 100, 30, std::to_string(page + 1) + " / " + std::to_string(pages),
                cText);
        if (uiButton(GX + 232, by, 120, 30, "►", page < pages - 1)) ++page;
        if (uiToggle(GX + 380, by, 290, 30,
                     m.tilemapPixels ? "Правка точками — тайл во всех копиях"
                                     : "Правка тайлами — кисть кладёт тайл",
                     m.tilemapPixels))
            m.tilemapPixels = !m.tilemapPixels;
        const int b2 = by + 40;
        if (uiButton(GX, b2, 210, 32, "Новый пустой тайл", true)) {
            m.addTileToActiveSet(Grid<Color>(ts.tw, ts.th, kClear));
            m.tilemapPixels = true;
        }
        if (uiButton(GX + 216, b2, 210, 32, "Тайл из выделения", m.hasSelection)) {
            m.addTileToActiveSet(tmap::cellImage(img::crop(m.compositeFrame(m.activeFrame), m.selX,
                                                           m.selY, ts.tw, ts.th, kClear),
                                                 0, 0, ts.tw, ts.th));
        }
        if (uiButton(GX + 432, b2, 240, 32, "Тайлы из набора игры", true)) importGameTiles();
        if (uiButton(WIN_W / 2 - 90, WIN_H - 52, 180, 36, "Закрыть", true)) done = true;
        redraw();
        const int key = takeKey();
        if (key == KEY_ESC || key == KEY_ENTER) done = true;
        idle();
    }
    waitMouseRelease();
    m.needRepaint = true;
}

void Editor::importGameTiles() {
    const Tileset* cur = m.activeTileset();
    if (cur == nullptr) return;
    const std::string dir = tilesDir();
    if (dir.empty() || !dirExists(dir)) {
        showMessage("Папка набора не задана", "Укажите её на экране «Наборы тайлов» (T).");
        return;
    }
    // Тайлы игры встают в набор слоя в размер его клетки ближайшим
    // соседом: тайлкарта из настоящих тайлов — как карта в игре.
    std::vector<Grid<Color>> tiles;
    for (const std::string& f : listFiles(dir)) {
        if (pathExtLower(f) != ".png") continue;
        Grid<Color> g;
        std::string err;
        if (!pngLoad(pathCombine(dir, f), g, err)) continue;
        tiles.push_back(img::scaleNearest(g, cur->tw, cur->th));
    }
    const int added = m.addTilesToActiveSet(tiles);
    showMessage("Тайлы из набора игры", "Добавлено тайлов: " + std::to_string(added) + ".");
}

}  // namespace tjtm
