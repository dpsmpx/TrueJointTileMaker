// Выбор цвета (план «Интерфейс», этап B, UX-27). У 1.6 «Точный выбор
// цвета» — четыре ползунка ARGB во всё окно: ни HEX, ни HSV, ни ввода
// числа, ни сравнения «было — стало». Здесь — карточка: квадрат и полосы,
// поля R, G, B, A, H, S, V и HEX, было и стало, недавние цвета, «в
// палитру». «Классика» показывает прежний экран.

#include <algorithm>
#include <cstdio>

#include "app/editor.h"
#include "core/pmath.h"
#include "core/text.h"
#include "core/theme.h"

namespace tjtm {

using namespace theme;

namespace {

constexpr int kSq = 200;  // сторона квадрата насыщенности и яркости
constexpr int kBarH = 14;

std::string hexOf(Color c) {
    char b[16];
    if (c.a == 255) {
        std::snprintf(b, sizeof b, "#%02X%02X%02X", c.r, c.g, c.b);
    } else {
        std::snprintf(b, sizeof b, "#%02X%02X%02X%02X", c.r, c.g, c.b, c.a);
    }
    return b;
}

/// «#RGB», «#RRGGBB», «#RRGGBBAA», с решёткой и без. false — не цвет.
bool parseHex(std::string s, Color& out) {
    s = netTrim(s);
    if (!s.empty() && s[0] == '#') s.erase(0, 1);
    if (s.size() == 3) s = {s[0], s[0], s[1], s[1], s[2], s[2]};
    if (s.size() != 6 && s.size() != 8) return false;
    unsigned v[4] = {0, 0, 0, 255};
    for (std::size_t i = 0; i < s.size() / 2; ++i) {
        unsigned x = 0;
        if (std::sscanf(s.substr(i * 2, 2).c_str(), "%2x", &x) != 1) return false;
        v[i] = x;
    }
    out = argb(static_cast<int>(v[3]), static_cast<int>(v[0]), static_cast<int>(v[1]),
               static_cast<int>(v[2]));
    return true;
}

}  // namespace

void Editor::rememberColor(Color c) {
    std::erase_if(recentColors_, [&](Color x) { return x == c; });
    recentColors_.insert(recentColors_.begin(), c);
    if (recentColors_.size() > 12) recentColors_.resize(12);
}

void Editor::showColorPicker() {
    const auto backdrop = cardBackdrop();
    constexpr int kW = 660;
    constexpr int kH = 440;
    const int x0 = (WIN_W - kW) / 2;
    const int y0 = (WIN_H - kH) / 2;
    const Color before = m.firstColor;
    int r = before.r;
    int g = before.g;
    int b = before.b;
    int a = before.a;
    HSVColor hsv = colorToHsv(before);
    int h = m.hue;  // у серого тон хранится отдельно и не пропадает
    int s = hsv.s;
    int v = hsv.v;
    std::string hex = hexOf(before);
    Grid<Color> plane(kSq, kSq);
    int planeHue = -1;
    int drag = 0;  // 1 квадрат, 2 тон, 3 прозрачность
    bool done = false;
    bool ok = false;
    dropInput();
    pf_.input().keys.clear();

    auto fromRgb = [&]() {
        const HSVColor q = rgbToHsv(r, g, b);
        if (q.s > 0) h = q.h;
        s = q.s;
        v = q.v;
    };
    auto fromHsv = [&]() {
        const Color c = hsvToRgb(h, s, v);
        r = c.r;
        g = c.g;
        b = c.b;
    };

    const int sqX = x0 + 24;
    const int sqY = y0 + 56;
    const int hueY = sqY + kSq + 12;
    const int alphaY = hueY + kBarH + 8;

    while (!done && !quitNow_) {
        beginFrameInput();
        // Протяжка по квадрату и полосам.
        if (frameClick_ && frameClickButton_ == MB_LEFT && frameClickX_ >= sqX &&
            frameClickX_ < sqX + kSq) {
            if (frameClickY_ >= sqY && frameClickY_ < sqY + kSq) drag = 1;
            if (frameClickY_ >= hueY && frameClickY_ < hueY + kBarH) drag = 2;
            if (frameClickY_ >= alphaY && frameClickY_ < alphaY + kBarH) drag = 3;
            if (drag != 0) frameClick_ = false;
        }
        if (!mousePressed()) drag = 0;
        if (drag == 1) {
            s = axisToPercent(clampI(mouseX() - sqX, 0, kSq - 1), kSq);
            v = axisToValue(clampI(mouseY() - sqY, 0, kSq - 1), kSq);
            fromHsv();
        } else if (drag == 2) {
            h = axisToHue(clampI(mouseX() - sqX, 0, kSq - 1), kSq);
            fromHsv();
        } else if (drag == 3) {
            a = axisToByte(clampI(mouseX() - sqX, 0, kSq - 1), kSq);
        }

        dialogBack(backdrop, x0, y0, x0 + kW, y0 + kH);
        textCentered(x0, y0 + 12, x0 + kW, y0 + 40, "Цвет", cText);

        // Квадрат насыщенности и яркости — по тону, перерисовка при смене.
        if (planeHue != h) {
            planeHue = h;
            for (int py = 0; py < kSq; ++py) {
                for (int px = 0; px < kSq; ++px)
                    plane.at(px, py) = hsvToRgb(h, axisToPercent(px, kSq), axisToValue(py, kSq));
            }
        }
        fb_.drawImage(plane, sqX, sqY);
        for (int px = 0; px < kSq; ++px) {
            fb_.fillRect(sqX + px, hueY, sqX + px + 1, hueY + kBarH,
                         hsvToRgb(axisToHue(px, kSq), 100, 100));
            const Color under = ((px / 7) % 2 == 0) ? cCheckA : cCheckB;
            fb_.fillRect(sqX + px, alphaY, sqX + px + 1, alphaY + kBarH,
                         blendOver(argb(axisToByte(px, kSq), r, g, b), under));
        }
        // Метки — кольцом в две линии: видны и на тёмном, и на светлом
        // (у панели чёрные линии пропадали на тёмных цветах).
        const int mx = sqX + percentToAxis(s, kSq);
        const int my = sqY + valueToAxis(v, kSq);
        fb_.strokeRoundRect(mx - 6, my - 6, mx + 7, my + 7, 13, 13, active.antsDark, 2);
        fb_.strokeRoundRect(mx - 4, my - 4, mx + 5, my + 5, 9, 9, active.antsLight, 1);
        for (const auto& [bx, by] : {std::pair{sqX + hueToAxis(h, kSq), hueY},
                                     std::pair{sqX + byteToAxis(a, kSq), alphaY}}) {
            fb_.frameRect(bx - 2, by - 2, bx + 3, by + kBarH + 2, active.antsDark);
            fb_.frameRect(bx - 1, by - 1, bx + 2, by + kBarH + 1, active.antsLight);
        }

        // Поля чисел и HEX.
        const int fx = x0 + 260;
        int fy = y0 + 56;
        auto row = [&](const char* name, int& val, int hi, bool rgbSide) {
            uiLabelLeft(fx, fy, 30, name, cText);
            if (uiNumber(fx + 24, fy, 90, 30, val, 0, hi, 1, 16)) {
                if (rgbSide) {
                    fromRgb();
                } else {
                    fromHsv();
                }
            }
        };
        row("R", r, 255, true);
        fy += 36;
        row("G", g, 255, true);
        fy += 36;
        row("B", b, 255, true);
        fy += 36;
        uiLabelLeft(fx, fy, 30, "A", cText);
        uiNumber(fx + 24, fy, 90, 30, a, 0, 255, 1, 16);
        fy = y0 + 56;
        const int gx = fx + 130;
        auto rowHsv = [&](const char* name, int& val, int hi) {
            uiLabelLeft(gx, fy, 30, name, cText);
            if (uiNumber(gx + 24, fy, 90, 30, val, 0, hi, 1, 10)) fromHsv();
            fy += 36;
        };
        rowHsv("H", h, 359);
        rowHsv("S", s, 100);
        rowHsv("V", v, 100);
        uiLabelLeft(fx, y0 + 206, 30, "HEX", cText);
        const std::string shown = hexOf(argb(a, r, g, b));
        if (textEditId_ != reinterpret_cast<std::uintptr_t>(&hex)) hex = shown;
        if (uiTextLine(fx + 40, y0 + 206, 200, 30, hex)) {
            Color c;
            if (parseHex(hex, c)) {
                r = c.r;
                g = c.g;
                b = c.b;
                a = c.a;
                fromRgb();
            }
        }

        // Было и стало.
        const int wx = x0 + kW - 150;
        uiLabel(wx, y0 + 54, 60, 18, "было", cTextDim);
        uiLabel(wx + 64, y0 + 54, 60, 18, "стало", cTextDim);
        drawChecker(wx, y0 + 74, wx + 124, y0 + 134);
        fb_.fillRect(wx, y0 + 74, wx + 62, y0 + 134, before);
        fb_.fillRect(wx + 62, y0 + 74, wx + 124, y0 + 134, argb(a, r, g, b));
        fb_.frameRect(wx - 1, y0 + 73, wx + 125, y0 + 135, cOutline);
        if (m.docMode == DOC_TILE &&
            uiButton(wx, y0 + 146, 124, 30, "В палитру", m.palCount() < 256)) {
            m.addPaletteColor(argb(a, r, g, b), -1);
            notify("Палитра", "Цвет " + hexOf(argb(a, r, g, b)) + " добавлен в конец");
        }

        // Недавние цвета: щелчок берёт.
        uiLabelLeft(x0 + 24, y0 + 318, 20, "Недавние", cTextDim);
        if (recentColors_.empty())
            uiLabelLeft(x0 + 24, y0 + 344, 20, "Здесь появятся цвета, выбранные кнопкой «Готово»",
                        cTextDim);
        for (std::size_t i = 0; i < recentColors_.size(); ++i) {
            const int cx = x0 + 24 + static_cast<int>(i) * 34;
            const int cy = y0 + 340;
            drawChecker(cx, cy, cx + 28, cy + 28);
            fb_.fillRect(cx, cy, cx + 28, cy + 28, recentColors_[i]);
            fb_.frameRect(cx - 1, cy - 1, cx + 29, cy + 29, cBorder);
            if (takeButtonClick(cx, cy, 28, 28, true)) {
                const Color c = recentColors_[i];
                r = c.r;
                g = c.g;
                b = c.b;
                a = c.a;
                fromRgb();
            }
        }

        if (uiPrimary(x0 + kW - 380, y0 + kH - 52, 170, 36, "Готово", true)) {
            ok = true;
            done = true;
        }
        if (uiButton(x0 + kW - 200, y0 + kH - 52, 170, 36, "Отмена", true)) done = true;
        redraw();
        const int key = takeKey();
        if (key == KEY_ESC) done = true;
        if (key == KEY_ENTER) {
            ok = true;
            done = true;
        }
        idle();
    }
    if (ok) {
        m.firstColor = argb(a, r, g, b);
        m.firstIdx = m.indexOfColor(m.firstColor);
        m.hue = h;
        m.sat = s;
        m.val = v;
        rememberColor(m.firstColor);
    }
    waitMouseRelease();
    endCard(backdrop);
    m.needRepaint = true;
}

}  // namespace tjtm
