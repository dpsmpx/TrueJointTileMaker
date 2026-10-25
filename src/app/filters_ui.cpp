// Вне пиксель-арта (этап 8 RoadToProLevel, раздел 24 плана): экран
// «Фильтры и эффекты», параметры аэрографа и его мазок.
//
// План советовал всего этого не добавлять: размытие, фото-фильтры,
// кривые, мягкая кисть и плавные преобразования дают новые цвета и мягкие
// края. Поэтому они собраны отдельно и помечены, а пиксельные инструменты
// их не касаются. Экран показывает «до» и «после» и пишет только по
// кнопке; в строгом индексном режиме итог сводится к палитре.

#include <algorithm>
#include <cmath>

#include "app/editor.h"
#include "core/filters.h"
#include "core/imageops.h"
#include "core/pmath.h"
#include "core/theme.h"

namespace tjtm {

using namespace theme;

namespace {

enum FilterKind : int {
    FK_BLUR,
    FK_SHARPEN,
    FK_NOISE,
    FK_EDGES,
    FK_EMBOSS,
    FK_GRAY,
    FK_SEPIA,
    FK_INVERT,
    FK_BCS,
    FK_LEVELS,
    FK_CURVES,
    FK_SKEW,
    FK_PERSPECTIVE,
    FK_COUNT
};

const char* filterName(int k) {
    switch (k) {
        case FK_BLUR: return "Размытие";
        case FK_SHARPEN: return "Резкость";
        case FK_NOISE: return "Шум";
        case FK_EDGES: return "Края";
        case FK_EMBOSS: return "Тиснение";
        case FK_GRAY: return "Оттенки серого";
        case FK_SEPIA: return "Сепия";
        case FK_INVERT: return "Негатив";
        case FK_BCS: return "Яркость и контраст";
        case FK_LEVELS: return "Уровни";
        case FK_CURVES: return "Кривые";
        case FK_SKEW: return "Наклон";
        default: return "Перспектива";
    }
}

const char* channelName(int c) {
    switch (c) {
        case 1: return "Красный";
        case 2: return "Зелёный";
        case 3: return "Синий";
        default: return "Все";
    }
}

Color channelColor(int c) {
    switch (c) {
        case 1: return rgb(210, 50, 50);
        case 2: return rgb(40, 160, 60);
        case 3: return rgb(50, 90, 220);
        default: return rgb(40, 40, 48);
    }
}

constexpr int LIST_X = 16;
constexpr int LIST_W = 180;
constexpr int MID_X = 212;
constexpr int PREV_X = 596;
constexpr int PREV_S = 170;
constexpr int BEFORE_Y = 86;
constexpr int AFTER_Y = 298;
// Поле кривых: 256 точек на 256 — ровно по точке на значение.
constexpr int CURVE_X = 260;
constexpr int CURVE_Y = 150;

}  // namespace

void Editor::drawChecker(int x0, int y0, int x1, int y1) {
    for (int y = y0; y < y1; y += 8) {
        for (int x = x0; x < x1; x += 8)
            fb_.fillRect(x, y, std::min(x + 8, x1), std::min(y + 8, y1),
                         ((x - x0) / 8 + (y - y0) / 8) % 2 == 0 ? cCheckA : cCheckB);
    }
}

bool Editor::filterBoxed() const {
    return fx_.kind == FK_SKEW || fx_.kind == FK_PERSPECTIVE;
}

Grid<Color> Editor::runFilter(const Grid<Color>& g) const {
    const FxParams& p = fx_;
    switch (p.kind) {
        case FK_BLUR: return fx::blur(g, p.blurRadius, p.wrap);
        case FK_SHARPEN: return fx::sharpen(g, p.sharpAmount, p.sharpRadius, p.wrap);
        case FK_NOISE:
            return fx::noise(g, p.noiseAmount, p.noiseMono,
                             static_cast<std::uint32_t>(p.noiseSeed));
        case FK_EDGES: return fx::edges(g, p.wrap);
        case FK_EMBOSS: return fx::emboss(g, p.wrap);
        case FK_GRAY: return fx::grayscale(g);
        case FK_SEPIA: return fx::sepia(g);
        case FK_INVERT: return fx::invertColors(g);
        case FK_BCS: return fx::brightnessContrast(g, p.brightness, p.contrast, p.saturation);
        case FK_LEVELS: {
            fx::Levels l;
            l.inLo = p.inLo;
            l.inHi = p.inHi;
            l.gamma = p.gammaPct / 100.0;
            l.outLo = p.outLo;
            l.outHi = p.outHi;
            return fx::levels(g, l);
        }
        case FK_CURVES: return fx::curves(g, p.curves[0], p.curves[1], p.curves[2], p.curves[3]);
        case FK_SKEW: return fx::skew(g, p.skewX, p.skewY, p.wrap);
        case FK_PERSPECTIVE: {
            fx::Quad q;
            for (std::size_t i = 0; i < 4; ++i) {
                q.x[i] = p.quad[2 * i] * g.w;
                q.y[i] = p.quad[2 * i + 1] * g.h;
            }
            return fx::perspective(g, q);
        }
        default: return g;
    }
}

void Editor::showFiltersScreen() {
    if (m.docMode != DOC_TILE || m.tile.empty()) return;
    const auto unchanged = [](const Grid<Color>& g) { return g; };
    if (m.filterResult(unchanged, false).empty()) {
        showMessage("Слой не растровый",
                    "Фильтры меняют точки растрового слоя. Вектор и тайлкарта тайлами "
                    "перерисовываются из своих данных — растрируйте слой на экране слоёв и "
                    "кадров или переключите тайлкарту на правку точками (Ctrl+T).");
        return;
    }
    for (auto& c : fx_.curves) {
        if (c.size() < 2) c = {{0, 0}, {255, 255}};
    }
    // По кругу — когда тайл и так рисуют бесшовным: размытый шов иначе
    // выдал бы себя на стыке.
    fx_.wrap = m.wrapOn || m.seamlessOn;
    bool done = false;
    bool apply = false;
    int drag = -1;        // точка кривой под мышью
    int dragCorner = -1;  // угол перспективы под мышью
    FxParams shown;
    shown.kind = -1;
    Grid<Color> after;
    Grid<Color> before = m.tile;
    std::array<int, 256> hist = fx::histogram(before);
    int hmax = 1;
    for (const int v : hist)
        hmax = std::max(hmax, v);
    dropInput();
    pf_.input().keys.clear();

    // Рамка, которую видит фильтр, и место картинки в окне «После».
    const bool sel = m.hasSelection && m.selW > 0 && m.selH > 0;
    const int ps =
        std::max(1, std::min(PREV_S / std::max(1, m.tile.w), PREV_S / std::max(1, m.tile.h)));
    const bool fits = m.tile.w <= PREV_S && m.tile.h <= PREV_S;
    const double scale = fits ? ps
                              : std::min(static_cast<double>(PREV_S) / m.tile.w,
                                         static_cast<double>(PREV_S) / m.tile.h);
    const int pw = std::max(1, static_cast<int>(m.tile.w * scale));
    const int ph = std::max(1, static_cast<int>(m.tile.h * scale));
    const int px0 = PREV_X + (PREV_S - pw) / 2;
    auto boxX = [&]() { return filterBoxed() && sel ? m.selX : 0; };
    auto boxY = [&]() { return filterBoxed() && sel ? m.selY : 0; };
    auto boxW = [&]() { return filterBoxed() && sel ? m.selW : m.tile.w; };
    auto boxH = [&]() { return filterBoxed() && sel ? m.selH : m.tile.h; };
    auto cornerScreen = [&](int i, int& sx, int& sy) {
        const auto k = static_cast<std::size_t>(i);
        sx = px0 + static_cast<int>(std::lround((boxX() + fx_.quad[2 * k] * boxW()) * scale));
        sy = AFTER_Y +
             static_cast<int>(std::lround((boxY() + fx_.quad[2 * k + 1] * boxH()) * scale));
    };

    while (!done && !quitNow_) {
        beginFrameInput();
        fb_.clear(cWindow);
        textCentered(0, 6, WIN_W, 34, "Фильтры и эффекты", cText);
        uiLabel(0, 32, WIN_W, 20,
                "Не пиксель-арт: новые цвета, полупрозрачность и мягкие края. "
                "В строгом индексном режиме итог сводится к палитре.",
                cWarn);
        uiLabel(0, 50, WIN_W, 18,
                std::string("Активный слой, ") + (sel ? "выделенное" : "вся ячейка") +
                    "   ·   применяется кнопкой, Z отменяет",
                cTextDim);

        // --- список фильтров ---
        for (int k = 0; k < FK_COUNT; ++k) {
            if (uiToggle(LIST_X, 76 + k * 32, LIST_W, 28, filterName(k), fx_.kind == k)) {
                fx_.kind = k;
                drag = -1;
                dragCorner = -1;
            }
        }

        // --- параметры ---
        const int k = fx_.kind;
        int y = 80;
        auto wrapToggle = [&]() {
            if (uiToggle(MID_X, y, 366, 30, "По кругу: край тайла продолжается с другой стороны",
                         fx_.wrap))
                fx_.wrap = !fx_.wrap;
            y += 40;
        };
        switch (k) {
            case FK_BLUR:
                uiStepper(MID_X, y, "Радиус", fx_.blurRadius, 1, 32, 4);
                y += 40;
                wrapToggle();
                break;
            case FK_SHARPEN:
                uiStepper(MID_X, y, "Сила", fx_.sharpAmount, 10, 500, 50, "%");
                y += 40;
                uiStepper(MID_X, y, "Радиус", fx_.sharpRadius, 1, 16, 4);
                y += 40;
                wrapToggle();
                break;
            case FK_NOISE:
                uiStepper(MID_X, y, "Сила", fx_.noiseAmount, 1, 128, 8);
                y += 40;
                if (uiToggle(MID_X, y, 180, 30, "Одноцветный", fx_.noiseMono))
                    fx_.noiseMono = !fx_.noiseMono;
                if (uiButton(MID_X + 186, y, 180, 30, "Другой узор", true)) ++fx_.noiseSeed;
                y += 40;
                break;
            case FK_EDGES:
            case FK_EMBOSS: wrapToggle(); break;
            case FK_BCS:
                uiStepper(MID_X, y, "Яркость", fx_.brightness, -100, 100, 10);
                y += 40;
                uiStepper(MID_X, y, "Контраст", fx_.contrast, -100, 100, 10);
                y += 40;
                uiStepper(MID_X, y, "Насыщенность", fx_.saturation, -100, 100, 10);
                y += 40;
                break;
            case FK_LEVELS: {
                uiStepper(MID_X, y, "Вход: чёрное", fx_.inLo, 0, 254, 16);
                fx_.inHi = std::max(fx_.inHi, fx_.inLo + 1);
                y += 38;
                uiStepper(MID_X, y, "Вход: белое", fx_.inHi, 1, 255, 16);
                fx_.inLo = std::min(fx_.inLo, fx_.inHi - 1);
                y += 38;
                uiStepper(MID_X, y, "Гамма", fx_.gammaPct, 10, 400, 10, "%");
                y += 38;
                uiStepper(MID_X, y, "Выход: чёрное", fx_.outLo, 0, 255, 16);
                y += 38;
                uiStepper(MID_X, y, "Выход: белое", fx_.outHi, 0, 255, 16);
                y += 44;
                // Гистограмма яркости исходника: где лежат тона.
                const int hx = MID_X + 55;
                const int hy = y + 80;
                fb_.fillRect(hx, y, hx + 256, hy, cFace);
                for (int v = 0; v < 256; ++v) {
                    const int hh = hist[static_cast<std::size_t>(v)] * 78 / hmax;
                    if (hh > 0) fb_.fillRect(hx + v, hy - hh, hx + v + 1, hy, cTextDim);
                }
                fb_.fillRect(hx + fx_.inLo, hy, hx + fx_.inLo + 1, hy + 6, cText);
                fb_.fillRect(hx + fx_.inHi, hy, hx + fx_.inHi + 1, hy + 6, cText);
                break;
            }
            case FK_CURVES: {
                for (int c = 0; c < 4; ++c) {
                    if (uiToggle(MID_X + c * 92, y, 88, 28, channelName(c),
                                 fx_.curveChannel == c)) {
                        fx_.curveChannel = c;
                        drag = -1;
                    }
                }
                auto& pts = fx_.curves[static_cast<std::size_t>(fx_.curveChannel)];
                fb_.fillRect(CURVE_X - 1, CURVE_Y - 1, CURVE_X + 257, CURVE_Y + 257, cBorder);
                fb_.fillRect(CURVE_X, CURVE_Y, CURVE_X + 256, CURVE_Y + 256, rgb(250, 250, 252));
                for (int v = 0; v < 256; ++v) {
                    const int hh = hist[static_cast<std::size_t>(v)] * 120 / hmax;
                    if (hh > 0)
                        fb_.fillRect(CURVE_X + v, CURVE_Y + 256 - hh, CURVE_X + v + 1,
                                     CURVE_Y + 256, cFace);
                }
                for (int q = 64; q < 256; q += 64) {
                    fb_.fillRect(CURVE_X + q, CURVE_Y, CURVE_X + q + 1, CURVE_Y + 256, cFace);
                    fb_.fillRect(CURVE_X, CURVE_Y + q, CURVE_X + 256, CURVE_Y + q + 1, cFace);
                }
                fb_.line(CURVE_X, CURVE_Y + 255, CURVE_X + 255, CURVE_Y, cBorder);
                const auto lut = fx::curveLut(pts);
                for (int v = 1; v < 256; ++v)
                    fb_.line(CURVE_X + v - 1, CURVE_Y + 255 - lut[static_cast<std::size_t>(v - 1)],
                             CURVE_X + v, CURVE_Y + 255 - lut[static_cast<std::size_t>(v)],
                             channelColor(fx_.curveChannel));
                for (const auto& [in, out] : pts)
                    fb_.frameRect(CURVE_X + in - 3, CURVE_Y + 255 - out - 3, CURVE_X + in + 4,
                                  CURVE_Y + 255 - out + 4, channelColor(fx_.curveChannel));
                // Щелчок — точка (новая или ближайшая), протяжка двигает её,
                // правая кнопка убирает. Крайние точки ходят только по высоте.
                auto nearestPoint = [&](int mx, int my) {
                    int best = -1;
                    int bd = 64;
                    for (std::size_t i = 0; i < pts.size(); ++i) {
                        const int dx = CURVE_X + pts[i].first - mx;
                        const int dy = CURVE_Y + 255 - pts[i].second - my;
                        if (dx * dx + dy * dy < bd) {
                            bd = dx * dx + dy * dy;
                            best = static_cast<int>(i);
                        }
                    }
                    return best;
                };
                if (frameClick_ && frameClickX_ >= CURVE_X - 4 && frameClickX_ < CURVE_X + 260 &&
                    frameClickY_ >= CURVE_Y - 4 && frameClickY_ < CURVE_Y + 260) {
                    frameClick_ = false;
                    const int hit = nearestPoint(frameClickX_, frameClickY_);
                    if (frameClickButton_ == MB_RIGHT) {
                        if (hit > 0 && hit + 1 < static_cast<int>(pts.size()))
                            pts.erase(pts.begin() + hit);
                    } else if (hit >= 0) {
                        drag = hit;
                    } else {
                        const int in = std::clamp(frameClickX_ - CURVE_X, 1, 254);
                        const int out = std::clamp(CURVE_Y + 255 - frameClickY_, 0, 255);
                        auto at = std::find_if(pts.begin(), pts.end(),
                                               [&](const auto& p) { return p.first > in; });
                        drag = static_cast<int>(at - pts.begin());
                        pts.insert(at, {in, out});
                    }
                }
                if (drag >= 0 && drag < static_cast<int>(pts.size()) && mousePressed()) {
                    const auto d = static_cast<std::size_t>(drag);
                    const bool edge = d == 0 || d + 1 == pts.size();
                    int in = pts[d].first;
                    if (!edge) {
                        // Точка не перескакивает соседей: порядок по входу.
                        in = std::clamp(mouseX() - CURVE_X, pts[d - 1].first + 1,
                                        pts[d + 1].first - 1);
                    }
                    pts[d] = {in, std::clamp(CURVE_Y + 255 - mouseY(), 0, 255)};
                } else {
                    drag = -1;
                }
                const fx::Curve straight = {{0, 0}, {255, 255}};
                if (uiButton(MID_X, CURVE_Y + 268, 180, 30, "Сбросить кривую", pts != straight))
                    pts = straight;
                y = CURVE_Y + 306;
                break;
            }
            case FK_SKEW:
                uiStepper(MID_X, y, "По горизонтали", fx_.skewX, -60, 60, 5, "°");
                y += 40;
                uiStepper(MID_X, y, "По вертикали", fx_.skewY, -60, 60, 5, "°");
                y += 40;
                wrapToggle();
                break;
            case FK_PERSPECTIVE:
                uiLabelLeft(MID_X, y, 24, "Тяните углы на картинке «После».", cText);
                y += 30;
                if (uiButton(MID_X, y, 180, 30, "Сбросить углы", true))
                    fx_.quad = {0, 0, 1, 0, 1, 1, 0, 1};
                y += 40;
                break;
            default:
                uiLabelLeft(MID_X, y, 24, "Без параметров.", cTextDim);
                y += 30;
                break;
        }
        if (filterBoxed() && sel)
            uiLabelLeft(MID_X, y, 24, "Фильтр видит только рамку выделения.", cTextDim);

        // --- перспектива: углы на «После» ---
        if (k == FK_PERSPECTIVE) {
            if (frameClick_ && frameClickButton_ == MB_LEFT) {
                for (int i = 0; i < 4; ++i) {
                    int sx = 0;
                    int sy = 0;
                    cornerScreen(i, sx, sy);
                    if (std::abs(frameClickX_ - sx) <= 7 && std::abs(frameClickY_ - sy) <= 7) {
                        dragCorner = i;
                        frameClick_ = false;
                    }
                }
            }
            if (dragCorner >= 0 && mousePressed()) {
                const auto c = static_cast<std::size_t>(dragCorner);
                fx_.quad[2 * c] =
                    std::clamp(((mouseX() - px0) / scale - boxX()) / boxW(), -0.5, 1.5);
                fx_.quad[2 * c + 1] =
                    std::clamp(((mouseY() - AFTER_Y) / scale - boxY()) / boxH(), -0.5, 1.5);
            } else {
                dragCorner = -1;
            }
        }

        // --- предпросмотр ---
        if (!(fx_ == shown)) {
            after = m.filterResult([this](const Grid<Color>& g) { return runFilter(g); },
                                   filterBoxed());
            shown = fx_;
        }
        uiLabelLeft(PREV_X, BEFORE_Y - 22, 20, "До", cTextDim);
        uiLabelLeft(PREV_X, AFTER_Y - 22, 20, "После", cTextDim);
        drawChecker(px0, BEFORE_Y, px0 + pw, BEFORE_Y + ph);
        fb_.drawImage(before, px0, BEFORE_Y, pw, ph);
        drawChecker(px0, AFTER_Y, px0 + pw, AFTER_Y + ph);
        if (!after.empty()) fb_.drawImage(after, px0, AFTER_Y, pw, ph);
        fb_.frameRect(px0 - 1, BEFORE_Y - 1, px0 + pw + 1, BEFORE_Y + ph + 1, cBorder);
        fb_.frameRect(px0 - 1, AFTER_Y - 1, px0 + pw + 1, AFTER_Y + ph + 1, cBorder);
        if (k == FK_PERSPECTIVE) {
            int lx = 0;
            int ly = 0;
            cornerScreen(3, lx, ly);
            for (int i = 0; i < 4; ++i) {
                int sx = 0;
                int sy = 0;
                cornerScreen(i, sx, sy);
                fb_.line(lx, ly, sx, sy, cAccent);
                lx = sx;
                ly = sy;
            }
            for (int i = 0; i < 4; ++i) {
                int sx = 0;
                int sy = 0;
                cornerScreen(i, sx, sy);
                fb_.fillRect(sx - 4, sy - 4, sx + 5, sy + 5, i == dragCorner ? cAccent : cText);
                fb_.fillRect(sx - 3, sy - 3, sx + 4, sy + 4, rgb(250, 250, 252));
            }
        }

        if (uiButton(MID_X, WIN_H - 50, 180, 36, "Применить", !after.empty())) {
            apply = true;
            done = true;
        }
        if (uiButton(MID_X + 186, WIN_H - 50, 180, 36, "Закрыть", true)) done = true;
        redraw();
        const int key = takeKey();
        if (key == KEY_ESC) done = true;
        if (key == KEY_ENTER && !after.empty()) {
            apply = true;
            done = true;
        }
        if (key == KEY_UP) fx_.kind = (fx_.kind + FK_COUNT - 1) % FK_COUNT;
        if (key == KEY_DOWN) fx_.kind = (fx_.kind + 1) % FK_COUNT;
        idle();
    }
    waitMouseRelease();
    if (apply) m.applyFilter([this](const Grid<Color>& g) { return runFilter(g); }, filterBoxed());
    m.needRepaint = true;
}

// ---------------------------------------------------------------------------
// Аэрограф
// ---------------------------------------------------------------------------

void Editor::showBrushOptions() {
    bool done = false;
    dropInput();
    pf_.input().keys.clear();
    constexpr int LX = 60;
    while (!done && !quitNow_) {
        beginFrameInput();
        fb_.clear(cWindow);
        textCentered(0, 12, WIN_W, 40, "Карандаш и аэрограф", cText);
        if (uiToggle(LX, 70, 320, 34, "Карандаш: чистые точки", !m.airbrush)) m.airbrush = false;
        if (uiToggle(LX + 330, 70, 320, 34, "Аэрограф: мягкая кисть", m.airbrush))
            m.airbrush = true;
        uiLabel(0, 108, WIN_W, 20, "Аэрограф — не пиксель-арт: мягкий край и смесь цветов.", cWarn);
        uiLabel(0, 126, WIN_W, 20,
                "В строгом индексном режиме точка становится цветом кисти, когда напор "
                "перевалит за половину.",
                cWarn);
        uiStepper(LX, 150, "Диаметр", m.airSize, 1, 64, 4);
        uiStepper(LX, 192, "Жёсткость", m.airHardness, 0, 100, 10, "%");
        uiStepper(LX, 234, "Напор", m.airFlow, 1, 100, 10, "%");
        uiLabelLeft(LX, 276, 22, "Жёсткость — доля радиуса со сплошной серединой; напор", cTextDim);
        uiLabelLeft(LX, 296, 22, "копится за мазок: проведите по месту ещё раз — гуще.", cTextDim);

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
                const Color px =
                    argb(static_cast<int>(std::lround(v * static_cast<float>(c.a))), c.r, c.g, c.b);
                const Color under = ((x * cell / 8 + y * cell / 8) % 2 == 0) ? cCheckA : cCheckB;
                fb_.fillRect(ox + x * cell, oy + y * cell, ox + (x + 1) * cell, oy + (y + 1) * cell,
                             blendOver(px, under));
            }
        }
        fb_.frameRect(ox - 1, oy - 1, ox + dab.w * cell + 1, oy + dab.h * cell + 1, cBorder);

        if (uiButton(LX, 340, 320, 30, "Все параметры инструментов…   O", true)) showToolOptions();
        uiLabelLeft(LX, 390, 22, "Сглаживание линии и фигур — в параметрах фигур:", cTextDim);
        uiLabelLeft(LX, 410, 22, "правая кнопка по линии, прямоугольнику или овалу.", cTextDim);
        if (uiButton(WIN_W / 2 - 90, WIN_H - 52, 180, 36, "Закрыть", true)) done = true;
        redraw();
        const int key = takeKey();
        if (key == KEY_ESC || key == KEY_ENTER) done = true;
        idle();
    }
    waitMouseRelease();
    m.needRepaint = true;
}

void Editor::doAirbrushStroke(int startX, int startY) {
    m.pushUndo();
    m.liveDraw = true;
    m.airbrushBegin(startX, startY);
    redraw();
    int lastX = startX;
    int lastY = startY;
    while (mousePressed() && !quitNow_) {
        const int mx = m.screenToTileX(mouseX());
        const int my = m.screenToTileY(mouseY());
        if (mx != lastX || my != lastY) {
            m.airbrushTo(mx, my);
            lastX = mx;
            lastY = my;
            redraw();
        }
        idle();
    }
    m.liveDraw = false;
    m.airbrushEnd();
}

}  // namespace tjtm
