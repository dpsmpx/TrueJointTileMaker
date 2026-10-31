// Инструменты пиксель-арта сверх плана (этап 9): экран карандаша с его
// видами (чистые точки, аэрограф, кисть из выделения, затенение), экран
// заливки с градиентом узором, протяжка градиента и призрак кисти под
// курсором. Модель — src/core/model_art.cpp.

#include <algorithm>
#include <cmath>

#include "app/editor.h"
#include "core/filters.h"
#include "core/pmath.h"
#include "core/theme.h"

namespace tjtm {

using namespace theme;

namespace {

const char* penModeName(int k) {
    switch (k) {
        case PEN_AIRBRUSH: return "Аэрограф";
        case PEN_STAMP: return "Кисть из выделения";
        case PEN_SHADE: return "Затенение";
        default: return "Чистые точки";
    }
}

}  // namespace

void Editor::showBrushOptions() {
    bool done = false;
    dropInput();
    pf_.input().keys.clear();
    constexpr int LX = 60;
    while (!done && !quitNow_) {
        beginFrameInput();
        fb_.clear(cWindow);
        textCentered(0, 12, WIN_W, 40, "Карандаш", cText);
        for (int k = 0; k < PEN_MODES; ++k) {
            if (uiToggle(LX + k * 170, 70, 164, 34, penModeName(k), m.penMode == k)) m.penMode = k;
        }
        const int mode = m.penMode;
        if (mode == PEN_PENCIL) {
            uiLabelLeft(LX, 130, 22,
                        "Каждая точка мазка — ровно один пиксель: замена или смешивание,",
                        cTextDim);
            uiLabelLeft(LX, 150, 22, "пиксель-перфект, сила кисти — в параметрах инструментов.",
                        cTextDim);
        } else if (mode == PEN_AIRBRUSH) {
            uiLabel(0, 108, WIN_W, 20, "Аэрограф — не пиксель-арт: мягкий край и смесь цветов.",
                    cWarn);
            uiLabel(0, 126, WIN_W, 20,
                    "В строгом индексном режиме точка становится цветом кисти, когда напор "
                    "перевалит за половину.",
                    cWarn);
            uiStepper(LX, 150, "Диаметр", m.airSize, 1, 64, 4);
            uiStepper(LX, 192, "Жёсткость", m.airHardness, 0, 100, 10, "%");
            uiStepper(LX, 234, "Напор", m.airFlow, 1, 100, 10, "%");
            uiLabelLeft(LX, 276, 22, "Жёсткость — доля радиуса со сплошной серединой; напор",
                        cTextDim);
            uiLabelLeft(LX, 296, 22, "копится за мазок: проведите по месту ещё раз — гуще.",
                        cTextDim);
            // Отпечаток первым цветом по точке экрана на четыре точки шахматки.
            const Grid<float> dab = fx::softDab(m.airSize, m.airHardness);
            const int cell = std::max(1, std::min(8, 200 / std::max(1, dab.w)));
            const int ox = 470 + (220 - dab.w * cell) / 2;
            const int oy = 150 + (200 - dab.h * cell) / 2;
            drawChecker(ox, oy, ox + dab.w * cell, oy + dab.h * cell);
            const Color c = m.firstColor;
            for (int y = 0; y < dab.h; ++y) {
                for (int x = 0; x < dab.w; ++x) {
                    const float v = dab.at(x, y) * static_cast<float>(m.airFlow) / 100.0F;
                    if (v <= 0) continue;
                    const Color px = argb(
                        static_cast<int>(std::lround(v * static_cast<float>(c.a))), c.r, c.g, c.b);
                    const Color under =
                        ((x * cell / 8 + y * cell / 8) % 2 == 0) ? cCheckA : cCheckB;
                    fb_.fillRect(ox + x * cell, oy + y * cell, ox + (x + 1) * cell,
                                 oy + (y + 1) * cell, blendOver(px, under));
                }
            }
            fb_.frameRect(ox - 1, oy - 1, ox + dab.w * cell + 1, oy + dab.h * cell + 1, cBorder);
        } else if (mode == PEN_STAMP) {
            if (uiButton(LX, 130, 360, 30, "Взять из выделения   Ctrl+E", m.hasSelection)) {
                if (!m.brushFromSelection())
                    showMessage("Пустое выделение", "В выделении нет ни одной видимой точки.");
            }
            uiLabelLeft(LX, 176, 22, "Кисть — выделенный кусок активного слоя, серединой",
                        cTextDim);
            uiLabelLeft(LX, 196, 22, "под курсором; ложится в каждую точку мазка. Прозрачные",
                        cTextDim);
            uiLabelLeft(LX, 216, 22, "точки ничего не кладут, симметрия отражает и саму кисть,",
                        cTextDim);
            uiLabelLeft(LX, 236, 22, "заворот и выделение действуют, как у карандаша.", cTextDim);
            // Сама кисть — крупно, на шахматке.
            const Grid<Color>& b = m.customBrush;
            if (b.empty()) {
                uiLabel(470, 240, 220, 20, "кисти пока нет", cTextDim);
            } else {
                const int cell = std::max(1, std::min(16, 200 / std::max(b.w, b.h)));
                const int ox = 470 + (220 - b.w * cell) / 2;
                const int oy = 150 + (200 - b.h * cell) / 2;
                drawChecker(ox, oy, ox + b.w * cell, oy + b.h * cell);
                fb_.drawImage(b, ox, oy, b.w * cell, b.h * cell);
                fb_.frameRect(ox - 1, oy - 1, ox + b.w * cell + 1, oy + b.h * cell + 1, cBorder);
                uiLabel(470, 356, 220, 20, std::to_string(b.w) + "×" + std::to_string(b.h),
                        cTextDim);
            }
        } else {
            if (uiToggle(LX, 130, 176, 30, "Светлее", !m.shadeDarker)) m.shadeDarker = false;
            if (uiToggle(LX + 184, 130, 176, 30, "Темнее", m.shadeDarker)) m.shadeDarker = true;
            uiLabelLeft(LX, 176, 22, "Мазок сдвигает цвет точки на соседнюю ячейку её ряда",
                        cTextDim);
            uiLabelLeft(LX, 196, 22, "палитры — по восемь в ряд. Светлый конец ряда — где",
                        cTextDim);
            uiLabelLeft(LX, 216, 22, "ярче крайняя ячейка. За мазок точка сдвигается раз;",
                        cTextDim);
            uiLabelLeft(LX, 236, 22, "цвет не из палитры не меняется. Shift — в другую сторону.",
                        cTextDim);
            // Ряд первого цвета — чтобы было видно, куда пойдёт мазок.
            const PalIndex idx =
                m.firstIdx < m.palette.size() ? m.firstIdx : m.nearestPaletteIndex(m.firstColor);
            if (idx < m.palette.size()) {
                const int first = static_cast<int>(idx) / PAL_COLS * PAL_COLS;
                const int last = std::min(first + PAL_COLS, static_cast<int>(m.palette.size()));
                for (int k = first; k < last; ++k) {
                    const int x = 470 + (k - first) * 26;
                    fb_.fillRect(x, 200, x + 24, 224, m.palette[static_cast<std::size_t>(k)]);
                    if (k == static_cast<int>(idx)) fb_.frameRect(x - 2, 198, x + 26, 226, cAccent);
                }
                uiLabel(470, 232, 208, 20, "ряд первого цвета", cTextDim);
            }
        }

        if (uiButton(LX, 400, 360, 30, "Все параметры инструментов…   O", true)) showToolOptions();
        uiLabelLeft(LX, 446, 22, "Сглаживание линии и фигур — в параметрах фигур:", cTextDim);
        uiLabelLeft(LX, 466, 22, "правая кнопка по линии, прямоугольнику или овалу.", cTextDim);
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
// Заливка и градиент узором
// ---------------------------------------------------------------------------

void Editor::showFillOptions() {
    bool done = false;
    dropInput();
    pf_.input().keys.clear();
    constexpr int LX = 60;
    while (!done && !quitNow_) {
        beginFrameInput();
        fb_.clear(cWindow);
        textCentered(0, 12, WIN_W, 40, "Заливка", cText);
        if (uiToggle(LX, 70, 250, 34, "Цветом", !m.fillGradient)) m.fillGradient = false;
        if (uiToggle(LX + 260, 70, 250, 34, "Градиентом узором", m.fillGradient))
            m.fillGradient = true;
        if (m.fillGradient) {
            if (uiToggle(LX, 120, 250, 30, "Линейный", !m.gradientRadial)) m.gradientRadial = false;
            if (uiToggle(LX + 260, 120, 250, 30, "Круговой", m.gradientRadial))
                m.gradientRadial = true;
            uiLabelLeft(LX, 166, 22, "Протяжка от первого цвета ко второму по связной области",
                        cTextDim);
            uiLabelLeft(LX, 186, 22, "под точкой нажатия (в границах выделения). Смесь — узором",
                        cTextDim);
            uiLabelLeft(LX, 206, 22, "Байера 8x8: новых цветов нет, 65 ступеней. Щелчок без",
                        cTextDim);
            uiLabelLeft(LX, 226, 22, "протяжки заливает первым цветом.", cTextDim);
            // Образец перехода первым и вторым цветом, по 4 точки экрана на точку.
            constexpr int SW = 128;
            constexpr int SH = 24;
            constexpr int PX = 4;
            const int ox = LX;
            const int oy = 264;
            for (int y = 0; y < SH; ++y) {
                for (int x = 0; x < SW; ++x) {
                    const double t = m.gradientRadial ? std::hypot(x + 0.5, y - SH / 2.0 + 0.5) / SW
                                                      : (x + 0.5) / SW;
                    static constexpr int kB[8][8] = {
                        {0, 32, 8, 40, 2, 34, 10, 42},  {48, 16, 56, 24, 50, 18, 58, 26},
                        {12, 44, 4, 36, 14, 46, 6, 38}, {60, 28, 52, 20, 62, 30, 54, 22},
                        {3, 35, 11, 43, 1, 33, 9, 41},  {51, 19, 59, 27, 49, 17, 57, 25},
                        {15, 47, 7, 39, 13, 45, 5, 37}, {63, 31, 55, 23, 61, 29, 53, 21}};
                    const bool second = (kB[y & 7][x & 7] + 0.5) / 64.0 < std::min(1.0, t);
                    const Color c = second ? m.secondColor : m.firstColor;
                    fb_.fillRect(ox + x * PX, oy + y * PX, ox + (x + 1) * PX, oy + (y + 1) * PX,
                                 blendOver(c, ((x + y) / 2) % 2 == 0 ? cCheckA : cCheckB));
                }
            }
            fb_.frameRect(ox - 1, oy - 1, ox + SW * PX + 1, oy + SH * PX + 1, cBorder);
        } else {
            uiLabelLeft(LX, 130, 22, "Связная область одного цвета — первым цветом, как в",
                        cTextDim);
            uiLabelLeft(LX, 150, 22, "оригинале; в строгом индексном режиме — цветом ячейки.",
                        cTextDim);
        }
        if (uiButton(LX, 400, 360, 30, "Все параметры инструментов…   O", true)) showToolOptions();
        if (uiButton(WIN_W / 2 - 90, WIN_H - 52, 180, 36, "Закрыть", true)) done = true;
        redraw();
        const int key = takeKey();
        if (key == KEY_ESC || key == KEY_ENTER) done = true;
        idle();
    }
    waitMouseRelease();
    m.needRepaint = true;
}

void Editor::doGradientTool(int ax, int ay) {
    const Grid<std::uint8_t> region = m.gradientRegion(ax, ay);
    if (region.empty()) return;
    m.pushUndo();
    m.ensureTileIdx();
    const Grid<Color> base = m.tile;
    const Grid<PalIndex> baseIdx = m.tileIdx;
    int bx = ax;
    int by = ay;
    m.applyGradient(region, ax, ay, bx, by);
    bool first = true;
    while ((mousePressed() || first) && !quitNow_) {
        const int nx = m.screenToTileX(mouseX());
        const int ny = m.screenToTileY(mouseY());
        if (first || nx != bx || ny != by) {
            first = false;
            bx = nx;
            by = ny;
            // Каждый кадр — с картинки до протяжки: видно ровно то, что
            // ляжет по отпусканию.
            m.tile = base;
            m.tileIdx = baseIdx;
            m.applyGradient(region, ax, ay, bx, by);
            drawCanvas();
            // Направление — линией цвета выделения от центра к центру точек.
            const int ps = m.pixelSize;
            fb_.setClip(canvasRect());
            fb_.line(m.tileToScreenX(ax) + ps / 2, m.tileToScreenY(ay) + ps / 2,
                     m.tileToScreenX(bx) + ps / 2, m.tileToScreenY(by) + ps / 2, cAccent);
            fb_.resetClip();
            redraw();
        }
        idle();
    }
    m.enforceIndexed();
    m.needRepaint = true;
}

// ---------------------------------------------------------------------------
// Призрак кисти
// ---------------------------------------------------------------------------

bool Editor::brushGhostShown() {
    return m.docMode == DOC_TILE && m.toolTile == T_PEN && m.penMode == PEN_STAMP &&
           !m.customBrush.empty() && overCanvas(mouseX(), mouseY());
}

/// Кисть из выделения полупрозрачной поверх холста там, где она ляжет.
void Editor::drawBrushGhost() {
    if (!brushGhostShown()) return;
    const Grid<Color>& b = m.customBrush;
    const int cx = m.screenToTileX(mouseX());
    const int cy = m.screenToTileY(mouseY());
    const int ox = cx - (b.w - 1) / 2;
    const int oy = cy - (b.h - 1) / 2;
    const int ps = m.pixelSize;
    fb_.setClip(canvasRect());
    for (int y = 0; y < b.h; ++y) {
        for (int x = 0; x < b.w; ++x) {
            const Color c = b.at(x, y);
            if (c.a == 0) continue;
            const int sx = m.tileToScreenX(ox + x);
            const int sy = m.tileToScreenY(oy + y);
            fb_.fillRect(sx, sy, sx + ps, sy + ps, argb(c.a / 2, c.r, c.g, c.b));
        }
    }
    fb_.resetClip();
}

}  // namespace tjtm
