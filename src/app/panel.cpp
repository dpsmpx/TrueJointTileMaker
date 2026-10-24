// Панель справа (TJTM.pas: «Панель»). Разбита на секции: при протяжке
// ползунка перерисовываются только полосы цвета, а не всё окно.

#include <algorithm>
#include <cstdio>

#include "app/editor.h"
#include "core/pmath.h"
#include "core/theme.h"

namespace tjtm {

using namespace theme;

namespace {

Grid<Color> gridFromBgra(const std::vector<std::uint8_t>& raw, int w) {
    if (w <= 0) return {};
    const int h = static_cast<int>(raw.size() / 4) / w;
    Grid<Color> g(w, h);
    for (std::size_t i = 0; i < g.d.size(); ++i)
        g.d[i] = Color(raw[i * 4 + 3], raw[i * 4 + 2], raw[i * 4 + 1], raw[i * 4]);
    return g;
}

}  // namespace

/// Квадрат насыщенности пересобирается только при смене тона.
void Editor::ensureHSVBitmap() {
    if (cachedHue_ == m.hue) return;
    cachedHue_ = m.hue;
    std::vector<std::uint8_t> raw(static_cast<std::size_t>(BAR_SIZE) * BAR_SIZE * 4);
    renderHSVPlane(raw, BAR_SIZE, m.hue);
    bmpHSV_ = gridFromBgra(raw, BAR_SIZE);
}

void Editor::initColorBars() {
    std::vector<std::uint8_t> raw(static_cast<std::size_t>(BAR_SIZE) * BAR_H * 4);
    renderHuePlane(raw, BAR_SIZE);
    bmpHue_ = gridFromBgra(raw, BAR_SIZE);
    renderAlphaPlane(raw, BAR_SIZE);
    bmpAlpha_ = gridFromBgra(raw, BAR_SIZE);
    cachedHue_ = -1;
}

std::string Editor::toolLabel(int i) const {
    if (m.docMode == DOC_MAP) {
        switch (i) {
            case M_PAINT: return "Мест";
            case M_FILL: return "Зал";
            case M_RECT: return "Прям";
            case M_OBJ: return "Объ";
            default: return "Выд";
        }
    }
    switch (i) {
        case T_PEN: return "Кар";
        case T_ERASER: return "Ласт";
        case T_FILL: return "Зал";
        case T_PICK: return "Пип";
        case T_LINE: return "Лин";
        case T_RECT: return "Прям";
        case T_ELLIPSE: return "Овал";
        case T_DITHER: return "Диз";
        default: return "Выд";
    }
}

void Editor::drawToolsSection() {
    fb_.fillRect(PANEL_X, 0, PANEL_X + PANEL_W, TOOLS_Y1 + 2, cPanel);
    for (int i = 0; i < m.toolCount(); ++i) {
        const int x0 = Model::toolCellX(i);
        const int y0 = Model::toolCellY(i);
        fb_.fillRoundRect(x0, y0, x0 + TOOL_CW - 2, y0 + TOOL_CH - 2, 5, 5, cBorder);
        Color face = cBack;
        if (m.curTool() == i) {
            face = cAccent;
        } else if (mouseX() >= x0 && mouseX() < x0 + TOOL_CW - 2 && mouseY() >= y0 &&
                   mouseY() < y0 + TOOL_CH - 2) {
            face = cHover;
        }
        fb_.fillRoundRect(x0 + 1, y0 + 1, x0 + TOOL_CW - 3, y0 + TOOL_CH - 3, 5, 5, face);
        // Нарисованный значок заменяет подпись, ненарисованный — нет: набор
        // делается по одному, и пока значка нет, кнопка выглядит как раньше.
        const int ic = m.docMode == DOC_MAP ? T_TILE_COUNT + i : i;
        if (ic >= 0 && ic < static_cast<int>(iconBmp_.size()) &&
            !iconBmp_[static_cast<std::size_t>(ic)].empty()) {
            fb_.drawImage(iconBmp_[static_cast<std::size_t>(ic)], x0 + (TOOL_CW - 2 - ICON_PX) / 2,
                          y0 + (TOOL_CH - 2 - ICON_PX) / 2);
        } else {
            textCentered(x0, y0, x0 + TOOL_CW - 2, y0 + TOOL_CH - 2, toolLabel(i),
                         m.curTool() == i ? rgb(255, 255, 255) : cText);
        }
    }
    drawToolChips();
}

void Editor::drawPaletteSection() {
    fb_.fillRect(PANEL_X, SLOT1_Y0 - 2, PANEL_X + PANEL_W, SLOT1_Y1 + 2, cPanel);
    // Палитра любой длины: на панели окно 8x8 с прокруткой колесом.
    // Пустые клетки за концом палитры — фон панели: щелчок по ним добавляет
    // первый цвет.
    for (int i = 0; i < PAL_ROWS; ++i) {
        for (int j = 0; j < PAL_COLS; ++j) {
            const int k = m.palCellIndex(j, i);
            const int x0 = Model::palCellX(j);
            const int y0 = Model::palCellY(i);
            if (k < 0) {
                fb_.fillRect(x0, y0, x0 + PAL_CELL, y0 + PAL_CELL, cFace);
                continue;
            }
            const Color c = m.palette[static_cast<std::size_t>(k)];
            fb_.fillRect(x0, y0, x0 + PAL_CELL, y0 + PAL_CELL, blendOver(c, m.checkerColor(j, i)));
            // Прозрачный номер: шахматка с уголком его цвета — видно и что
            // он прозрачен, и какой цвет за ним записан.
            if (k == m.transparentIdx) {
                for (int q = 0; q < PAL_CELL; q += 4) {
                    for (int r = 0; r < PAL_CELL; r += 4)
                        fb_.fillRect(x0 + q, y0 + r, x0 + q + 4, y0 + r + 4,
                                     ((q + r) / 4) % 2 == 0 ? cCheckA : cCheckB);
                }
                fb_.fillRect(x0, y0, x0 + 6, y0 + 6, argb(255, c.r, c.g, c.b));
            }
        }
    }
    if (m.palMaxScroll() > 0) {
        // Полоса прокрутки справа от палитры: где окно и сколько всего.
        const int total = (m.palCount() + PAL_COLS - 1) / PAL_COLS;
        const int x0 = PAL_X0 + PAL_COLS * PAL_CELL + 3;
        const int h = PAL_ROWS * PAL_CELL;
        fb_.fillRect(x0, PAL_Y0, x0 + 3, PAL_Y0 + h, cLine);
        const int t0 = PAL_Y0 + h * m.palScroll / total;
        const int t1 = PAL_Y0 + h * (m.palScroll + PAL_ROWS) / total;
        fb_.fillRect(x0, t0, x0 + 3, t1, cTextDim);
    }
    // Рамка цветом выделения: индексный режим включён, и правка ячейки
    // перекрасит тайл — об этом надо знать заранее.
    const Color edge = (m.indexedOn && m.docMode == DOC_TILE) ? cAccent : cBorder;
    fb_.frameRect(PAL_X0 - 1, PAL_Y0 - 1, PAL_X0 + PAL_COLS * PAL_CELL + 1,
                  PAL_Y0 + PAL_ROWS * PAL_CELL + 1, edge);
    // Ячейка, из которой взят первый цвет: она поедет за кистью при правке.
    const int fRow = m.firstIdx / PAL_COLS - m.palScroll;
    if (m.firstIdx != IDX_NONE && m.firstIdx < m.palCount() && fRow >= 0 && fRow < PAL_ROWS &&
        m.docMode == DOC_TILE) {
        const int fj = m.firstIdx % PAL_COLS;
        const int fi = fRow;
        fb_.frameRect(Model::palCellX(fj), Model::palCellY(fi), Model::palCellX(fj + 1),
                      Model::palCellY(fi + 1), cAccent);
    }
}

void Editor::drawColorSection() {
    fb_.fillRect(PANEL_X, SLOT2_Y0 - 2, PANEL_X + PANEL_W, ALPHA_Y1 + 2, cPanel);
    ensureHSVBitmap();
    fb_.drawImage(bmpHSV_, HSV_X0, HSV_Y0);
    fb_.drawImage(bmpHue_, HSV_X0, HUE_Y0);
    fb_.drawImage(bmpAlpha_, HSV_X0, ALPHA_Y0);

    int mx = HSV_X0 + percentToAxis(m.sat, BAR_SIZE);
    const int my = HSV_Y0 + valueToAxis(m.val, BAR_SIZE);
    fb_.fillRect(mx - 1, HSV_Y0, mx + 1, HSV_Y1, argb(200, 0, 0, 0));
    fb_.fillRect(HSV_X0, my - 1, HSV_X0 + BAR_SIZE, my + 1, argb(200, 0, 0, 0));

    mx = HSV_X0 + hueToAxis(m.hue, BAR_SIZE);
    fb_.fillRect(mx - 1, HUE_Y0, mx + 1, HUE_Y1, argb(200, 0, 0, 0));

    mx = HSV_X0 + byteToAxis(m.firstColor.a, BAR_SIZE);
    fb_.fillRect(mx - 1, ALPHA_Y0, mx + 1, ALPHA_Y1, argb(200, 220, 40, 40));

    // Оба рабочих цвета: первым рисуют, второй участвует в дизеринге и обмене.
    fb_.fillRect(PANEL_X + 4, SLOT2_Y0, PANEL_X + 34, SLOT2_Y0 + 30,
                 blendOver(m.firstColor, cCheckA));
    fb_.frameRect(PANEL_X + 4, SLOT2_Y0, PANEL_X + 34, SLOT2_Y0 + 30, cBorder);
    fb_.fillRect(PANEL_X + 4, SLOT2_Y0 + 34, PANEL_X + 34, SLOT2_Y0 + 64,
                 blendOver(m.secondColor, cCheckA));
    fb_.frameRect(PANEL_X + 4, SLOT2_Y0 + 34, PANEL_X + 34, SLOT2_Y0 + 64, cBorder);
}

void Editor::drawTerrainSection() {
    fb_.fillRect(PANEL_X, SLOT1_Y0 - 2, PANEL_X + PANEL_W, SLOT1_Y1 + 2, cPanel);
    for (int i = 0; i < MT_COUNT; ++i) {
        const int x0 = Model::mselX(i);
        const int y0 = Model::terrCellY(i);
        fb_.fillRect(x0, y0, x0 + MSEL_CW - 2, y0 + MTERR_CH - 2, mapTileColor(i));
        fb_.frameRect(x0, y0, x0 + MSEL_CW - 2, y0 + MTERR_CH - 2,
                      m.mapBrush == i ? cAccent : cBorder);
        // Подпись светлая на тёмной местности и наоборот.
        const bool dark = i == MT_WALL || i == MT_TREE || i == MT_WATER || i == MT_DEADWATER;
        textCentered(x0, y0, x0 + MSEL_CW - 2, y0 + MTERR_CH - 2,
                     std::string(1, mapTileChar(i)) + " " + mapTileName(i),
                     dark ? rgb(245, 245, 245) : rgb(20, 20, 24));
    }
}

void Editor::drawObjectKindSection() {
    fb_.fillRect(PANEL_X, SLOT2_Y0 - 2, PANEL_X + PANEL_W, ALPHA_Y1 + 2, cPanel);
    for (int i = 0; i < mapKindCount(); ++i) {
        const int x0 = Model::mselX(i);
        const int y0 = Model::objCellY(i);
        fb_.fillRoundRect(x0, y0, x0 + MSEL_CW - 2, y0 + MOBJ_CH - 2, 4, 4, cBorder);
        fb_.fillRoundRect(x0 + 1, y0 + 1, x0 + MSEL_CW - 3, y0 + MOBJ_CH - 3, 4, 4,
                          m.mapObjKind == i ? cAccent : cBack);
        textCentered(x0, y0, x0 + MSEL_CW - 2, y0 + MOBJ_CH - 2,
                     mapObjGlyph(mapKindName(i)) + " " + mapKindName(i),
                     m.mapObjKind == i ? rgb(255, 255, 255) : cText);
    }
}

/// Картинка обзора: пересобирается только когда документ изменился; при
/// панорамировании двигается лишь рамка видимой области.
void Editor::buildOverview() {
    const int pw = m.overPixW();
    if (pw < 1 || m.overPixH() < 1 || m.docW() < 1 || m.docH() < 1) return;
    if (!m.overDirty && !bmpOver_.empty() && overCacheW_ == m.docW() && overCacheH_ == m.docH() &&
        overCacheMode_ == m.docMode) {
        return;
    }
    m.overDirty = false;
    overCacheW_ = m.docW();
    overCacheH_ = m.docH();
    overCacheMode_ = m.docMode;
    bmpOver_ = gridFromBgra(m.buildOverviewRaw(), pw);
}

void Editor::drawOverviewSection() {
    fb_.fillRect(PANEL_X, OVER_Y0 - 2, PANEL_X + PANEL_W, OVER_Y1 + 2, cPanel);
    buildOverview();
    const int ox = m.overOriginX();
    const int oy = m.overOriginY();
    const int pw = m.overPixW();
    const int ph = m.overPixH();
    fb_.drawImage(bmpOver_, ox, oy);
    fb_.frameRect(ox - 1, oy - 1, ox + pw + 1, oy + ph + 1, cBorder);
    // Рамка видимой области: панорама холста и обзор всегда смотрят на одно
    // и то же место документа.
    int vx0 = 0;
    int vy0 = 0;
    int vx1 = 0;
    int vy1 = 0;
    if (m.overviewViewport(vx0, vy0, vx1, vy1))
        fb_.frameRect(vx0, vy0, vx1, vy1, rgb(250, 240, 80));
}

/// Сетка цветов, вписанная в квадрат: перебор по точкам приёмника, поэтому
/// стоимость не зависит от размера исходника.
void Editor::drawGridThumb(const Grid<Color>& g, int x0, int y0, int size) {
    if (g.empty() || size < 1) return;
    for (int py = 0; py < size; ++py) {
        for (int px = 0; px < size; ++px) {
            const int sx = clampI(px * g.w / size, 0, g.w - 1);
            const int sy = clampI(py * g.h / size, 0, g.h - 1);
            fb_.fillRect(x0 + px, y0 + py, x0 + px + 1, y0 + py + 1,
                         blendOver(g.at(sx, sy), m.checkerColor(sx, sy)));
        }
    }
}

void Editor::drawInfoSection() {
    // Фон — между разделителями, не задевая их: в оригинале заливка от
    // INFO_Y0 - 2 до INFO_Y1 + 2 стирала оба, верхний не был виден вовсе.
    fb_.fillRect(PANEL_X, INFO_Y0 - 1, PANEL_X + PANEL_W, INFO_Y1 + 1, cPanel);
    if (m.docMode == DOC_TILE) {
        fb_.fillRect(PANEL_X + 2, INFO_Y0, PANEL_X + 42, INFO_Y0 + 40, cFace);
        drawGridThumb(m.buffer, PANEL_X + 2, INFO_Y0, 40);
        fb_.frameRect(PANEL_X + 2, INFO_Y0, PANEL_X + 42, INFO_Y0 + 40, cBorder);
    }
    const int tx = m.screenToTileX(mouseX());
    const int ty = m.screenToTileY(mouseY());
    std::string s;
    std::string alpha;
    if (mouseX() < m.W && m.inDoc(tx, ty)) {
        s = std::to_string(tx) + " : " + std::to_string(ty);
        if (m.docMode == DOC_MAP) {
            s += "  " + mapTileName(m.map.at(tx, ty));
        } else {
            // Цвет под курсором: пипеткой его не узнать, не потеряв
            // рабочий, а знать его нужно часто.
            const Color c = m.tile.at(tx, ty);
            char hex[16];
            std::snprintf(hex, sizeof hex, "  #%02X%02X%02X", c.r, c.g, c.b);
            s += hex;
            if (c.a < 255) alpha = "  α" + std::to_string(c.a);
        }
    } else {
        s = "—";
    }
    s += "\n" + std::to_string(m.docW()) + "x" + std::to_string(m.docH()) + "  " +
         std::to_string(m.pixelSize) + "x" + alpha;
    if (m.docMode == DOC_MAP) s += "  объектов " + std::to_string(m.map.objects.size());
    uiLabel(PANEL_X + 46, INFO_Y0, PANEL_W - 48, INFO_Y1 - INFO_Y0, s, cText);
}

void Editor::drawMenuButtonSection() {
    fb_.fillRect(PANEL_X, MENU_Y0 - 2, PANEL_X + PANEL_W, WIN_H, cPanel);
    std::string caption = m.docMode == DOC_MAP ? "Меню — карта" : "Меню — тайл";
    if (m.isModified()) caption += " *";
    uiButton(PANEL_X + 4, MENU_Y0, PANEL_W - 8, MENU_Y1 - MENU_Y0, caption, true);
}

/// Тонкая линия во всю ширину панели: инструменты, цвет, обзор и сведения
/// иначе шли встык, и панель читалась одним столбцом.
void Editor::panelDivider(int y) {
    fb_.fillRect(PANEL_X + 6, y, PANEL_X + PANEL_W - 6, y + 1, cLine);
}

void Editor::drawPanel() {
    fb_.fillRect(PANEL_X, 0, PANEL_X + PANEL_W, WIN_H, cPanel);
    // Граница панели: на светлой теме разница тонов мала, и край терялся.
    fb_.fillRect(PANEL_X, 0, PANEL_X + 1, WIN_H, cBorder);
    drawToolsSection();
    panelDivider(TOOLS_Y1 + 2);
    if (m.docMode == DOC_MAP) {
        drawTerrainSection();
        drawObjectKindSection();
    } else {
        drawPaletteSection();
        drawColorSection();
    }
    panelDivider(OVER_Y0 - 3);
    drawOverviewSection();
    panelDivider(INFO_Y0 - 2);
    drawInfoSection();
    panelDivider(MENU_Y0 - 3);
    drawMenuButtonSection();
}

void Editor::repaint() {
    // Команда могла положить цвет не из палитры: в строгом индексном режиме
    // он сводится к ячейке до того, как его увидят.
    m.enforceIndexed();
    drawCanvas();
    drawTimeline();
    drawPanel();
    redraw();
    m.needRepaint = false;
}

/// Только холст: при протяжке панель не меняется, а её обзор и миниатюры —
/// самая дорогая часть кадра.
void Editor::repaintCanvasOnly() {
    drawCanvas();
    redraw();
}

/// Холст вместе с обзором: рамка видимой области следует за панорамой.
void Editor::repaintCanvasAndOverview() {
    drawCanvas();
    drawOverviewSection();
    redraw();
}

void Editor::repaintColorOnly() {
    drawColorSection();
    redraw();
}

}  // namespace tjtm
