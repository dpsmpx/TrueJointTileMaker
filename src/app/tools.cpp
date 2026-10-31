// Инструменты холста, панель и клавиатура (TJTM.pas: «Инструменты
// холста», «Панель инструментов», «Клавиатура»). Инструмент — цикл, пока
// кнопка мыши нажата: так в оригинале, и так проще сличать.

#include <algorithm>

#include "app/editor.h"
#include "core/fsutil.h"
#include "core/pmath.h"
#include "core/text.h"
#include "core/theme.h"

namespace tjtm {

using namespace theme;

/// Мазок кистью. Промежуточные точки достраиваются отрезком: при быстром
/// движении мыши в линии не остаётся пропусков.
void Editor::doPaintStroke(int startX, int startY) {
    m.pushUndo();
    m.beginStroke();
    m.liveDraw = m.docMode == DOC_TILE;
    int lastX = startX;
    int lastY = startY;
    m.emitPixel(startX, startY);
    if (m.docMode == DOC_MAP) {
        repaintCanvasOnly();
    } else {
        redraw();
    }
    while (mousePressed() && !quitNow_) {
        const int mx = m.screenToTileX(mouseX());
        const int my = m.screenToTileY(mouseY());
        if (mx != lastX || my != lastY) {
            m.shapeLine(lastX, lastY, mx, my);
            lastX = mx;
            lastY = my;
            if (m.docMode == DOC_MAP) {
                repaintCanvasOnly();
            } else {
                redraw();
            }
        }
        idle();
    }
    m.liveDraw = false;
    m.endStroke();
    m.needRepaint = true;
}

/// Фигура текущего инструмента: один вызов служит и предпросмотром, и
/// применением, разницу задаёт shapePreview.
void Editor::emitShape(int ax, int ay, int bx, int by) {
    const bool filled = isKeyPressed(KEY_CTRL);
    if (m.docMode == DOC_MAP) {
        m.shapeRect(ax, ay, bx, by, filled);
        return;
    }
    switch (m.toolTile) {
        case T_LINE: m.shapeLine(ax, ay, bx, by); break;
        case T_RECT: m.shapeRect(ax, ay, bx, by, filled); break;
        case T_ELLIPSE: m.shapeEllipse(ax, ay, bx, by, filled); break;
        default: break;
    }
}

/// Линия, прямоугольник или овал: протяжка с предпросмотром, запись в
/// документ по отпусканию кнопки.
void Editor::doShapeTool(int ax, int ay) {
    int bx = ax;
    int by = ay;
    while (mousePressed() && !quitNow_) {
        int nx = m.screenToTileX(mouseX());
        int ny = m.screenToTileY(mouseY());
        if (isKeyPressed(KEY_SHIFT)) Model::squarifyTo(ax, ay, nx, ny);
        if (nx != bx || ny != by) {
            bx = nx;
            by = ny;
            drawCanvas();
            fb_.setClip(canvasRect());
            m.shapePreview = true;
            emitShape(ax, ay, bx, by);
            m.shapePreview = false;
            fb_.resetClip();
            redraw();
        }
        idle();
    }
    m.pushUndo();
    m.beginStroke();
    emitShape(ax, ay, bx, by);
    m.endStroke();
    m.needRepaint = true;
}

/// Протяжка рамки выделения. Обе граничные точки внутри документа.
void Editor::defineSelection(int ax, int ay) {
    int bx = ax;
    int by = ay;
    m.hasSelection = true;
    m.selX = ax;
    m.selY = ay;
    m.selW = 1;
    m.selH = 1;
    while (mousePressed() && !quitNow_) {
        const int nx = clampI(m.screenToTileX(mouseX()), 0, m.docW() - 1);
        const int ny = clampI(m.screenToTileY(mouseY()), 0, m.docH() - 1);
        if (nx != bx || ny != by) {
            bx = nx;
            by = ny;
            m.selX = std::min(ax, bx);
            m.selY = std::min(ay, by);
            m.selW = std::abs(bx - ax) + 1;
            m.selH = std::abs(by - ay) + 1;
            repaintCanvasOnly();
        }
        idle();
    }
    // Щелчок без протяжки — вставка буфера в эту точку.
    if (bx == ax && by == ay) {
        m.hasSelection = false;
        if (m.bufferFilled()) {
            m.pushUndo();
            m.pasteBufferAt(ax, ay);
        }
        m.needRepaint = true;
        return;
    }
    m.copySelectionToBuffer();
    m.needRepaint = true;
}

/// Перетаскивание выделения: содержимое копируется на новое место,
/// исходная область остаётся нетронутой.
void Editor::dragSelection(int mx, int my) {
    m.copySelectionToBuffer();
    const int offX = mx - m.selX;
    const int offY = my - m.selY;
    m.draggingSelection = true;
    while (mousePressed() && !quitNow_) {
        const int nx = m.screenToTileX(mouseX()) - offX;
        const int ny = m.screenToTileY(mouseY()) - offY;
        if (nx != m.selX || ny != m.selY) {
            m.selX = nx;
            m.selY = ny;
            repaintCanvasOnly();
        }
        idle();
    }
    m.draggingSelection = false;
    m.pushUndo();
    m.pasteBufferAt(m.selX, m.selY);
    m.selW = std::min(m.selW, m.docW());
    m.selH = std::min(m.selH, m.docH());
    m.selX = clampI(m.selX, 0, m.docW() - m.selW);
    m.selY = clampI(m.selY, 0, m.docH() - m.selH);
    m.needRepaint = true;
}

void Editor::doSelectTool(int mx, int my) {
    // В режиме тайла — выделение этапа 1: формы, сочетания, перенос.
    // Карта выделяет рамкой, как оригинал.
    if (m.docMode == DOC_TILE) {
        selectToolPro(mx, my);
        return;
    }
    if (m.hasSelection && m.selW > 0 && m.selH > 0 && mx >= m.selX && mx < m.selX + m.selW &&
        my >= m.selY && my < m.selY + m.selH) {
        dragSelection(mx, my);
    } else {
        defineSelection(mx, my);
    }
}

/// Перетаскивание объекта карты на новую клетку.
void Editor::dragObject(int idx) {
    bool moved = false;
    while (mousePressed() && !quitNow_) {
        const int nx = clampI(m.screenToTileX(mouseX()), 0, m.map.w - 1);
        const int ny = clampI(m.screenToTileY(mouseY()), 0, m.map.h - 1);
        MapObject& o = m.map.objects[static_cast<std::size_t>(idx)];
        if (nx != o.x || ny != o.y) {
            if (!moved) {
                m.pushUndo();
                moved = true;
            }
            MapObject& o2 = m.map.objects[static_cast<std::size_t>(idx)];
            o2.x = nx;
            o2.y = ny;
            repaintCanvasOnly();
        }
        idle();
    }
    m.needRepaint = true;
}

void Editor::doObjectTool(int mx, int my) {
    const int idx = m.map.objectAt(mx, my);
    if (frameClickButton_ == MB_RIGHT) {
        if (idx >= 0) {
            m.pushUndo();
            m.map.deleteObject(idx);
            m.needRepaint = true;
        }
        return;
    }
    if (idx >= 0) {
        dragObject(idx);
    } else {
        m.pushUndo();
        m.map.addObject(mapKindName(m.mapObjKind), mx, my, mapKindTemplate(m.mapObjKind));
        m.needRepaint = true;
    }
}

/// Протяжка средней кнопкой двигает изображение; щелчок без протяжки
/// меняет местами рабочие цвета.
void Editor::doMiddleDrag() {
    const int sx = frameClickX_;
    const int sy = frameClickY_;
    const int vx = m.viewX;
    const int vy = m.viewY;
    bool moved = false;
    while (mousePressed() && !quitNow_) {
        if (std::abs(mouseX() - sx) > 2 || std::abs(mouseY() - sy) > 2) moved = true;
        if (moved) {
            if (!classicUi_) pf_.setCursor(Platform::CURSOR_MOVE);
            m.viewX = vx + (mouseX() - sx);
            m.viewY = vy + (mouseY() - sy);
            m.clampView();
            repaintCanvasAndOverview();
        }
        idle();
    }
    if (moved) {
        m.needRepaint = true;
    } else {
        m.swapColors();
    }
}

/// Пробел, пока его держат, — рука: протяжка двигает холст, как во всех
/// графических редакторах. У оригинала пробел сразу вписывал картинку и
/// сбрасывал масштаб, ломая привычное движение (UX-11); короткое нажатие
/// без протяжки вписывает и теперь.
void Editor::spaceHand() {
    bool used = false;
    pf_.setCursor(Platform::CURSOR_MOVE);
    while (isKeyPressed(KEY_SPACE) && !quitNow_) {
        if (mousePressed()) {
            used = true;
            const int sx = mouseX();
            const int sy = mouseY();
            const int vx = m.viewX;
            const int vy = m.viewY;
            while (mousePressed() && !quitNow_) {
                m.viewX = vx + (mouseX() - sx);
                m.viewY = vy + (mouseY() - sy);
                m.clampView();
                repaintCanvasAndOverview();
                idle();
            }
        }
        idle();
    }
    // Щелчок, которым двигали холст, не рисует, а повторы удержанного
    // пробела не вписывают картинку заново.
    dropInput();
    std::erase_if(pf_.input().keys, [](const KeyPress& k) { return k.vk == KEY_SPACE; });
    if (!used) m.fitScale();
    m.needRepaint = true;
}

/// Открывает тайл клетки карты в редакторе тайла. true — открыл.
bool Editor::openTileOfMapCell(int x, int y) {
    const std::string base = m.mapCellTileFile(x, y);
    if (base.empty()) return false;
    const std::string path = tsetPathOf(TS_BASE, base);
    if (path.empty()) {
        showMessage("Папка набора не задана",
                    "Чтобы открывать тайлы прямо с карты, укажите папку набора — например "
                    "data/tiles игры. Это "
                    "делается в настройках или на экране наборов.");
        return false;
    }
    // Правки тайла не должны потеряться из-за щелчка по карте.
    if (m.tileModified &&
        !askYesNo("Несохранённый тайл",
                  "В открытом тайле есть изменения, которых нет в файле. Открыть " + base +
                      ".png и потерять их?",
                  "Открыть", "Отмена")) {
        return false;
    }
    if (fileExists(path)) return loadTileFrom(path, false, true);
    // Файла нет — обычное дело: тайл ещё не рисовали. Предложим завести.
    if (!askYesNo("Тайла ещё нет",
                  "Файла " + base +
                      ".png в папке набора нет. Создать пустой и начать рисовать? Он попадёт в "
                      "игру, как только вы "
                      "его сохраните.",
                  "Создать", "Отмена")) {
        return false;
    }
    m.newTileOfSize(cfg.defaultWidth, cfg.defaultHeight);
    // Имя сразу: «Сохранить» положит тайл туда, куда смотрит игра.
    m.currentFile = path;
    m.tileModified = true;
    return true;
}

void Editor::handleMapCanvas(int mx, int my) {
    // Двойной щелчок левой кнопкой открывает тайл клетки. Второй щелчок
    // пары своё обычное дело не делает: иначе двойной щелчок инструментом
    // объектов ставил бы два объекта разом.
    const int now = pf_.milliseconds();
    const bool twice = frameClickButton_ == MB_LEFT && !isKeyPressed(KEY_ALT) &&
                       mx == lastClickCX_ && my == lastClickCY_ && now - lastClickMs_ >= 0 &&
                       now - lastClickMs_ <= DOUBLE_CLICK_MS;
    lastClickMs_ = now;
    lastClickCX_ = mx;
    lastClickCY_ = my;
    if (twice) {
        // Пара засчитана: следующий щелчок начинает новую.
        lastClickCX_ = -100000;
        // Инструментом объектов двойной щелчок открывает форму объекта: у
        // оригинала она была только на средней кнопке, а у трекпада и пера
        // её нет (UX-10). Тайл клетки открывают инструменты местности.
        const int obj = m.toolMap == M_OBJ ? m.map.objectAt(mx, my) : -1;
        if (!classicUi_ && obj >= 0) {
            showObjectForm(obj);
            return;
        }
        if (openTileOfMapCell(mx, my)) return;
        m.needRepaint = true;
        return;
    }
    // Правая кнопка и Alt берут вид местности из-под курсора; инструмент
    // объектов разбирает правую кнопку сам.
    if ((frameClickButton_ == MB_RIGHT || isKeyPressed(KEY_ALT)) && m.toolMap != M_OBJ) {
        m.mapBrush = m.map.at(mx, my);
        m.needRepaint = true;
        return;
    }
    switch (m.toolMap) {
        case M_PAINT: doPaintStroke(mx, my); break;
        case M_FILL:
            m.pushUndo();
            m.floodFillMap(mx, my, m.mapBrush);
            m.needRepaint = true;
            break;
        case M_RECT: doShapeTool(mx, my); break;
        case M_OBJ: doObjectTool(mx, my); break;
        case M_SELECT: doSelectTool(mx, my); break;
        default: break;
    }
}

void Editor::handleCanvasClick() {
    const int mx = m.screenToTileX(frameClickX_);
    const int my = m.screenToTileY(frameClickY_);

    if (isMiddle(frameClickButton_)) {
        // Над объектом средняя кнопка открывает правку его строки.
        if (m.docMode == DOC_MAP && m.toolMap == M_OBJ && m.inDoc(mx, my)) {
            const int idx = m.map.objectAt(mx, my);
            if (idx >= 0) {
                showObjectForm(idx);
                return;
            }
        }
        doMiddleDrag();
        return;
    }
    if (!m.inDoc(mx, my)) {
        // Снятое щелчком мимо выделение можно вернуть: Ctrl+Shift+D.
        if (m.hasSelection) m.deselect();
        return;
    }
    if (m.docMode == DOC_MAP) {
        handleMapCanvas(mx, my);
        return;
    }
    // Alt у выделения вычитает из него, а не берёт цвет.
    const int kind = m.activeKind();
    const bool tileStamp = kind == LK_TILEMAP && !m.tilemapPixels;
    if (frameClickButton_ == MB_RIGHT || (isKeyPressed(KEY_ALT) && m.toolTile != T_SELECT) ||
        m.toolTile == T_PICK) {
        // У тайлкарты, которую правят тайлами, пипетка берёт тайл клетки.
        if (tileStamp) {
            m.tileBrush = m.tileIndexAt(mx, my);
            m.needRepaint = true;
            return;
        }
        m.pickColorFromTile(mx, my);
        return;
    }
    if (kind == LK_GROUP) {
        notify("Это группа",
               "Группа сама не рисуется: выберите слой внутри неё на ленте кадров (J).");
        return;
    }
    if (kind == LK_REFERENCE && m.toolTile != T_SELECT) {
        notify("Слой-образец",
               "Образец — подложка для срисовки: на нём не рисуют, и в выгрузку он не "
               "попадает. Выберите слой рисунка на ленте кадров (J) или клавишами PgUp и "
               "PgDn.");
        return;
    }
    const bool shapeTool = m.toolTile == T_LINE || m.toolTile == T_RECT || m.toolTile == T_ELLIPSE;
    if (kind == LK_VECTOR && !shapeTool) {
        notify("Векторный слой",
               "На векторном слое рисуют линией, прямоугольником и овалом — фигуры "
               "остаются правимыми. Чтобы рисовать точками, растрируйте слой на экране "
               "слоёв и кадров.");
        return;
    }
    // По скрытому или запертому слою кисть ничего не меняет: лучше сказать
    // об этом, чем молча не рисовать.
    if (m.toolTile != T_SELECT && !m.activeLayerEditable()) {
        const Layer& l = m.doc.layers[static_cast<std::size_t>(m.activeLayer)];
        const bool own = l.locked || !l.visible;
        notify(m.layerLockedDeep(m.activeLayer) ? "Слой заперт" : "Слой скрыт",
               "Слой «" + l.name + "» " + (m.layerLockedDeep(m.activeLayer) ? "заперт" : "скрыт") +
                   (own ? "" : " вместе со своей группой") +
                   ": рисунок на нём не меняется. Щёлкните по замку или глазу на ленте "
                   "кадров (клавиша J).");
        return;
    }
    switch (m.toolTile) {
        case T_PEN:
        case T_ERASER:
        case T_DITHER:
            // Аэрограф — вид карандаша (этап 8); тайлкарту тайлами он не
            // штампует — там карандаш кладёт тайл.
            if (m.toolTile == T_PEN && m.penMode == PEN_AIRBRUSH && !tileStamp) {
                doAirbrushStroke(mx, my);
            } else if (m.toolTile == T_PEN && m.penMode == PEN_SHADE && !tileStamp) {
                // Shift — затенение в другую сторону, только на этот мазок.
                const bool flip = isKeyPressed(KEY_SHIFT);
                m.shadeDarker = m.shadeDarker != flip;
                doPaintStroke(mx, my);
                m.shadeDarker = m.shadeDarker != flip;
            } else {
                doPaintStroke(mx, my);
            }
            break;
        case T_FILL:
            if (tileStamp) {
                m.fillTilesAt(mx, my);
                break;
            }
            if (m.fillGradient) {
                doGradientTool(mx, my);
                break;
            }
            m.pushUndo();
            // В строгом индексном режиме заливка — цветом ячейки.
            m.floodFillTile(mx, my, m.fillColor());
            m.needRepaint = true;
            break;
        case T_LINE:
        case T_RECT:
        case T_ELLIPSE:
            // Фигуры векторного слоя и линии особых видов — векторным
            // инструментом; прямая и фигуры растра — как в оригинале.
            // Сглаженные (этап 8) — тоже фигурой: её растр умеет край долями.
            if (kind == LK_VECTOR || m.antialias ||
                (m.toolTile == T_LINE && m.lineKind != LINE_STRAIGHT)) {
                doVectorTool(mx, my);
            } else {
                doShapeTool(mx, my);
            }
            break;
        case T_SELECT: doSelectTool(mx, my); break;
        default: break;
    }
}

// ---------------------------------------------------------------------------
// Панель инструментов
// ---------------------------------------------------------------------------

void Editor::dragHSVSquare() {
    while (mousePressed() && !quitNow_) {
        m.sat = axisToPercent(mouseX() - HSV_X0, BAR_SIZE);
        m.val = axisToValue(mouseY() - HSV_Y0, BAR_SIZE);
        m.applyHSVToColor();
        repaintColorOnly();
        idle();
    }
    m.needRepaint = true;
}

void Editor::dragHueBar() {
    while (mousePressed() && !quitNow_) {
        m.hue = axisToHue(mouseX() - HSV_X0, BAR_SIZE);
        m.applyHSVToColor();
        repaintColorOnly();
        idle();
    }
    m.needRepaint = true;
}

void Editor::dragAlphaBar() {
    while (mousePressed() && !quitNow_) {
        m.firstColor = argb(axisToByte(mouseX() - HSV_X0, BAR_SIZE), m.firstColor.r, m.firstColor.g,
                            m.firstColor.b);
        repaintColorOnly();
        idle();
    }
    m.needRepaint = true;
}

void Editor::handlePaletteClick() {
    if (isMiddle(frameClickButton_)) {
        // Средняя кнопка открывала загрузку и сохранение палитры; теперь —
        // экран палитры, где есть и они.
        showPaletteScreen();
        return;
    }
    int cj = 0;
    int ci = 0;
    if (!Model::palHit(frameClickX_, frameClickY_, cj, ci)) return;
    const int cell = m.palCellIndex(cj, ci);
    if (cell < 0) {
        // Пустая клетка за концом палитры: левая кнопка добавляет первый цвет.
        if (frameClickButton_ == MB_LEFT) m.addPaletteColor(m.firstColor, -1);
        return;
    }
    const auto idx = static_cast<PalIndex>(cell);
    const Color palCell = m.palette[idx];
    // У оригинала левая кнопка записывала в ячейку первый цвет, а брала
    // цвет правая. Во всех прочих редакторах щелчок по образцу цвет
    // выбирает, и новичок первым же движением портил палитру (UX-20).
    // Теперь левая берёт цвет первым, правая — вторым, а запись — только
    // Ctrl+щелчок.
    const bool write = frameClickButton_ == MB_LEFT && (classicUi_ || isKeyPressed(KEY_CTRL));
    if (!write) {
        if (classicUi_ || frameClickButton_ == MB_LEFT) {
            m.pickColor(m.effectiveColor(idx));
            // Цвет взят из конкретной ячейки: привязка точная, а не выведенная
            // по цвету, — когда цвет стоит в палитре дважды, важно, из какой.
            m.firstIdx = idx;
        } else {
            m.pickSecondFromPalette(idx);
        }
        return;
    }
    const Color oldC = palCell;
    if (sameColor(oldC, m.firstColor)) return;
    // Запись в ячейку отменяется всегда: у оригинала вне индексного
    // режима она шла мимо истории (docs/fixes.md).
    m.pushUndo();
    if (m.indexedOn && m.docMode == DOC_TILE) {
        // Пиксели и сама ячейка меняются одной операцией и одной же отменяются.
        m.palette[idx] = m.firstColor;
        m.recolorPaletteEntry(idx, oldC, m.firstColor);
        m.enforceIndexed();
        // Второй рабочий цвет, взятый из этой ячейки, едет за ней следом.
        if (m.secondIdx == idx)
            m.secondColor = argb(m.secondColor.a, m.firstColor.r, m.firstColor.g, m.firstColor.b);
        m.overDirty = true;
        m.tileBmpDirty = true;
    } else {
        m.palette[idx] = m.firstColor;
    }
    m.needRepaint = true;
}

/// Протяжка по обзору двигает холст, а панорама холста двигает рамку.
void Editor::doOverviewDrag() {
    while (mousePressed() && !quitNow_) {
        m.overviewCenterOn(mouseX(), mouseY());
        repaintCanvasAndOverview();
        idle();
    }
    m.needRepaint = true;
}

/// Щелчок по панели: ветки взаимоисключающие, щелчок разбирает ровно одна
/// секция.
void Editor::handlePanelClick() {
    const int py = frameClickY_;
    int idx = 0;
    if (py >= TOOLS_Y0 && py < TOOLS_Y1) {
        if (m.toolHit(frameClickX_, frameClickY_, idx)) {
            m.setTool(idx);
            // Правая кнопка по инструменту тайла — его параметры; у линии и
            // фигур — параметры фигур (этап 6).
            // На карте правая кнопка по кисти, прямоугольнику или заливке —
            // кисть карты: одной местностью или смесью (этап 10).
            if (m.docMode == DOC_MAP && frameClickButton_ == MB_RIGHT &&
                (idx == M_PAINT || idx == M_FILL || idx == M_RECT))
                showMapBrushOptions();
            if (m.docMode == DOC_TILE && frameClickButton_ == MB_RIGHT) {
                if (idx == T_LINE || idx == T_RECT || idx == T_ELLIPSE) {
                    showShapeOptions();
                } else if (idx == T_PEN) {
                    showBrushOptions();
                } else if (idx == T_FILL) {
                    showFillOptions();
                } else {
                    showToolOptions();
                }
            }
        } else if (m.docMode == DOC_TILE) {
            const int chip = chipHit(frameClickX_, frameClickY_);
            if (chip >= 0) handleChipClick(chip);
        }
    } else if (py >= OVER_Y0 && py < OVER_Y1) {
        if (isMiddle(frameClickButton_)) {
            doOverviewDrag();
        } else {
            m.overviewCenterOn(frameClickX_, frameClickY_);
        }
    } else if (py >= MENU_Y0 && py < MENU_Y1) {
        if (classicUi_ || frameClickButton_ == MB_LEFT) showMainMenu();
    } else if (m.docMode == DOC_MAP) {
        if (py >= SLOT1_Y0 && py < SLOT1_Y1) {
            if (Model::terrHit(frameClickX_, frameClickY_, idx)) {
                m.mapBrush = idx;
                m.needRepaint = true;
            }
        } else if (py >= SLOT2_Y0 && py < ALPHA_Y1 &&
                   Model::objHit(frameClickX_, frameClickY_, idx)) {
            m.mapObjKind = idx;
            m.needRepaint = true;
        }
    } else if (py >= PAL_Y0 && py < PAL_Y1) {
        handlePaletteClick();
    } else if (py >= HSV_Y0 && py < HSV_Y1) {
        if (isMiddle(frameClickButton_)) {
            showColorScreen();
        } else {
            dragHSVSquare();
        }
    } else if (py >= HUE_Y0 && py < HUE_Y1) {
        if (isMiddle(frameClickButton_)) {
            showColorScreen();
        } else {
            dragHueBar();
        }
    } else if (py >= ALPHA_Y0 && py < ALPHA_Y1) {
        if (isMiddle(frameClickButton_)) {
            showColorScreen();
        } else {
            dragAlphaBar();
        }
    }
}

// ---------------------------------------------------------------------------
// Клавиатура
// ---------------------------------------------------------------------------

void Editor::handleModifiedKey(int key) {
    const bool tile = m.docMode == DOC_TILE;
    if (!lastKey_.ctrl || lastKey_.alt) return;
    // Файл, отмена и поиск — по общим правилам настольных программ.
    switch (key) {
        case KEY_N: commandNew(); return;
        case KEY_O: commandOpen(); return;
        case KEY_S:
            if (lastKey_.shift) {
                commandSaveAs();
            } else {
                commandSave();
            }
            return;
        case KEY_Z:
            if (lastKey_.shift) {
                m.doRedo();
            } else {
                m.doUndo();
            }
            return;
        case KEY_Y: m.doRedo(); return;
        case KEY_0:
            m.fitScale();
            m.needRepaint = true;
            return;
        case KEY_F:
            if (!tile) showMapFindScreen();
            return;
        case KEY_B:
            // К следующему месту местности тайла на карте (этап 7).
            ++mapCtxIndex_;
            m.needRepaint = true;
            return;
        default: break;
    }
    if (handleGuideKey(key)) return;
    handleProCtrlKey(key);
}

void Editor::handleEditorKey(int key) {
    // У оригинала Ctrl с буквой разбирался как сама буква: Ctrl+O открывал
    // параметры, Ctrl+Shift+Z был ещё одной отменой, Ctrl+F на тайле
    // включал ось симметрии (UX-16). Теперь у Ctrl и Alt — своя таблица.
    if (!classicUi_ && (lastKey_.ctrl || lastKey_.alt)) {
        handleModifiedKey(key);
        return;
    }
    if (handleGuideKey(key)) return;
    if (handleProKey(key)) return;
    if (handleTimelineKey(key)) return;
    const int step = std::max(1, m.pixelSize * 4);
    const bool tile = m.docMode == DOC_TILE;
    if (!tile && lastKey_.ctrl && key == KEY_F) {
        showMapFindScreen();
        return;
    }
    // Однобуквенные команды, которые пишут файл или меняют всю картинку,
    // в интерфейсе 1.7 — только с Ctrl или на экране «Изображение»: промах
    // по клавише инструмента не должен перезаписать файл (UX-17).
    const bool letters = classicUi_;
    switch (key) {
        case KEY_ESC:
        case KEY_M: showMainMenu(); break;
        case KEY_H: showHelpScreen(); break;
        case KEY_N:
            if (letters) commandNew();
            break;
        case KEY_L:
            if (letters) commandOpen();
            break;
        case KEY_S:
            if (letters) commandSave();
            break;
        case KEY_A: commandSaveSelection(); break;
        case KEY_P:
            if (tile) showColorScreen();
            break;
        case KEY_Z: m.doUndo(); break;
        case KEY_Y: m.doRedo(); break;
        case KEY_X: m.swapColors(); break;
        case KEY_R:
            if (tile && letters) m.randomizeTile();
            break;
        case KEY_DELETE:
            // Delete с выделением разобран выше; без выделения у оригинала
            // он очищал весь тайл.
            if (tile && letters) m.clearTile();
            break;
        case KEY_TAB: m.switchMode(tile ? DOC_MAP : DOC_TILE); break;
        case KEY_ENTER:
            // Enter над объектом карты — его форма (UX-10); в режиме тайла
            // Enter разобран лентой кадров.
            if (!tile && !classicUi_) {
                const int mx = m.screenToTileX(mouseX());
                const int my = m.screenToTileY(mouseY());
                const int obj = m.inDoc(mx, my) ? m.map.objectAt(mx, my) : -1;
                if (obj >= 0) showObjectForm(obj);
            }
            break;
        case KEY_V:
            if (m.bufferFilled()) {
                m.pushUndo();
                m.pasteBufferAt(m.screenToTileX(mouseX()), m.screenToTileY(mouseY()));
                m.needRepaint = true;
            }
            break;
        case KEY_G:
            m.gridOn = !m.gridOn;
            cfg.showGrid = m.gridOn;
            saveConfig();
            m.needRepaint = true;
            break;
        case KEY_C:
            m.checkerOn = !m.checkerOn;
            cfg.showChecker = m.checkerOn;
            saveConfig();
            m.needRepaint = true;
            break;
        case KEY_B:
            // Shift+B — картинки набора: на карте — карта тайлами, у тайла
            // местности — его окружение на настоящей карте; Ctrl+B — к
            // следующему месту этой местности (этап 7).
            if (lastKey_.shift) {
                if (tile) {
                    toggleMapContext();
                } else {
                    m.mapTiles = !m.mapTiles;
                    gameTilesDirty_ = true;
                }
            } else if (lastKey_.ctrl) {
                ++mapCtxIndex_;
            } else {
                m.seamlessOn = !m.seamlessOn;
                cfg.showSeamless = m.seamlessOn;
                saveConfig();
            }
            m.needRepaint = true;
            break;
        case KEY_W:
            m.wrapOn = !m.wrapOn;
            m.needRepaint = true;
            break;
        case KEY_F:
            m.symX = !m.symX;
            m.needRepaint = true;
            break;
        case KEY_D:
            m.symY = !m.symY;
            m.needRepaint = true;
            break;
        case KEY_E:
            m.brushReplace = !m.brushReplace;
            m.needRepaint = true;
            break;
        case KEY_T:
            if (tile) showTilesetScreen();
            break;
        case KEY_I:
            if (tile) toggleIndexed();
            break;
        case KEY_PLUS:
        case KEY_NUMPLUS: m.zoomAt(1, m.W / 2, m.H / 2); break;
        case KEY_MINUS:
        case KEY_NUMMINUS: m.zoomAt(-1, m.W / 2, m.H / 2); break;
        case KEY_LEFT:
            m.viewX += step;
            m.clampView();
            m.needRepaint = true;
            break;
        case KEY_RIGHT:
            m.viewX -= step;
            m.clampView();
            m.needRepaint = true;
            break;
        case KEY_UP:
            m.viewY += step;
            m.clampView();
            m.needRepaint = true;
            break;
        case KEY_DOWN:
            m.viewY -= step;
            m.clampView();
            m.needRepaint = true;
            break;
        case KEY_SPACE:
            if (classicUi_) {
                m.fitScale();
                m.needRepaint = true;
            } else {
                spaceHand();
            }
            break;
        default:
            // Цифры выбирают инструмент. Оригинал прижимал номер к числу
            // инструментов, и в режиме карты клавиши 6..9 все выбирали
            // выделение; здесь лишние цифры ничего не делают.
            if (key >= KEY_1 && key <= KEY_9 && key - KEY_1 < m.toolCount()) m.setTool(key - KEY_1);
            break;
    }
}

}  // namespace tjtm
