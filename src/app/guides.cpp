// Линейки, направляющие, крупная сетка и начало координат (этап 5
// RoadToProLevel, раздел 20 плана).
//
// Линейка показывает координаты от начала, которое можно сдвинуть: тайл
// 16x16 в листе удобнее мерить от его угла, а не от угла листа. Метки на
// линейке — направляющие её оси: верхняя линейка держит вертикальные,
// левая — горизонтальные. Щелчок по линейке ставит направляющую, протяжка
// двигает, правая кнопка или вынос за документ убирают. Числа пишутся
// крошечным шрифтом 3x5: обычный в полосу шириной 20 точек не влезает.

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <numbers>

#include "app/editor.h"
#include "core/pmath.h"
#include "core/theme.h"

namespace tjtm {

using namespace theme;

namespace {

constexpr int RULER = 20;

/// Цифры 3x5: строка — три бита, старший слева.
constexpr std::uint8_t kDigits[11][5] = {
    {7, 5, 5, 5, 7}, {2, 6, 2, 2, 7}, {7, 1, 7, 4, 7}, {7, 1, 3, 1, 7},
    {5, 5, 7, 1, 1}, {7, 4, 7, 1, 7}, {7, 4, 7, 5, 7}, {7, 1, 2, 2, 2},
    {7, 5, 7, 5, 7}, {7, 5, 7, 1, 7}, {0, 0, 7, 0, 0},  // минус
};

void tinyNumber(gfx::Surface& fb, int x, int y, int v, Color c) {
    const std::string s = std::to_string(v);
    for (const char ch : s) {
        const int d = ch == '-' ? 10 : ch - '0';
        if (d < 0 || d > 10) continue;
        for (int row = 0; row < 5; ++row) {
            for (int col = 0; col < 3; ++col) {
                if (((kDigits[d][row] >> (2 - col)) & 1) != 0)
                    fb.fillRect(x + col, y + row, x + col + 1, y + row + 1, c);
            }
        }
        x += 4;
    }
}

/// Шаг делений: наименьшая степень двойки, у которой деление не уже
/// minPx точек экрана. Степени двойки — потому что тайлы 8, 16, 32.
int rulerStep(double scale, int minPx) {
    int s = 1;
    while (s * scale < minPx && s < (1 << 20))
        s *= 2;
    return s;
}

}  // namespace

// ---------------------------------------------------------------------------
// Рисование
// ---------------------------------------------------------------------------

void Editor::drawGuides() {
    const int ps = m.cellPx();
    const int ox = m.tileToScreenX(0);
    const int oy = m.tileToScreenY(0);
    const int dw = m.docW();
    const int dh = m.docH();
    if (dw <= 0 || dh <= 0) return;
    const int ex = ox + m.scr(dw);
    const int ey = oy + m.scr(dh);
    fb_.setClip(canvasRect());

    // Крупная сетка — по границам точек, поверх мелкой.
    if (m.majorGridW > 0) {
        for (int k = m.majorGridW; k < dw; k += m.majorGridW) {
            const int sx = ox + m.scr(k) - (ps >= 2 ? 1 : 0);
            fb_.fillRect(sx, oy, sx + 1, ey, active.majorGrid);
        }
    }
    if (m.majorGridH > 0) {
        for (int k = m.majorGridH; k < dh; k += m.majorGridH) {
            const int sy = oy + m.scr(k) - (ps >= 2 ? 1 : 0);
            fb_.fillRect(ox, sy, ex, sy + 1, active.majorGrid);
        }
    }
    if (!m.guidesOn) {
        fb_.resetClip();
        return;
    }
    auto dottedV = [&](int sx, Color c, int on, int off) {
        for (int y = oy; y < ey; y += on + off)
            fb_.fillRect(sx, y, sx + 1, std::min(ey, y + on), c);
    };
    auto dottedH = [&](int sy, Color c, int on, int off) {
        for (int x = ox; x < ex; x += on + off)
            fb_.fillRect(x, sy, std::min(ex, x + on), sy + 1, c);
    };
    // Середины клеток крупной сетки: пунктир, чтобы не спорить с границами.
    if (m.guideCellCenters) {
        if (m.majorGridW > 0) {
            for (int k = 0; (2 * k + 1) * m.majorGridW <= 2 * dw; ++k)
                dottedV(ox + m.scr2((2 * k + 1) * m.majorGridW), active.cellCenter, 2, 3);
        }
        if (m.majorGridH > 0) {
            for (int k = 0; (2 * k + 1) * m.majorGridH <= 2 * dh; ++k)
                dottedH(oy + m.scr2((2 * k + 1) * m.majorGridH), active.cellCenter, 2, 3);
        }
    }
    if (m.guideCenter) {
        dottedV(ox + m.scr2(dw), active.guideCenter, 5, 3);
        dottedH(oy + m.scr2(dh), active.guideCenter, 5, 3);
    }
    // Свои направляющие идут через весь холст, как в редакторах растра:
    // по ним выравнивают и то, что за краем документа в бесшовном виде.
    for (const int x2 : m.guidesX2) {
        const int sx = ox + m.scr2(x2);
        fb_.fillRect(sx, m.Y0, sx + 1, m.H, active.guide);
    }
    for (const int y2 : m.guidesY2) {
        const int sy = oy + m.scr2(y2);
        fb_.fillRect(m.X0, sy, m.W, sy + 1, active.guide);
    }
    // Начало координат, сдвинутое с угла документа, — крестиком.
    if (m.originX != 0 || m.originY != 0) {
        const int cx = ox + m.scr(m.originX);
        const int cy = oy + m.scr(m.originY);
        fb_.fillRect(cx - 6, cy, cx + 7, cy + 1, active.guideCenter);
        fb_.fillRect(cx, cy - 6, cx + 1, cy + 7, active.guideCenter);
    }
    fb_.resetClip();
}

void Editor::drawRadialAxes() {
    if (m.symRadial < 2) return;
    // Лучи из точки пересечения осей: видно, на сколько секторов делится
    // рисунок. Первый луч смотрит вверх.
    const double ps = m.scale();
    const double cx = m.tileToScreenX(0) + m.axisX2() * ps / 2.0;
    const double cy = m.tileToScreenY(0) + m.axisY2() * ps / 2.0;
    const double r = std::max(m.docW(), m.docH()) * ps;
    const Color c = active.radialAxis;
    fb_.setClip(canvasRect());
    for (int k = 0; k < m.symRadial; ++k) {
        const double a = -std::numbers::pi / 2 + 2 * std::numbers::pi * k / m.symRadial;
        fb_.line(pround(cx), pround(cy), pround(cx + r * std::cos(a)), pround(cy + r * std::sin(a)),
                 c);
    }
    fb_.fillRect(pround(cx) - 2, pround(cy) - 2, pround(cx) + 3, pround(cy) + 3, c);
    fb_.resetClip();
}

void Editor::drawRulers() {
    if (!m.rulersOn || m.X0 <= 0 || m.Y0 <= 0) return;
    fb_.resetClip();
    fb_.fillRect(0, 0, m.W, m.Y0, cPanel);
    fb_.fillRect(0, m.Y0, m.X0, m.H, cPanel);
    fb_.fillRect(0, m.Y0 - 1, m.W, m.Y0, cBorder);
    fb_.fillRect(m.X0 - 1, 0, m.X0, m.H, cBorder);
    const double ps = m.scale();
    const int minor = rulerStep(ps, 4);
    const int major = rulerStep(ps, 36);

    // Верхняя: деления по x от начала координат.
    fb_.setClip(gfx::Rect{m.X0, 0, m.W, m.Y0 - 1});
    const int u0 = m.screenToTileX(m.X0) - m.originX - 1;
    const int u1 = m.screenToTileX(m.W) - m.originX + 1;
    for (int u = u0 - ((u0 % minor) + minor) % minor; u <= u1; u += minor) {
        const int sx = m.tileToScreenX(u + m.originX);
        const bool big = u % major == 0;
        fb_.fillRect(sx, big ? 2 : m.Y0 - 5, sx + 1, m.Y0 - 1, big ? cText : cTextDim);
        if (big) tinyNumber(fb_, sx + 2, 3, u, cText);
    }
    for (const int x2 : m.guidesX2) {
        const int sx = m.tileToScreenX(0) + m.scr2(x2);
        fb_.fillRect(sx - 3, m.Y0 - 7, sx + 4, m.Y0 - 4, active.rulerMark);
        fb_.fillRect(sx - 1, m.Y0 - 4, sx + 2, m.Y0 - 1, active.rulerMark);
    }
    if (overCanvas(mouseX(), mouseY())) fb_.fillRect(mouseX(), 0, mouseX() + 1, m.Y0 - 1, cAccent);

    // Левая: деления по y.
    fb_.setClip(gfx::Rect{0, m.Y0, m.X0 - 1, m.H});
    const int v0 = m.screenToTileY(m.Y0) - m.originY - 1;
    const int v1 = m.screenToTileY(m.H) - m.originY + 1;
    for (int v = v0 - ((v0 % minor) + minor) % minor; v <= v1; v += minor) {
        const int sy = m.tileToScreenY(v + m.originY);
        const bool big = v % major == 0;
        fb_.fillRect(big ? 2 : m.X0 - 5, sy, m.X0 - 1, sy + 1, big ? cText : cTextDim);
        if (big) tinyNumber(fb_, 2, sy + 2, v, cText);
    }
    for (const int y2 : m.guidesY2) {
        const int sy = m.tileToScreenY(0) + m.scr2(y2);
        fb_.fillRect(m.X0 - 7, sy - 3, m.X0 - 4, sy + 4, active.rulerMark);
        fb_.fillRect(m.X0 - 4, sy - 1, m.X0 - 1, sy + 2, active.rulerMark);
    }
    if (overCanvas(mouseX(), mouseY())) fb_.fillRect(0, mouseY(), m.X0 - 1, mouseY() + 1, cAccent);
    fb_.resetClip();

    // Угол: сюда щёлкают за экраном сетки и направляющих.
    fb_.fillRect(0, 0, m.X0 - 1, m.Y0 - 1, cFace);
    fb_.fillRect(4, m.Y0 / 2, m.X0 - 5, m.Y0 / 2 + 1, cTextDim);
    fb_.fillRect(m.X0 / 2, 4, m.X0 / 2 + 1, m.Y0 - 5, cTextDim);
}

// ---------------------------------------------------------------------------
// Ввод
// ---------------------------------------------------------------------------

void Editor::handleRulerPress() {
    const int x = frameClickX_;
    const int y = frameClickY_;
    const bool top = y < m.Y0 && x >= m.X0;
    const bool left = x < m.X0 && y >= m.Y0;
    if (!top && !left) {
        showGridScreen();
        return;
    }
    std::vector<int>* const list = top ? &m.guidesX2 : &m.guidesY2;
    const double ps = m.scale();
    const int origin = top ? m.tileToScreenX(0) : m.tileToScreenY(0);
    const int limit2 = 2 * (top ? m.docW() : m.docH());
    auto screenOf = [&](int v2) { return origin + m.scr2(v2); };
    // Направляющая встаёт на границу точек; с Shift — и на середину точки.
    auto valueAt = [&](int s) {
        const double u = static_cast<double>(s - origin) / ps;
        return isKeyPressed(KEY_SHIFT) ? pround(u * 2) : 2 * pround(u);
    };
    const int pos = top ? x : y;
    int hit = -1;
    for (std::size_t i = 0; i < list->size(); ++i) {
        if (std::abs(screenOf((*list)[i]) - pos) <= 4) hit = static_cast<int>(i);
    }
    if (frameClickButton_ == MB_RIGHT) {
        if (hit >= 0) {
            list->erase(list->begin() + hit);
            m.markModified();
        }
        m.needRepaint = true;
        return;
    }
    if (hit < 0) {
        list->push_back(valueAt(pos));
        hit = static_cast<int>(list->size()) - 1;
    }
    while (mousePressed() && !quitNow_) {
        const int v = valueAt(top ? mouseX() : mouseY());
        if (v != (*list)[static_cast<std::size_t>(hit)]) {
            (*list)[static_cast<std::size_t>(hit)] = v;
            drawCanvas();
            drawRulers();
            redraw();
        }
        idle();
    }
    // Вынесенная за документ направляющая убирается: так её снимают, не
    // ища правую кнопку.
    const int v = (*list)[static_cast<std::size_t>(hit)];
    if (v < 0 || v > limit2) list->erase(list->begin() + hit);
    m.markModified();
    m.needRepaint = true;
}

// ---------------------------------------------------------------------------
// Экран «Сетка и направляющие»
// ---------------------------------------------------------------------------

namespace {

std::string halfList(const std::vector<int>& v2) {
    if (v2.empty()) return "нет";
    std::vector<int> s = v2;
    std::sort(s.begin(), s.end());
    std::string out;
    for (const int x2 : s) {
        if (!out.empty()) out += ", ";
        out += std::to_string(x2 / 2);
        if (x2 % 2 != 0) out += "½";
    }
    return out;
}

}  // namespace

void Editor::showGridScreen() {
    bool done = false;
    dropInput();
    pf_.input().keys.clear();
    constexpr int LX = 40;
    constexpr int RX = 420;
    auto stepper = [&](int x, int y, const std::string& label, int& v, int lo, int hi) {
        uiLabelLeft(x, y, 30, label, cText);
        if (uiButton(x + 150, y, 40, 30, "−", v > lo)) {
            v = std::max(lo, v - 1);
            m.markModified();
        }
        fb_.fillRoundRect(x + 196, y, x + 256, y + 30, 6, 6, cFace);
        uiLabel(x + 196, y, 60, 30, std::to_string(v), cText);
        if (uiButton(x + 262, y, 40, 30, "+", v < hi)) {
            v = std::min(hi, v + 1);
            m.markModified();
        }
    };
    while (!done && !quitNow_) {
        beginFrameInput();
        fb_.clear(cWindow);
        textCentered(0, 12, WIN_W, 40, "Сетка и направляющие", cText);
        const int dw = m.docW();
        const int dh = m.docH();

        uiLabelLeft(LX, 46, 24, "Показ", cTextDim);
        if (uiToggle(LX, 72, 330, 30, "Линейки   Ctrl+R", m.rulersOn)) m.rulersOn = !m.rulersOn;
        if (uiToggle(LX, 108, 330, 30, "Направляющие видны   Ctrl+;", m.guidesOn))
            m.guidesOn = !m.guidesOn;
        if (uiToggle(LX, 144, 330, 30, "Через середину документа", m.guideCenter))
            m.guideCenter = !m.guideCenter;
        if (uiToggle(LX, 180, 330, 30, "Через середины клеток крупной сетки", m.guideCellCenters))
            m.guideCellCenters = !m.guideCellCenters;
        if (uiToggle(LX, 216, 330, 30, "Мелкая сетка по точкам   G", m.gridOn))
            m.gridOn = !m.gridOn;

        uiLabelLeft(LX, 260, 24, "Крупная сетка: клетка в точках, 0 — нет", cTextDim);
        stepper(LX, 286, "Ширина клетки", m.majorGridW, 0, dw);
        stepper(LX, 322, "Высота клетки", m.majorGridH, 0, dh);
        static const int kPresets[4] = {8, 16, 32, 64};
        for (int k = 0; k < 4; ++k) {
            if (uiButton(LX + k * 84, 358, 78, 30,
                         std::to_string(kPresets[k]) + "×" + std::to_string(kPresets[k]),
                         kPresets[k] <= std::max(dw, dh))) {
                m.majorGridW = std::min(kPresets[k], dw);
                m.majorGridH = std::min(kPresets[k], dh);
                m.markModified();
            }
        }

        uiLabelLeft(RX, 46, 24, "Начало координат линеек", cTextDim);
        stepper(RX, 72, "X", m.originX, -dw, dw);
        stepper(RX, 108, "Y", m.originY, -dh, dh);
        if (uiButton(RX, 144, 100, 30, "угол", m.originX != 0 || m.originY != 0)) {
            m.originX = 0;
            m.originY = 0;
            m.markModified();
        }
        if (uiButton(RX + 106, 144, 100, 30, "середина", true)) {
            m.originX = dw / 2;
            m.originY = dh / 2;
            m.markModified();
        }
        if (uiButton(RX + 212, 144, 118, 30, "у выделения", m.hasSelection)) {
            m.originX = m.selX;
            m.originY = m.selY;
            m.markModified();
        }

        uiLabelLeft(RX, 190, 24, "Направляющие (в точках)", cTextDim);
        uiLabelLeft(RX, 214, 24, fitText("Вертикальные: " + halfList(m.guidesX2), 340), cText);
        uiLabelLeft(RX, 238, 24, fitText("Горизонтальные: " + halfList(m.guidesY2), 340), cText);
        if (uiButton(RX, 270, 162, 30, "+ Вертикальная…", true)) {
            int v = dw / 2;
            if (askNumbers("Вертикальная направляющая", "Граница перед этой точкой по X",
                           {{"X", &v, 0, dw}}, "Добавить", nullptr)) {
                m.guidesX2.push_back(2 * v);
                m.markModified();
            }
        }
        if (uiButton(RX + 168, 270, 162, 30, "+ Горизонтальная…", true)) {
            int v = dh / 2;
            if (askNumbers("Горизонтальная направляющая", "Граница перед этой точкой по Y",
                           {{"Y", &v, 0, dh}}, "Добавить", nullptr)) {
                m.guidesY2.push_back(2 * v);
                m.markModified();
            }
        }
        if (uiButton(RX, 306, 330, 30, "Убрать все направляющие",
                     !m.guidesX2.empty() || !m.guidesY2.empty())) {
            m.guidesX2.clear();
            m.guidesY2.clear();
            m.markModified();
        }
        uiLabelLeft(RX, 346, 20, "На линейке: щелчок ставит направляющую,", cTextDim);
        uiLabelLeft(RX, 366, 20, "протяжка двигает, Shift — на середину точки,", cTextDim);
        uiLabelLeft(RX, 386, 20, "правая кнопка или вынос за край — убирает.", cTextDim);

        if (uiButton(WIN_W / 2 - 90, WIN_H - 52, 180, 36, "Закрыть", true)) done = true;
        redraw();
        const int key = takeKey();
        if (key == KEY_ESC || key == KEY_ENTER) done = true;
        idle();
    }
    waitMouseRelease();
    m.needRepaint = true;
}

}  // namespace tjtm
