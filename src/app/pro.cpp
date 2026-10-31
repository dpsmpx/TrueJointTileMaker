// Этап 1 RoadToProLevel в окне: выделение произвольной формы и его
// перемещение, преобразования картинки, параметры инструментов, клавиши.
//
// Раскладка окна оригинала не меняется: новые режимы кисти встают в три
// пустые ячейки третьего ряда инструментов, а команды — на экран
// «Изображение и выделение», куда ведёт бывший пункт меню «Операции с
// цветом». Режим карты остаётся таким, каким был.

#include <algorithm>
#include <cstdio>

#include "app/editor.h"
#include "core/imageops.h"
#include "core/pmath.h"
#include "core/theme.h"

namespace tjtm {

using namespace theme;

namespace {

constexpr int kChipFirst = T_TILE_COUNT;  // ячейки после девяти инструментов
constexpr int kChipCount = 3;

std::string halfText(int v2) {
    // Ось в половинах точки: 33 — это «16½».
    std::string s = std::to_string(v2 / 2);
    if (v2 % 2 != 0) s += "½";
    return s;
}

std::string shapeName(int s) {
    switch (s) {
        case SS_ELLIPSE: return "Овал";
        case SS_LASSO: return "Лассо";
        case SS_WAND: return "Палочка";
        case SS_COLOR: return "По цвету";
        case SS_POLYGON: return "Многоугольник";
        default: return "Рамка";
    }
}

}  // namespace

// ---------------------------------------------------------------------------
// Ячейки режимов на панели инструментов
// ---------------------------------------------------------------------------

int Editor::chipHit(int px, int py) {
    for (int c = 0; c < kChipCount; ++c) {
        const int x0 = Model::toolCellX(kChipFirst + c);
        const int y0 = Model::toolCellY(kChipFirst + c);
        if (px >= x0 && px < x0 + TOOL_CW - 2 && py >= y0 && py < y0 + TOOL_CH - 2) return c;
    }
    return -1;
}

void Editor::drawToolChips() {
    if (m.docMode != DOC_TILE) return;
    for (int c = 0; c < kChipCount; ++c) {
        const int x0 = Model::toolCellX(kChipFirst + c);
        const int y0 = Model::toolCellY(kChipFirst + c);
        const int x1 = x0 + TOOL_CW - 2;
        const int y1 = y0 + TOOL_CH - 2;
        // Включённый режим обведён цветом выделения, как выбранный
        // инструмент, но не залит им: это режим, а не инструмент.
        const bool on = (c == 0 && !m.brushReplace) || (c == 1 && m.pixelPerfect);
        const bool over = mouseX() >= x0 && mouseX() < x1 && mouseY() >= y0 && mouseY() < y1;
        fb_.fillRoundRect(x0, y0, x1, y1, 5, 5, on ? cAccent : cBorder);
        fb_.fillRoundRect(x0 + 1, y0 + 1, x1 - 1, y1 - 1, 5, 5, (on || over) ? cHover : cBack);
        std::string label;
        switch (c) {
            case 0: label = m.brushReplace ? "Зам." : "Смесь"; break;
            case 1: label = "ПП"; break;
            default: label = "…"; break;
        }
        textCentered(x0, y0, x1, y1, label, cText);
    }
}

void Editor::handleChipClick(int chip) {
    switch (chip) {
        case 0: m.brushReplace = !m.brushReplace; break;
        case 1: m.pixelPerfect = !m.pixelPerfect; break;
        default: showToolOptions(); break;
    }
    m.needRepaint = true;
}

// ---------------------------------------------------------------------------
// Инструмент выделения
// ---------------------------------------------------------------------------

void Editor::selectToolPro(int mx, int my) {
    const bool shift = isKeyPressed(KEY_SHIFT);
    const bool alt = isKeyPressed(KEY_ALT);
    // Shift добавляет, Alt вычитает, оба — пересекают: как в Aseprite.
    // Пипетка по Alt у выделения поэтому выключена; правая кнопка — по-прежнему.
    int op = m.selOp;
    if (shift && alt) {
        op = img::SEL_INTERSECT;
    } else if (shift) {
        op = img::SEL_ADD;
    } else if (alt) {
        op = img::SEL_SUBTRACT;
    }
    if (m.selShape == SS_WAND || m.selShape == SS_COLOR) {
        m.selectByColorAt(mx, my, m.selShape == SS_WAND, op);
        if (m.hasSelection) m.copySelection();
        m.needRepaint = true;
        waitMouseRelease();
        return;
    }
    // Протяжка за выделенное без Shift и Alt двигает содержимое; с Ctrl —
    // копирует, оставляя исходное на месте.
    if (!shift && !alt && m.isSelected(mx, my)) {
        moveSelectionPro(mx, my, isKeyPressed(KEY_CTRL));
        return;
    }
    if (m.selShape == SS_POLYGON) {
        definePolygonSelection(mx, my, op);
        return;
    }
    defineSelectionPro(mx, my, op);
}

/// Многоугольник по вершинам: щелчок ставит вершину, щелчок по первой,
/// двойной щелчок или Enter замыкают, Backspace снимает последнюю,
/// Escape и правая кнопка отменяют. Отрезок к курсору виден всё время.
void Editor::definePolygonSelection(int ax, int ay, int op) {
    const Grid<std::uint8_t> base = m.selectionMask();
    const bool had = m.hasSelection;
    std::vector<std::pair<int, int>> pts{{ax, ay}};
    waitMouseRelease();
    dropInput();
    pf_.input().keys.clear();
    bool closed = false;
    bool cancelled = false;
    int lastMs = pf_.milliseconds();
    auto previewLine = [&](int x0, int y0, int x1, int y1) {
        const int dx = std::abs(x1 - x0);
        const int dy = -std::abs(y1 - y0);
        const int sx = x0 < x1 ? 1 : -1;
        const int sy = y0 < y1 ? 1 : -1;
        int err = dx + dy;
        while (true) {
            drawPreviewPixel(x0, y0);
            if (x0 == x1 && y0 == y1) break;
            const int e2 = 2 * err;
            if (e2 >= dy) {
                err += dy;
                x0 += sx;
            }
            if (e2 <= dx) {
                err += dx;
                y0 += sy;
            }
        }
    };
    while (!closed && !cancelled && !quitNow_) {
        beginFrameInput();
        const int cx = std::clamp(m.screenToTileX(mouseX()), 0, m.docW() - 1);
        const int cy = std::clamp(m.screenToTileY(mouseY()), 0, m.docH() - 1);
        drawCanvas();
        for (std::size_t k = 1; k < pts.size(); ++k)
            previewLine(pts[k - 1].first, pts[k - 1].second, pts[k].first, pts[k].second);
        previewLine(pts.back().first, pts.back().second, cx, cy);
        // Первая вершина обведена: щелчок по ней замыкает фигуру.
        const int fx = m.tileToScreenX(pts.front().first);
        const int fy = m.tileToScreenY(pts.front().second);
        fb_.setClip(canvasRect());
        fb_.frameRect(fx - 3, fy - 3, fx + m.pixelSize + 3, fy + m.pixelSize + 3, cAccent);
        fb_.resetClip();
        redraw();
        if (frameClick_) {
            frameClick_ = false;
            const int px = std::clamp(m.screenToTileX(frameClickX_), 0, m.docW() - 1);
            const int py = std::clamp(m.screenToTileY(frameClickY_), 0, m.docH() - 1);
            const int now = pf_.milliseconds();
            if (frameClickButton_ == MB_RIGHT) {
                cancelled = true;
            } else if (pts.size() >= 3 && std::abs(px - pts.front().first) <= 1 &&
                       std::abs(py - pts.front().second) <= 1) {
                closed = true;
            } else if (std::make_pair(px, py) == pts.back() && now - lastMs <= DOUBLE_CLICK_MS) {
                closed = pts.size() >= 3;
            } else {
                pts.emplace_back(px, py);
            }
            lastMs = now;
        }
        switch (takeKey()) {
            case KEY_ENTER: closed = pts.size() >= 3; break;
            case KEY_ESC: cancelled = true; break;
            case KEY_BACK:
                if (pts.size() > 1) pts.pop_back();
                break;
            default: break;
        }
        idle();
    }
    waitMouseRelease();
    if (cancelled || !closed) {
        if (had) {
            m.setSelectionMask(base);
        } else {
            m.resetSelection();
        }
        m.needRepaint = true;
        return;
    }
    Grid<std::uint8_t> cur = base;
    img::combine(cur, img::maskPolygon(m.docW(), m.docH(), pts), op);
    m.setSelectionMask(cur);
    if (m.hasSelection) m.copySelection();
    m.needRepaint = true;
}

void Editor::defineSelectionPro(int ax, int ay, int op) {
    const Grid<std::uint8_t> base = m.selectionMask();
    const bool had = m.hasSelection;
    std::vector<std::pair<int, int>> pts{{ax, ay}};
    int bx = ax;
    int by = ay;
    bool moved = false;
    auto update = [&]() {
        Grid<std::uint8_t> shape;
        switch (m.selShape) {
            case SS_ELLIPSE: shape = img::maskEllipse(m.docW(), m.docH(), ax, ay, bx, by); break;
            case SS_LASSO: shape = img::maskPolygon(m.docW(), m.docH(), pts); break;
            default: shape = img::maskRect(m.docW(), m.docH(), ax, ay, bx, by); break;
        }
        Grid<std::uint8_t> cur = base;
        img::combine(cur, shape, op);
        m.setSelectionMask(cur);
        repaintCanvasOnly();
    };
    update();
    while (mousePressed() && !quitNow_) {
        const int nx = clampI(m.screenToTileX(mouseX()), 0, m.docW() - 1);
        const int ny = clampI(m.screenToTileY(mouseY()), 0, m.docH() - 1);
        if (nx != bx || ny != by) {
            bx = nx;
            by = ny;
            moved = true;
            pts.emplace_back(nx, ny);
            update();
        }
        idle();
    }
    if (!moved) {
        if (op == img::SEL_REPLACE) {
            // Щелчок без протяжки, как в оригинале, вставляет буфер в эту
            // точку; вставленное сразу выделено и его можно двигать.
            m.deselect();
            if (m.bufferFilled()) m.pasteAsSelection(ax, ay);
        } else if (had) {
            m.setSelectionMask(base);
        } else {
            m.resetSelection();
        }
        m.needRepaint = true;
        return;
    }
    // Выделенное сразу в буфере — как у оригинала: обвёл и вставляй.
    if (m.hasSelection) m.copySelection();
    m.needRepaint = true;
}

void Editor::moveSelectionPro(int mx, int my, bool copy) {
    // Содержимое вынимается целиком — с маской, а не рамкой: овал везёт
    // овал. При переносе исходное место становится прозрачным.
    m.pushUndo();
    Grid<Color> content;
    Grid<std::uint8_t> mask;
    m.liftSelection(!copy, content, mask);
    m.buffer = content;
    const int offX = mx - m.selX;
    const int offY = my - m.selY;
    const int startX = m.selX;
    const int startY = m.selY;
    // Пока тащат, выделение рисуется рамкой вынутого: так его видно и
    // тогда, когда часть ушла за край.
    m.selMask.clear();
    m.draggingSelection = true;
    repaintCanvasOnly();
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
    const bool moved = m.selX != startX || m.selY != startY;
    m.placeFloating(content, mask, m.selX, m.selY);
    // Щелчок без протяжки ничего не изменил: шаг истории ему не нужен.
    if (!moved && !copy && !m.undoStack.empty()) m.undoStack.pop_back();
    m.needRepaint = true;
}

/// Выделение произвольной формы: подкраска по точкам и «муравьи» по
/// границе. Прямоугольник рисуется кодом оригинала, и его вид не меняется.
void Editor::drawSelectionMask() {
    if (m.selMask.empty() || !m.hasSelection) return;
    fb_.setClip(canvasRect());
    const Color tint = argb(64, cAccent.r, cAccent.g, cAccent.b);
    for (int y = m.selY; y < m.selY + m.selH; ++y) {
        int x = m.selX;
        while (x < m.selX + m.selW) {
            if (!m.isSelected(x, y)) {
                ++x;
                continue;
            }
            const int run0 = x;
            while (x < m.selX + m.selW && m.isSelected(x, y))
                ++x;
            fb_.fillRect(m.tileToScreenX(run0), m.tileToScreenY(y), m.tileToScreenX(x),
                         m.tileToScreenY(y + 1), tint);
        }
    }
    const Color light = argb(255, 245, 245, 250);
    const Color dark = argb(255, 20, 20, 24);
    // Узор «муравьёв» привязан к экрану по диагонали: бежит одинаково по
    // всем краям, сколько бы их ни было у фигуры.
    auto ant = [&](int px, int py) {
        const bool d = ((px + py - antsPhase_) & 7) < 4;
        fb_.blendPixel(px, py, d ? dark : light);
    };
    for (int y = m.selY; y < m.selY + m.selH; ++y) {
        for (int x = m.selX; x < m.selX + m.selW; ++x) {
            if (!m.isSelected(x, y)) continue;
            const int sx0 = m.tileToScreenX(x);
            const int sy0 = m.tileToScreenY(y);
            const int sx1 = m.tileToScreenX(x + 1);
            const int sy1 = m.tileToScreenY(y + 1);
            if (!m.isSelected(x - 1, y)) {
                for (int py = sy0 - 1; py <= sy1; ++py)
                    ant(sx0 - 1, py);
            }
            if (!m.isSelected(x + 1, y)) {
                for (int py = sy0 - 1; py <= sy1; ++py)
                    ant(sx1, py);
            }
            if (!m.isSelected(x, y - 1)) {
                for (int px = sx0 - 1; px <= sx1; ++px)
                    ant(px, sy0 - 1);
            }
            if (!m.isSelected(x, y + 1)) {
                for (int px = sx0 - 1; px <= sx1; ++px)
                    ant(px, sy1);
            }
        }
    }
    fb_.resetClip();
}

// ---------------------------------------------------------------------------
// Клавиши
// ---------------------------------------------------------------------------

void Editor::pasteBufferHere() {
    if (!m.bufferFilled()) return;
    int tx = m.screenToTileX(mouseX());
    int ty = m.screenToTileY(mouseY());
    // Курсор над панелью — вставка в начало картинки, а не за край.
    if (!m.inDoc(tx, ty)) {
        tx = 0;
        ty = 0;
    }
    m.pasteAsSelection(tx, ty);
}

bool Editor::handleProCtrlKey(int key) {
    if (m.docMode != DOC_TILE) return false;
    // Модификаторы — того мига, когда клавишу нажали, а не нынешние.
    const bool shift = lastKey_.shift;
    switch (key) {
        case KEY_T: showTilePickerScreen(); return true;
        case KEY_E:
            // Кисть из выделения (этап 9): сразу карандашом этого вида.
            if (m.brushFromSelection()) {
                m.setTool(T_PEN);
                m.needRepaint = true;
            } else {
                showMessage("Кисть из выделения",
                            "Выделите кусок активного слоя — он станет кистью карандаша.");
            }
            return true;
        case KEY_A: m.selectAll(); return true;
        case KEY_D:
            if (shift) {
                m.reselect();
            } else {
                m.deselect();
            }
            return true;
        case KEY_I: m.invertSelection(); return true;
        case KEY_C: m.copySelection(); return true;
        case KEY_X: m.cutSelection(); return true;
        case KEY_V: pasteBufferHere(); return true;
        default: return false;
    }
}

bool Editor::handleProKey(int key) {
    if (m.docMode != DOC_TILE) return false;
    if (lastKey_.ctrl && handleProCtrlKey(key)) return true;
    const bool shift = lastKey_.shift;
    switch (key) {
        case KEY_V: pasteBufferHere(); return true;
        case KEY_DELETE:
            // На векторном слое Delete убирает последнюю тронутую фигуру.
            if (m.activeKind() == LK_VECTOR) {
                if (vecSel_ >= 0 && m.deleteVectorShape(vecSel_)) vecSel_ = -1;
                return true;
            }
            // С выделением Delete стирает выделенное; без него — весь тайл.
            if (!m.hasSelection) return false;
            m.eraseSelection();
            return true;
        case KEY_Q:
            m.pixelPerfect = !m.pixelPerfect;
            m.needRepaint = true;
            return true;
        case KEY_O: showToolOptions(); return true;
        case KEY_K:
            // Shift+K — фильтры (этап 8): рядом с экраном изображения, но
            // отдельно от пиксельных операций.
            if (shift) {
                showFiltersScreen();
            } else {
                showImageScreen();
            }
            return true;
        case KEY_U: showPaletteScreen(); return true;
        case KEY_LBRACKET:
        case KEY_RBRACKET: {
            const int n = img::ditherPatternCount();
            m.ditherPattern = (m.ditherPattern + (key == KEY_RBRACKET ? 1 : n - 1)) % n;
            m.needRepaint = true;
            return true;
        }
        default: return false;
    }
}

// ---------------------------------------------------------------------------
// Параметры инструментов
// ---------------------------------------------------------------------------

void Editor::showToolOptions() {
    bool done = false;
    dropInput();
    pf_.input().keys.clear();
    constexpr int LX = 40;
    constexpr int RX = 420;
    while (!done && !quitNow_) {
        beginFrameInput();
        fb_.clear(cWindow);
        textCentered(0, 12, WIN_W, 40, "Параметры инструментов", cText);

        // --- кисть ---
        uiLabelLeft(LX, 46, 24, "Карандаш, ластик, узор", cTextDim);
        if (uiToggle(LX, 72, 160, 30, "Замена цвета", m.brushReplace)) m.brushReplace = true;
        if (uiToggle(LX + 170, 72, 160, 30, "Смешивание", !m.brushReplace)) m.brushReplace = false;
        if (uiToggle(LX, 110, 330, 30, "«Пиксель-перфект»: без углов в мазке  (Q)", m.pixelPerfect))
            m.pixelPerfect = !m.pixelPerfect;
        // Сила кисти в процентах: так её задают во всех редакторах.
        {
            const int before = pround(m.brushOpacity * 100.0 / 255);
            int pct = before;
            uiLabelLeft(LX, 148, 30, "Сила кисти", cText);
            if (uiButton(LX + 110, 148, 44, 30, "−10", pct > 1)) pct = std::max(1, pct - 10);
            if (uiButton(LX + 158, 148, 36, 30, "−1", pct > 1)) pct = std::max(1, pct - 1);
            fb_.fillRoundRect(LX + 198, 148, LX + 254, 178, 6, 6, cFace);
            uiLabel(LX + 198, 148, 56, 30, std::to_string(pct) + "%", cText);
            if (uiButton(LX + 258, 148, 36, 30, "+1", pct < 100)) pct = std::min(100, pct + 1);
            if (uiButton(LX + 298, 148, 44, 30, "+10", pct < 100)) pct = std::min(100, pct + 10);
            if (pct != before) m.brushOpacity = std::clamp(pround(pct * 255.0 / 100), 1, 255);
        }

        // --- узоры ---
        uiLabelLeft(LX, 186, 24, "Узор инструмента «Узор»  ([ и ] — листать)", cTextDim);
        const int n = img::ditherPatternCount();
        for (int p = 0; p < n; ++p) {
            const int cx = LX + (p % 2) * 170;
            const int cy = 212 + (p / 2) * 38;
            if (uiToggle(cx, cy, 160, 34, "", m.ditherPattern == p)) m.ditherPattern = p;
            // Образец узора первым и вторым цветом, по две точки экрана на точку.
            for (int py = 0; py < 12; ++py) {
                for (int px = 0; px < 12; ++px) {
                    const Color c = img::ditherFirst(p, px, py) ? m.firstColor : m.secondColor;
                    fb_.fillRect(cx + 6 + px * 2, cy + 5 + py * 2, cx + 8 + px * 2, cy + 7 + py * 2,
                                 blendOver(c, m.checkerColor(px, py)));
                }
            }
            uiLabelLeft(cx + 38, cy, 34, img::ditherPatternName(p), cText);
        }

        // --- выделение ---
        uiLabelLeft(RX, 46, 24, "Выделение", cTextDim);
        for (int s = 0; s < SS_COUNT; ++s) {
            if (uiToggle(RX + (s % 3) * 110, 72 + (s / 3) * 38, 104, 30, shapeName(s),
                         m.selShape == s))
                m.selShape = s;
        }
        uiLabelLeft(RX, 150, 24, "Действие без клавиш", cTextDim);
        static const char* kOps[4] = {"Заменить", "Добавить", "Вычесть", "Пересечь"};
        for (int o = 0; o < 4; ++o) {
            if (uiToggle(RX + (o % 2) * 165, 176 + (o / 2) * 38, 160, 30, kOps[o], m.selOp == o))
                m.selOp = o;
        }
        uiLabelLeft(RX, 252, 20, "Shift — добавить, Alt — вычесть, оба — пересечь.", cTextDim);
        uiLabelLeft(RX, 272, 20, "Протяжка за выделенное двигает, с Ctrl — копирует.", cTextDim);

        // --- симметрия ---
        uiLabelLeft(RX, 304, 24, "Симметрия: положение осей", cTextDim);
        auto axisRow = [&](int y, bool& on, int& axis2, int size, const std::string& caption) {
            if (uiToggle(RX, y, 110, 30, caption, on)) on = !on;
            const int cur = axis2 < 0 ? size : axis2;
            fb_.fillRoundRect(RX + 116, y, RX + 186, y + 30, 6, 6, cFace);
            uiLabel(RX + 116, y, 70, 30, halfText(cur), cText);
            if (uiButton(RX + 192, y, 40, 30, "−½", cur > 1)) axis2 = cur - 1;
            if (uiButton(RX + 238, y, 40, 30, "+½", cur < 2 * size - 1)) axis2 = cur + 1;
            if (uiButton(RX + 284, y, 46, 30, "центр", axis2 >= 0)) axis2 = -1;
        };
        if (m.docMode == DOC_TILE) {
            axisRow(330, m.symX, m.symAxisX2, m.tile.w, "Ось X  (F)");
            axisRow(368, m.symY, m.symAxisY2, m.tile.h, "Ось Y  (D)");
        }
        // Радиальная: лучи вокруг точки пересечения осей.
        uiLabelLeft(RX, 406, 30, "Лучей", cText);
        static const int kRays[5] = {0, 3, 4, 6, 8};
        for (int k = 0; k < 5; ++k) {
            const std::string cap = kRays[k] == 0 ? "нет" : std::to_string(kRays[k]);
            if (uiToggle(RX + 60 + k * 54, 406, 50, 30, cap,
                         m.symRadial == kRays[k] || (kRays[k] == 0 && m.symRadial < 2)))
                m.symRadial = kRays[k];
        }
        if (uiButton(RX, 450, 330, 30, "Сетка и направляющие…   Ctrl+G", true)) showGridScreen();

        if (uiButton(WIN_W / 2 - 90, WIN_H - 52, 180, 36, "Закрыть", true)) done = true;
        redraw();
        const int key = takeKey();
        if (key == KEY_ESC || key == KEY_ENTER || key == KEY_O) done = true;
        if (key == KEY_LBRACKET || key == KEY_RBRACKET)
            m.ditherPattern = (m.ditherPattern + (key == KEY_RBRACKET ? 1 : n - 1)) % n;
        if (key == KEY_Q) m.pixelPerfect = !m.pixelPerfect;
        idle();
    }
    waitMouseRelease();
    m.needRepaint = true;
}

// ---------------------------------------------------------------------------
// Изображение и выделение
// ---------------------------------------------------------------------------

bool Editor::askTwoNumbers(const std::string& title, const std::string& hint,
                           const std::string& label1, int& v1, int lo1, int hi1,
                           const std::string& label2, int& v2, int lo2, int hi2,
                           const std::string& okCaption, int* anchor) {
    std::vector<NumField> f = {{label1, &v1, lo1, hi1}, {label2, &v2, lo2, hi2}};
    return askNumbers(title, hint, f, okCaption, anchor);
}

bool Editor::askNumbers(const std::string& title, const std::string& hint,
                        const std::vector<NumField>& fields, const std::string& okCaption,
                        int* anchor) {
    for (const NumField& f : fields)
        *f.v = clampI(*f.v, f.lo, f.hi);
    bool done = false;
    bool ok = false;
    dropInput();
    pf_.input().keys.clear();
    auto stepRow = [&](int y, const NumField& f) {
        int& v = *f.v;
        uiLabel(40, y, 180, 34, f.label, cText);
        const int s1 = std::max(1, f.step);
        const int s8 = s1 * 8;
        const std::string n1 = std::to_string(s1);
        const std::string n8 = std::to_string(s8);
        if (uiButton(230, y, 46, 34, "−" + n8, v > f.lo)) v = clampI(v - s8, f.lo, f.hi);
        if (uiButton(282, y, 46, 34, "−" + n1, v > f.lo)) v = clampI(v - s1, f.lo, f.hi);
        fb_.fillRoundRect(334, y, 434, y + 34, 6, 6, cFace);
        uiLabel(334, y, 100, 34, std::to_string(v), cText);
        if (uiButton(440, y, 46, 34, "+" + n1, v < f.hi)) v = clampI(v + s1, f.lo, f.hi);
        if (uiButton(492, y, 46, 34, "+" + n8, v < f.hi)) v = clampI(v + s8, f.lo, f.hi);
        // Ноль — частая точка отсчёта у сдвигов: возврат к нему одним щелчком.
        if (f.lo < 0 && f.hi > 0 && uiButton(548, y, 46, 34, "0", v != 0)) v = 0;
    };
    while (!done && !quitNow_) {
        beginFrameInput();
        fb_.clear(cWindow);
        textCentered(0, 30, WIN_W, 66, title, cText);
        uiLabel(0, 70, WIN_W, 26, hint, cTextDim);
        int y = 120;
        for (const NumField& f : fields) {
            stepRow(y, f);
            y += 50;
        }
        if (anchor != nullptr) {
            // Якорь: куда прижать старую картинку на новом холсте.
            uiLabel(40, y + 10, 180, 30, "Картинка к", cText);
            static const char* kArrows[9] = {"↖", "↑", "↗", "←", "·", "→", "↙", "↓", "↘"};
            for (int k = 0; k < 9; ++k) {
                if (uiToggle(230 + (k % 3) * 40, y + 6 + (k / 3) * 38, 36, 34, kArrows[k],
                             *anchor == k))
                    *anchor = k;
            }
            y += 130;
        }
        const int by = y + 30;
        if (uiButton(WIN_W / 2 - 180, by, 170, 38, okCaption, true)) {
            ok = true;
            done = true;
        }
        if (uiButton(WIN_W / 2 + 10, by, 170, 38, "Отмена", true)) done = true;
        redraw();
        const int key = takeKey();
        if (key == KEY_ESC) done = true;
        if (key == KEY_ENTER) {
            ok = true;
            done = true;
        }
        idle();
    }
    waitMouseRelease();
    m.needRepaint = true;
    return ok;
}

/// Поворот на любой угол: угол, способ, рост холста.
void Editor::showRotateDialog() {
    int deg = rotateDeg_;
    bool done = false;
    bool ok = false;
    dropInput();
    pf_.input().keys.clear();
    const bool sel = m.hasSelection;
    while (!done && !quitNow_) {
        beginFrameInput();
        fb_.clear(cWindow);
        textCentered(0, 30, WIN_W, 66, "Поворот на любой угол", cText);
        uiLabel(0, 70, WIN_W, 26,
                std::string("По часовой стрелке, ") + (sel ? "выделенное" : "все слои и кадры") +
                    (rotateMethod_ == img::ROT_SMOOTH ? "" : "; новых цветов не появляется"),
                cTextDim);
        uiLabel(40, 120, 180, 34, "Угол, градусы", cText);
        if (uiButton(230, 120, 46, 34, "−15", deg > -180)) deg = std::max(-180, deg - 15);
        if (uiButton(282, 120, 46, 34, "−1", deg > -180)) --deg;
        fb_.fillRoundRect(334, 120, 434, 154, 6, 6, cFace);
        uiLabel(334, 120, 100, 34, std::to_string(deg) + "°", cText);
        if (uiButton(440, 120, 46, 34, "+1", deg < 180)) ++deg;
        if (uiButton(492, 120, 46, 34, "+15", deg < 180)) deg = std::min(180, deg + 15);
        if (uiButton(548, 120, 46, 34, "0", deg != 0)) deg = 0;
        uiLabel(40, 170, 180, 34, "Способ", cText);
        if (uiToggle(230, 170, 160, 34, "RotSprite", rotateMethod_ == img::ROT_SPRITE))
            rotateMethod_ = img::ROT_SPRITE;
        if (uiToggle(396, 170, 180, 34, "Ближайший сосед", rotateMethod_ == img::ROT_NEAREST))
            rotateMethod_ = img::ROT_NEAREST;
        if (uiToggle(582, 170, 160, 34, "Плавно", rotateMethod_ == img::ROT_SMOOTH))
            rotateMethod_ = img::ROT_SMOOTH;
        const char* how = "Ближайший сосед быстрее и честнее к точкам, но рвёт тонкие линии";
        if (rotateMethod_ == img::ROT_SPRITE)
            how = "RotSprite держит линии и контур целыми — для спрайтов";
        if (rotateMethod_ == img::ROT_SMOOTH)
            how = "Плавно — не пиксель-арт: билинейно, с новыми цветами и мягким краем";
        uiLabel(0, 210, WIN_W, 22, how, rotateMethod_ == img::ROT_SMOOTH ? cWarn : cTextDim);
        if (!sel) {
            uiLabel(40, 250, 180, 34, "Холст", cText);
            if (uiToggle(230, 250, 200, 34, "Вырастить", rotateExpand_)) rotateExpand_ = true;
            if (uiToggle(436, 250, 200, 34, "Оставить размер", !rotateExpand_))
                rotateExpand_ = false;
        }
        if (uiButton(WIN_W / 2 - 180, 320, 170, 38, "Повернуть", deg != 0)) {
            ok = true;
            done = true;
        }
        if (uiButton(WIN_W / 2 + 10, 320, 170, 38, "Отмена", true)) done = true;
        redraw();
        const int key = takeKey();
        if (key == KEY_ESC) done = true;
        if (key == KEY_ENTER && deg != 0) {
            ok = true;
            done = true;
        }
        idle();
    }
    waitMouseRelease();
    m.needRepaint = true;
    rotateDeg_ = deg;
    if (!ok) return;
    if (!m.rotateImageBy(deg, rotateMethod_, rotateExpand_)) {
        showMessage("Не помещается",
                    "Повёрнутый тайл больше " + std::to_string(MAX_TILE) +
                        " точек по стороне. "
                        "Поверните с прежним размером холста или уменьшите картинку.");
    }
}

/// Контур первым цветом: снаружи или внутри, толщина, соседи.
void Editor::showOutlineDialog() {
    bool done = false;
    bool ok = false;
    dropInput();
    pf_.input().keys.clear();
    while (!done && !quitNow_) {
        beginFrameInput();
        fb_.clear(cWindow);
        textCentered(0, 30, WIN_W, 66, "Контур первым цветом", cText);
        uiLabel(0, 70, WIN_W, 26,
                std::string("По точкам, без сглаживания; ") +
                    (m.hasSelection ? "в пределах выделения" : "по всему тайлу"),
                cTextDim);
        uiLabel(40, 120, 180, 34, "Где", cText);
        if (uiToggle(230, 120, 200, 34, "Снаружи", outlineOutside_)) outlineOutside_ = true;
        if (uiToggle(436, 120, 200, 34, "Внутри", !outlineOutside_)) outlineOutside_ = false;
        uiLabel(40, 170, 180, 34, "Соседи", cText);
        if (uiToggle(230, 170, 200, 34, "Четыре — углы открыты", !outlineEight_))
            outlineEight_ = false;
        if (uiToggle(436, 170, 200, 34, "Восемь — сплошной", outlineEight_)) outlineEight_ = true;
        uiLabel(40, 220, 180, 34, "Толщина", cText);
        if (uiButton(282, 220, 46, 34, "−1", outlineThick_ > 1)) --outlineThick_;
        fb_.fillRoundRect(334, 220, 434, 254, 6, 6, cFace);
        uiLabel(334, 220, 100, 34, std::to_string(outlineThick_), cText);
        if (uiButton(440, 220, 46, 34, "+1", outlineThick_ < 16)) ++outlineThick_;
        uiLabel(40, 270, 180, 34, "Цвет", cText);
        fb_.fillRect(230, 272, 290, 302, blendOver(m.firstColor, cCheckA));
        fb_.frameRect(230, 272, 290, 302, cBorder);
        uiLabelLeft(300, 270, 34, "первый рабочий", cTextDim);
        if (uiButton(WIN_W / 2 - 180, 340, 170, 38, "Обвести", true)) {
            ok = true;
            done = true;
        }
        if (uiButton(WIN_W / 2 + 10, 340, 170, 38, "Отмена", true)) done = true;
        redraw();
        const int key = takeKey();
        if (key == KEY_ESC) done = true;
        if (key == KEY_ENTER) {
            ok = true;
            done = true;
        }
        idle();
    }
    waitMouseRelease();
    m.needRepaint = true;
    if (ok) m.outlineImage(m.firstColor, outlineEight_, outlineOutside_, outlineThick_);
}

void Editor::showImageScreen() {
    bool done = false;
    dropInput();
    pf_.input().keys.clear();
    constexpr int CW = 228;
    constexpr int BH = 28;
    constexpr int ST = 31;
    const int c1 = 30;
    const int c2 = c1 + CW + 18;
    const int c3 = c2 + CW + 18;

    while (!done && !quitNow_) {
        beginFrameInput();
        fb_.clear(cWindow);
        textCentered(0, 8, WIN_W, 34, "Изображение и выделение", cText);
        const bool tile = m.docMode == DOC_TILE;
        const bool sel = tile && m.hasSelection;
        const std::string what = sel ? "выделенного" : "всего тайла";
        uiLabel(0, 32, WIN_W, 20,
                std::to_string(m.tile.w) + "×" + std::to_string(m.tile.h) +
                    (sel ? "   ·   выделено " + std::to_string(img::maskCount(m.selectionMask())) +
                               " точек"
                         : "   ·   выделения нет"),
                cTextDim);
        // Каждая кнопка делает своё и закрывает экран: результат виден на
        // холсте, а не здесь.
        auto act = [&](int x, int y, const std::string& caption, bool enabled) {
            const bool hit = uiButton(x, y, CW, BH, caption, enabled);
            if (hit) done = true;
            return hit;
        };

        // --- выделение ---
        int y = 78;
        uiLabelLeft(c1, y - 24, 20, "Выделение", cTextDim);
        if (act(c1, y, "Выделить всё   Ctrl+A", tile)) m.selectAll();
        y += ST;
        if (act(c1, y, "Снять   Ctrl+D", sel)) m.deselect();
        y += ST;
        if (act(c1, y, "Обратить   Ctrl+I", tile)) m.invertSelection();
        y += ST;
        if (act(c1, y, "Вернуть прежнее   Ctrl+Shift+D", tile && !m.lastSelMask.empty()))
            m.reselect();
        y += ST;
        if (act(c1, y, "Всё непрозрачное", tile)) m.selectOpaque(img::SEL_REPLACE);
        y += ST;
        if (act(c1, y, "По первому цвету", tile)) {
            for (int k = 0; k < m.tile.w * m.tile.h; ++k) {
                if (sameColor(m.tile.d[static_cast<std::size_t>(k)], m.firstColor)) {
                    m.selectByColorAt(k % m.tile.w, k / m.tile.w, false, img::SEL_REPLACE);
                    break;
                }
            }
        }
        y += ST;
        if (act(c1, y, "Расширить на точку", sel)) m.growSelection(1);
        y += ST;
        if (act(c1, y, "Сузить на точку", sel)) m.growSelection(-1);
        y += ST;
        if (act(c1, y, "Копировать   Ctrl+C", sel)) m.copySelection();
        y += ST;
        if (act(c1, y, "Вырезать   Ctrl+X", sel)) m.cutSelection();
        y += ST;
        if (act(c1, y, "Стереть выделенное   Delete", sel)) m.eraseSelection();

        // --- отражение, поворот, масштаб ---
        y = 78;
        uiLabelLeft(c2, y - 24, 20, "Отражение и поворот " + what, cTextDim);
        if (act(c2, y, "Отразить слева направо", tile)) m.transformImage(TF_FLIP_H);
        y += ST;
        if (act(c2, y, "Отразить сверху вниз", tile)) m.transformImage(TF_FLIP_V);
        y += ST;
        if (act(c2, y, "Повернуть на 90° по часовой", tile)) m.transformImage(TF_ROT90);
        y += ST;
        if (act(c2, y, "Повернуть на 180°", tile)) m.transformImage(TF_ROT180);
        y += ST;
        if (act(c2, y, "Повернуть на 90° против", tile)) m.transformImage(TF_ROT270);
        y += ST;
        if (act(c2, y, "Повернуть на любой угол…", tile)) showRotateDialog();
        y += ST;
        const int sw = sel ? m.selW : m.tile.w;
        const int sh = sel ? m.selH : m.tile.h;
        for (int f = 2; f <= 4; ++f) {
            if (act(c2, y, "Увеличить в " + std::to_string(f) + " раза",
                    tile && sw * f <= MAX_TILE && sh * f <= MAX_TILE))
                m.scaleBy(f, 1);
            y += ST;
        }
        if (act(c2, y, "Уменьшить вдвое",
                tile && sw >= 2 && sh >= 2 && (sel || (sw / 2 >= MIN_TILE && sh / 2 >= MIN_TILE))))
            m.scaleBy(1, 2);
        y += ST;
        if (act(c2, y, "Контур первым цветом…", tile)) showOutlineDialog();
        y += ST;
        // Была только клавиша R: промахнуться по ней легко, а заливает она
        // весь тайл (UX-17).
        if (act(c2, y, "Случайные цвета на весь тайл", tile)) m.randomizeTile();

        // --- холст ---
        y = 78;
        uiLabelLeft(c3, y - 24, 20, "Холст", cTextDim);
        if (act(c3, y, "Обрезать по выделению", sel)) m.cropToSelection();
        y += ST;
        if (act(c3, y, "Обрезать по содержимому", tile)) m.cropToContent();
        y += ST;
        if (act(c3, y, "Размер холста…", tile)) {
            int nw = m.tile.w;
            int nh = m.tile.h;
            int anchor = 4;
            if (askTwoNumbers("Размер холста",
                              "Картинка не масштабируется: холст растёт или обрезается", "Ширина",
                              nw, MIN_TILE, MAX_TILE, "Высота", nh, MIN_TILE, MAX_TILE, "Применить",
                              &anchor))
                m.resizeCanvas(nw, nh, anchor % 3, anchor / 3);
        }
        y += ST;
        if (act(c3, y, "Масштаб до размера…", tile)) {
            int nw = m.tile.w;
            int nh = m.tile.h;
            if (askTwoNumbers("Масштаб до размера", "Ближайшим соседом: точки не размываются",
                              "Ширина", nw, 1, MAX_TILE, "Высота", nh, 1, MAX_TILE, "Применить",
                              nullptr))
                m.scaleImageTo(nw, nh);
        }
        y += ST;
        if (act(c3, y, "Сдвиг на полтайла", tile)) m.offsetImage(m.tile.w / 2, m.tile.h / 2);
        y += ST;
        if (act(c3, y, "Сдвиг с переносом…", tile)) {
            int dx = 0;
            int dy = 0;
            if (askTwoNumbers("Сдвиг с переносом",
                              "Ушедшее за край выходит с другой стороны — так видно шов",
                              "По горизонтали", dx, -m.tile.w + 1, m.tile.w - 1, "По вертикали", dy,
                              -m.tile.h + 1, m.tile.h - 1, "Сдвинуть", nullptr))
                m.offsetImage(dx, dy);
        }
        y += ST + 8;
        uiLabelLeft(c3, y - 6, 20, sel ? "Залить выделенное" : "Залить весь тайл", cTextDim);
        y += 16;
        if (act(c3, y, "Первым цветом", tile)) m.fillSelectionOrAll(m.firstColor);
        y += ST;
        if (act(c3, y, "Вторым цветом", tile)) m.fillSelectionOrAll(m.secondColor);
        y += ST;
        if (act(c3, y, "Прозрачным", tile)) m.fillSelectionOrAll(argb(0, 0, 0, 0));
        y += ST;
        if (act(c3, y, "Убрать одиночные точки", tile)) m.removeIsolatedPixels();
        y += ST;
        if (act(c3, y, "Тон, насыщенность, яркость…", tile)) {
            int dh = 0;
            int ds = 0;
            int dv = 0;
            if (askNumbers("Тон, насыщенность, яркость",
                           std::string("Для точек ") + (sel ? "выделения" : "всего тайла") +
                               (m.strictIndexedOn() ? "; итог сведётся к палитре" : ""),
                           {{"Тон, градусы", &dh, -180, 180},
                            {"Насыщенность, %", &ds, -100, 100},
                            {"Яркость, %", &dv, -100, 100}},
                           "Применить", nullptr))
                m.adjustImageHSV(dh, ds, dv);
        }

        // --- край и цвета ---
        if (act(c1, 447, "Нарастить край", tile)) m.growImageEdge(outlineEight_);
        if (act(c1, 478, "Срезать край", tile)) m.shrinkImageEdge(outlineEight_);
        if (act(c2, 478, "Фильтры и эффекты…   Shift+K", tile)) showFiltersScreen();
        if (act(c3, 447, "Плавный масштаб…", tile)) {
            int nw = m.tile.w;
            int nh = m.tile.h;
            if (askTwoNumbers("Плавный масштаб",
                              "Не пиксель-арт: усреднение и билинейная выборка дают новые цвета",
                              "Ширина", nw, 1, MAX_TILE, "Высота", nh, 1, MAX_TILE, "Применить",
                              nullptr))
                m.scaleImageTo(nw, nh, true);
        }
        if (act(c2, 447, "Сократить цвета…", tile)) {
            int n = 16;
            if (askNumbers("Сократить цвета",
                           std::string("Медианный разрез цветов ") +
                               (sel ? "выделения" : "всего тайла") +
                               "; прозрачность точек остаётся",
                           {{"Цветов", &n, 1, 256}}, "Применить", nullptr))
                m.reduceImageColors(n);
        }
        if (uiButton(c1, WIN_H - 50, CW / 2 - 3, 34, "Цвет…", tile)) showColorOpsScreen();
        if (uiButton(c1 + CW / 2 + 3, WIN_H - 50, CW / 2 - 3, 34, "Палитра…", tile))
            showPaletteScreen();
        if (uiButton(c2, WIN_H - 50, CW, 34, "Параметры инструментов…", true)) showToolOptions();
        if (uiButton(c3, WIN_H - 50, CW, 34, "Закрыть", true)) done = true;
        redraw();
        const int key = takeKey();
        if (key == KEY_ESC || key == KEY_K) done = true;
        idle();
    }
    waitMouseRelease();
    m.needRepaint = true;
}

}  // namespace tjtm
