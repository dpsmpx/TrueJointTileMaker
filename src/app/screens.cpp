// Модальные экраны (TJTM.pas: «Модальные экраны», «Ввод строки»,
// «Операции над цветом всего тайла», «Свойства карты»).
//
// Каждый экран — свой цикл: снимок ввода, отрисовка, разбор нажатий,
// пауза. Так было в оригинале, и так перенос остаётся сличимым с ним.

#include <algorithm>

#include "app/editor.h"
#include "app/textfield.h"
#include "core/pmath.h"
#include "core/text.h"
#include "core/theme.h"

namespace tjtm {

using namespace theme;

// ---------------------------------------------------------------------------
// Текст, сообщения, вопросы
// ---------------------------------------------------------------------------

/// Экран с прокручиваемым текстом. Закрывается кнопкой, Enter или Escape.
void Editor::showTextScreen(const std::string& title, const std::vector<std::string>& paragraphs) {
    bool done = false;
    int top = 0;
    dropInput();
    pf_.input().keys.clear();
    int lineH = textHeight() + 2;
    if (lineH < 8) lineH = 8;
    const int visible = std::max(1, (WIN_H - 120) / lineH);
    const std::vector<std::string> lines = layoutText(paragraphs, WIN_W - 80);

    while (!done && !quitNow_) {
        beginFrameInput();
        const int maxTop = std::max(0, static_cast<int>(lines.size()) - visible);
        top = clampI(top, 0, maxTop);

        fb_.clear(cWindow);
        textCentered(0, 14, WIN_W, 44, title, cText);
        fb_.fillRoundRect(24, 52, WIN_W - 24, WIN_H - 56, BTN_R, BTN_R, cBorder);
        fb_.fillRoundRect(25, 53, WIN_W - 25, WIN_H - 57, BTN_R, BTN_R, cBack);
        for (int i = 0; i < visible; ++i) {
            const int k = top + i;
            if (k >= static_cast<int>(lines.size())) break;
            textOut(36, 60 + i * lineH, lines[static_cast<std::size_t>(k)], cText);
        }

        if (maxTop > 0) {
            uiLabel(WIN_W - 220, WIN_H - 52, 200, 20,
                    "строки " + std::to_string(top + 1) + "–" +
                        std::to_string(std::min(static_cast<int>(lines.size()), top + visible)) +
                        " из " + std::to_string(lines.size()),
                    cTextDim);
            if (uiButton(24, WIN_H - 50, 60, 28, "▲", top > 0)) top -= visible;
            if (uiButton(90, WIN_H - 50, 60, 28, "▼", top < maxTop)) top += visible;
        }
        if (uiButton(WIN_W / 2 - 60, WIN_H - 50, 120, 28, "Закрыть", true)) done = true;

        redraw();
        const int key = takeKey();
        if (key == KEY_ESC || key == KEY_ENTER || key == KEY_BACK) done = true;
        if (key == KEY_DOWN) ++top;
        if (key == KEY_UP) --top;
        if (key == KEY_PGDN) top += visible;
        if (key == KEY_PGUP) top -= visible;
        top -= takeWheel() * 3;
        idle();
    }
    waitMouseRelease();
    m.needRepaint = true;
}

/// Подпись под кнопками вопроса: какую нажимает Enter, какую — Escape.
std::string Editor::keysHint(const std::string& enter, const std::string& escape) {
    std::string s = "Enter — «";
    s += enter;
    s += "»,   Escape — «";
    s += escape;
    s += "»";
    return s;
}

std::optional<Editor::CardBack> Editor::cardBackdrop() {
    if (!active.modern) return std::nullopt;
    CardBack b{fb_, fb_};
    b.dim.fillRect(0, 0, WIN_W, WIN_H, active.scrim);
    return b;
}

void Editor::endCard(const std::optional<CardBack>& backdrop) {
    if (backdrop) fb_ = backdrop->orig;
}

void Editor::dialogBack(const std::optional<CardBack>& backdrop, int x0, int y0, int x1, int y1) {
    if (!backdrop) {
        fb_.clear(cWindow);
        return;
    }
    fb_ = backdrop->dim;
    const int r = active.radius * 4;
    fb_.fillRoundRect(x0, y0, x1, y1, r, r, cBorder);
    fb_.fillRoundRect(x0 + 1, y0 + 1, x1 - 1, y1 - 1, r - 2, r - 2, cFace);
}

void Editor::notify(const std::string& title, const std::string& text) {
    if (!active.modern) {
        showMessage(title, text);
        return;
    }
    toastTitle_ = title;
    toastText_ = text;
    toastUntil_ = pf_.milliseconds() + 4000;
    m.needRepaint = true;
}

void Editor::drawToast() {
    if (toastText_.empty()) return;
    const std::vector<std::string> lines = layoutText({toastText_}, 460);
    int w = textWidth(toastTitle_);
    for (const std::string& l : lines)
        w = std::max(w, textWidth(l));
    w += 28;
    const int lh = textHeight() + 2;
    const int h = (static_cast<int>(lines.size()) + 1) * lh + 18;
    // Внизу холста, над лентой кадров: там, куда смотрят после действия,
    // и не поверх панели с инструментами.
    const int x = std::max(8, (m.W - w) / 2);
    const int y = m.H - h - 14;
    fb_.fillRoundRect(x, y, x + w, y + h, 12, 12, active.inverseSurface);
    textOut(x + 14, y + 8, toastTitle_, active.inverseOnSurface);
    for (std::size_t i = 0; i < lines.size(); ++i)
        textOut(x + 14, y + 10 + static_cast<int>(i + 1) * lh, lines[i], active.inverseOnSurface);
}

void Editor::showError(const std::string& title, const std::string& human,
                       const std::string& details) {
    if (!active.modern) {
        showMessage(title, details.empty() ? human : details);
        return;
    }
    constexpr int kW = 600;
    const auto backdrop = cardBackdrop();
    const int lh = textHeight() + 2;
    bool more = false;
    bool done = false;
    dropInput();
    pf_.input().keys.clear();
    while (!done && !quitNow_) {
        beginFrameInput();
        std::vector<std::string> lines = layoutText({human}, kW - 48);
        std::vector<std::string> extra;
        if (more) extra = layoutText({details}, kW - 48);
        const int h =
            116 + static_cast<int>(lines.size() + extra.size()) * lh + (extra.empty() ? 0 : 10);
        const int x0 = (WIN_W - kW) / 2;
        const int y0 = std::max(8, (WIN_H - h) / 2);
        dialogBack(backdrop, x0, y0, x0 + kW, y0 + h);
        textCentered(x0, y0 + 12, x0 + kW, y0 + 40, title, cWarn);
        int y = y0 + 48;
        for (const std::string& l : lines) {
            textOut(x0 + 24, y, l, cText);
            y += lh;
        }
        if (!extra.empty()) {
            y += 10;
            for (const std::string& l : extra) {
                textOut(x0 + 24, y, l, cTextDim);
                y += lh;
            }
        }
        if (!details.empty() &&
            uiButton(x0 + 24, y0 + h - 52, 150, 34, more ? "Скрыть" : "Подробнее", true))
            more = !more;
        if (uiPrimary(x0 + kW - 174, y0 + h - 52, 150, 34, "Закрыть", true)) done = true;
        redraw();
        const int key = takeKey();
        if (key == KEY_ESC || key == KEY_ENTER) done = true;
        idle();
    }
    waitMouseRelease();
    endCard(backdrop);
    m.needRepaint = true;
}

void Editor::errorOpen(const std::string& title, const std::string& details) {
    showError(title,
              "Файл не открылся: его нет на прежнем месте, это не картинка, которую редактор "
              "читает (PNG, GIF, APNG, BMP, TIFF, JPEG, проект .tjt, карта .map), или он "
              "повреждён. Проверьте путь или откройте другой файл (Ctrl+O).",
              details);
}

void Editor::errorSave(const std::string& title, const std::string& details) {
    showError(title,
              "Записать не удалось: папка защищена от записи, диск заполнен или файл занят "
              "другой программой. Сохраните в другую папку через «Сохранить как» "
              "(Ctrl+Shift+S).",
              details);
}

void Editor::showMessage(const std::string& title, const std::string& text) {
    // Сообщение в несколько строк — карточкой над окном, как вопрос:
    // страница во всё окно ради одной строки теряла картинку (UX-45).
    constexpr int kW = 560;
    const std::vector<std::string> lines = layoutText({text}, kW - 48);
    if (!active.modern || lines.size() > 12) {
        showTextScreen(title, {text});
        return;
    }
    const auto backdrop = cardBackdrop();
    const int lh = textHeight() + 2;
    const int h = 112 + static_cast<int>(lines.size()) * lh;
    const int x0 = (WIN_W - kW) / 2;
    const int y0 = (WIN_H - h) / 2;
    bool done = false;
    dropInput();
    pf_.input().keys.clear();
    while (!done && !quitNow_) {
        beginFrameInput();
        dialogBack(backdrop, x0, y0, x0 + kW, y0 + h);
        textCentered(x0, y0 + 12, x0 + kW, y0 + 40, title, cText);
        for (std::size_t i = 0; i < lines.size(); ++i)
            textOut(x0 + 24, y0 + 48 + static_cast<int>(i) * lh, lines[i], cText);
        if (uiPrimary(x0 + kW / 2 - 70, y0 + h - 50, 140, 34, "Закрыть", true)) done = true;
        redraw();
        const int key = takeKey();
        if (key == KEY_ESC || key == KEY_ENTER || key == KEY_BACK) done = true;
        idle();
    }
    waitMouseRelease();
    endCard(backdrop);
    m.needRepaint = true;
}

/// Новый документ карточкой (UX-26): у 1.6 число набиралось только кнопками
/// ±1 и ±8 — тайл 200×120 из 32×32 стоил около тридцати щелчков, — размеры
/// были только квадратными, а фон — только белым или прозрачным по
/// настройке.
bool Editor::askNewSizeCard(const std::string& title, int& nw, int& nh, int lo, int hi,
                            int* background) {
    static constexpr int kSizes[][2] = {{16, 16}, {16, 32},   {32, 32},  {48, 48},
                                        {64, 64}, {128, 128}, {256, 256}};
    static const char* const kBg[] = {"Прозрачный", "Белый", "Второй цвет"};
    const auto backdrop = cardBackdrop();
    const int h = background != nullptr ? 380 : 300;
    const int x0 = (WIN_W - 600) / 2;
    const int y0 = (WIN_H - h) / 2;
    bool lock = false;
    int ratioW = nw;
    int ratioH = nh;
    bool done = false;
    bool ok = false;
    dropInput();
    pf_.input().keys.clear();
    while (!done && !quitNow_) {
        beginFrameInput();
        dialogBack(backdrop, x0, y0, x0 + 600, y0 + h);
        textCentered(x0, y0 + 12, x0 + 600, y0 + 40, title, cText);
        uiLabel(x0, y0 + 38, 600, 20,
                "Размер от " + std::to_string(lo) + " до " + std::to_string(hi) + " точек",
                cTextDim);
        const int pw = nw;
        const int ph = nh;
        uiLabelLeft(x0 + 40, y0 + 72, 34, "Ширина", cText);
        uiNumber(x0 + 140, y0 + 72, 140, 34, nw, lo, hi, 1, 8);
        uiLabelLeft(x0 + 40, y0 + 114, 34, "Высота", cText);
        uiNumber(x0 + 140, y0 + 114, 140, 34, nh, lo, hi, 1, 8);
        // Замок пропорций: вторая сторона идёт за первой.
        if (uiSwitch(x0 + 320, y0 + 92, 240, "Сохранять пропорции", lock) && lock) {
            ratioW = nw;
            ratioH = nh;
        }
        if (lock && nw != pw) nh = clampI((nw * ratioH + ratioW / 2) / ratioW, lo, hi);
        if (lock && nh != ph && nw == pw) nw = clampI((nh * ratioW + ratioH / 2) / ratioH, lo, hi);

        uiLabelLeft(x0 + 24, y0 + 158, 20, "Готовые размеры", cTextDim);
        for (int i = 0; i < static_cast<int>(std::size(kSizes)); ++i) {
            const int sw = kSizes[i][0];
            const int sh = kSizes[i][1];
            if (sw > hi || sh > hi) continue;
            const bool cur = nw == sw && nh == sh;
            if (uiToggle(x0 + 24 + i * 79, y0 + 182, 73, 30,
                         std::to_string(sw) + "×" + std::to_string(sh), cur)) {
                nw = sw;
                nh = sh;
                ratioW = sw;
                ratioH = sh;
            }
        }
        if (background != nullptr) {
            uiLabelLeft(x0 + 24, y0 + 228, 20, "Фон", cTextDim);
            for (int i = 0; i < 3; ++i) {
                if (uiToggle(x0 + 24 + i * 186, y0 + 252, 180, 32, kBg[i], *background == i))
                    *background = i;
            }
        }
        if (uiPrimary(x0 + 300 - 180, y0 + h - 56, 170, 36, "Создать", true)) {
            ok = true;
            done = true;
        }
        if (uiButton(x0 + 310, y0 + h - 56, 170, 36, "Отмена", true)) done = true;
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
    endCard(backdrop);
    m.needRepaint = true;
    return ok;
}

/// Вопрос с тремя вариантами: 1 — да, 0 — нет, -1 — отмена.
int Editor::ask3(const std::string& title, const std::string& text, const std::string& yes,
                 const std::string& no, const std::string& cancel) {
    int res = -1;
    bool done = false;
    const auto backdrop = cardBackdrop();
    dropInput();
    pf_.input().keys.clear();
    const int H = WIN_H;
    // На карточке текст — в её ширину, на странице — почти во всё окно.
    const int tx = backdrop ? WIN_W / 2 - 280 : 40;
    while (!done && !quitNow_) {
        beginFrameInput();
        dialogBack(backdrop, WIN_W / 2 - 300, H / 2 - 104, WIN_W / 2 + 300, H / 2 + 104);
        textCentered(0, H / 2 - 90, WIN_W, H / 2 - 50, title, cText);
        textCentered(tx, H / 2 - 46, WIN_W - tx, H / 2 + 10, text, cText);
        if (uiPrimary(WIN_W / 2 - 270, H / 2 + 30, 170, 34, yes, true)) {
            res = 1;
            done = true;
        }
        if (uiButton(WIN_W / 2 - 85, H / 2 + 30, 170, 34, no, true)) {
            res = 0;
            done = true;
        }
        if (uiButton(WIN_W / 2 + 100, H / 2 + 30, 170, 34, cancel, true)) {
            res = -1;
            done = true;
        }
        // Главная кнопка — первая: Enter её нажимает (UX-19).
        if (!classicUi_)
            textCentered(0, H / 2 + 72, WIN_W, H / 2 + 92, keysHint(yes, cancel), cTextDim);
        redraw();
        const int key = takeKey();
        if (key == KEY_ESC) {
            res = -1;
            done = true;
        } else if (key == KEY_ENTER && !classicUi_) {
            res = 1;
            done = true;
        }
        idle();
    }
    waitMouseRelease();
    endCard(backdrop);
    m.needRepaint = true;
    return res;
}

/// Вопрос с двумя ответами. Escape равнозначен отказу.
bool Editor::askYesNo(const std::string& title, const std::string& text, const std::string& yes,
                      const std::string& no) {
    bool res = false;
    bool done = false;
    const auto backdrop = cardBackdrop();
    dropInput();
    pf_.input().keys.clear();
    const int H = WIN_H;
    const int tx = backdrop ? WIN_W / 2 - 230 : 40;
    while (!done && !quitNow_) {
        beginFrameInput();
        dialogBack(backdrop, WIN_W / 2 - 250, H / 2 - 104, WIN_W / 2 + 250, H / 2 + 104);
        textCentered(0, H / 2 - 90, WIN_W, H / 2 - 50, title, cText);
        textCentered(tx, H / 2 - 46, WIN_W - tx, H / 2 + 10, text, cText);
        if (uiPrimary(WIN_W / 2 - 180, H / 2 + 30, 170, 34, yes, true)) {
            res = true;
            done = true;
        }
        if (uiButton(WIN_W / 2 + 10, H / 2 + 30, 170, 34, no, true)) {
            res = false;
            done = true;
        }
        if (!classicUi_)
            textCentered(0, H / 2 + 72, WIN_W, H / 2 + 92, keysHint(yes, no), cTextDim);
        redraw();
        const int key = takeKey();
        if (key == KEY_ESC) {
            res = false;
            done = true;
        } else if (key == KEY_ENTER && !classicUi_) {
            res = true;
            done = true;
        }
        idle();
    }
    waitMouseRelease();
    endCard(backdrop);
    m.needRepaint = true;
    return res;
}

/// Экран размера нового документа. Оригинал звал его и для карты — с
/// заголовком «Создать новый тайл» и пределами тайла 4..512, хотя карта
/// не бывает больше 256; здесь у каждого режима свои заголовок и пределы.
bool Editor::askNewSize(const std::string& title, int& nw, int& nh, int lo, int hi,
                        int* background) {
    static const int kPresets[] = {8, 16, 32, 64, 128, 256};
    nw = clampI(nw, lo, hi);
    nh = clampI(nh, lo, hi);
    bool done = false;
    bool ok = false;
    if (active.modern) return askNewSizeCard(title, nw, nh, lo, hi, background);
    dropInput();
    pf_.input().keys.clear();

    auto stepRow = [&](int y, const std::string& caption, int& v) {
        uiLabel(60, y, 160, 34, caption, cText);
        if (uiButton(230, y, 46, 34, "−8", v > lo)) v = clampI(v - 8, lo, hi);
        if (uiButton(282, y, 46, 34, "−1", v > lo)) v = clampI(v - 1, lo, hi);
        fb_.fillRoundRect(334, y, 434, y + 34, 6, 6, cFace);
        uiLabel(334, y, 100, 34, std::to_string(v), cText);
        if (uiButton(440, y, 46, 34, "+1", v < hi)) v = clampI(v + 1, lo, hi);
        if (uiButton(492, y, 46, 34, "+8", v < hi)) v = clampI(v + 8, lo, hi);
    };

    while (!done && !quitNow_) {
        beginFrameInput();
        fb_.clear(cWindow);
        textCentered(0, 30, WIN_W, 66, title, cText);
        uiLabel(0, 70, WIN_W, 26,
                "Размер от " + std::to_string(lo) + " до " + std::to_string(hi) + " пикселей",
                cTextDim);
        stepRow(120, "Ширина", nw);
        stepRow(170, "Высота", nh);
        uiLabel(60, 230, 200, 30, "Быстрый выбор", cTextDim);
        for (int i = 0; i < 6; ++i) {
            if (kPresets[i] > hi) continue;
            if (uiButton(60 + i * 82, 264, 76, 34, std::to_string(kPresets[i]), true)) {
                nw = kPresets[i];
                nh = kPresets[i];
            }
        }
        if (uiButton(WIN_W / 2 - 180, 350, 170, 38, "Создать", true)) {
            ok = true;
            done = true;
        }
        if (uiButton(WIN_W / 2 + 10, 350, 170, 38, "Отмена", true)) {
            ok = false;
            done = true;
        }
        redraw();
        const int key = takeKey();
        if (key == KEY_ESC) {
            ok = false;
            done = true;
        }
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

/// Точная настройка цвета по каналам ARGB. Положение ползунка и разбор
/// щелчка — одна пара формул: маркер встаёт ровно под курсор.
void Editor::showColorScreen() {
    constexpr int SX0 = 70;
    constexpr int SX1 = WIN_W - 70;
    auto valueToX = [](int v) {
        return SX0 + pround(static_cast<double>(clampI(v, 0, 255) * (SX1 - SX0)) / 255);
    };
    auto xToValue = [](int px) {
        return clampI(pround(static_cast<double>((px - SX0) * 255) / (SX1 - SX0)), 0, 255);
    };

    int ch[4] = {m.firstColor.a, m.firstColor.r, m.firstColor.g, m.firstColor.b};
    static const char* const kNames[4] = {"Прозрачность", "Красный", "Зелёный", "Синий"};
    bool done = false;
    dropInput();
    pf_.input().keys.clear();

    auto apply = [&]() {
        m.firstColor = argb(ch[0], ch[1], ch[2], ch[3]);
        // Оригинал оставлял прежнюю привязку к ячейке палитры, и рамка на
        // палитре показывала ячейку, к цвету которой новый уже не относится.
        m.firstIdx = m.indexOfColor(m.firstColor);
        m.syncHSVFromColor();
    };

    while (!done && !quitNow_) {
        beginFrameInput();
        fb_.clear(cWindow);
        textCentered(0, 16, WIN_W, 50, "Точный выбор цвета", cText);
        for (int i = 0; i < 4; ++i) {
            const int y = 74 + i * 54;
            uiLabel(0, y - 22, WIN_W, 20, std::string(kNames[i]) + ": " + std::to_string(ch[i]),
                    cText);
            fb_.fillRoundRect(SX0 - 6, y, SX1 + 6, y + 22, 5, 5, cFace);
            fb_.fillRect(SX0, y + 9, SX1, y + 13, cBorder);
            const int mx = valueToX(ch[i]);
            fb_.fillRoundRect(mx - 7, y - 2, mx + 7, y + 24, 4, 4, cAccent);
            // Тащить ползунок можно, удерживая кнопку в его полосе.
            if (mousePressed() && mouseY() >= y - 6 && mouseY() <= y + 28)
                ch[i] = xToValue(mouseX());
        }
        fb_.fillRect(70, 296, WIN_W - 68, 366, cBorder);
        // Шахматка блоками по 8 точек: экран перерисовывается каждый кадр,
        // пока тащат ползунок.
        for (int bx = 0; bx <= (WIN_W - 140) / 8; ++bx) {
            for (int by = 0; by <= 8; ++by) {
                if ((bx + by) % 2 == 0) {
                    fb_.fillRect(71 + bx * 8, 297 + by * 8, std::min(WIN_W - 69, 79 + bx * 8),
                                 std::min(365, 305 + by * 8), cCheckA);
                }
            }
        }
        fb_.fillRect(71, 297, WIN_W - 69, 365, argb(ch[0], ch[1], ch[2], ch[3]));

        if (uiButton(WIN_W / 2 - 180, WIN_H - 80, 170, 38, "Применить", true)) {
            apply();
            done = true;
        }
        if (uiButton(WIN_W / 2 + 10, WIN_H - 80, 170, 38, "Отмена", true)) done = true;
        redraw();
        const int key = takeKey();
        if (key == KEY_ESC || key == KEY_BACK) done = true;
        if (key == KEY_ENTER) {
            apply();
            done = true;
        }
        idle();
    }
    waitMouseRelease();
    m.needRepaint = true;
}

// ---------------------------------------------------------------------------
// Поле ввода
// ---------------------------------------------------------------------------

/// Клавиша правки поля: стрелки, Home и End, удаление, слова по Ctrl,
/// выделение по Shift, буфер обмена. Enter, Escape и Tab решает окно.
bool editTextField(TextField& tf, const KeyPress& kp, Platform& pf) {
    const bool shift = kp.shift;
    auto moveTo = [&](std::size_t p) {
        tf.caret = p;
        if (!shift) tf.anchor = p;
    };
    switch (kp.vk) {
        case KEY_LEFT:
            if (!shift && tf.hasSel() && !kp.ctrl) {
                moveTo(tf.selLo());
            } else {
                moveTo(kp.ctrl ? tf.wordLeft(tf.caret) : (tf.caret > 0 ? tf.caret - 1 : 0));
            }
            return true;
        case KEY_RIGHT:
            if (!shift && tf.hasSel() && !kp.ctrl) {
                moveTo(tf.selHi());
            } else {
                moveTo(kp.ctrl ? tf.wordRight(tf.caret) : std::min(tf.text.size(), tf.caret + 1));
            }
            return true;
        case KEY_HOME: moveTo(0); return true;
        case KEY_END: moveTo(tf.text.size()); return true;
        case KEY_BACK:
            if (tf.hasSel()) {
                tf.eraseSel();
            } else if (tf.caret > 0) {
                const std::size_t from = kp.ctrl ? tf.wordLeft(tf.caret) : tf.caret - 1;
                tf.text.erase(from, tf.caret - from);
                tf.caret = tf.anchor = from;
            }
            return true;
        case KEY_DELETE:
            if (tf.hasSel()) {
                tf.eraseSel();
            } else if (tf.caret < tf.text.size()) {
                const std::size_t to = kp.ctrl ? tf.wordRight(tf.caret) : tf.caret + 1;
                tf.text.erase(tf.caret, to - tf.caret);
            }
            return true;
        case KEY_A:
            if (!kp.ctrl) return false;
            tf.anchor = 0;
            tf.caret = tf.text.size();
            return true;
        case KEY_C:
        case KEY_X:
            if (!kp.ctrl) return false;
            if (tf.hasSel()) {
                pf.setClipboardText(u32ToUtf8(tf.text.substr(tf.selLo(), tf.selHi() - tf.selLo())));
                if (kp.vk == KEY_X) tf.eraseSel();
            }
            return true;
        case KEY_V:
            if (!kp.ctrl) return false;
            tf.insert(utf8ToU32(pf.clipboardText()));
            return true;
        case KEY_INSERT:
            if (!kp.shift) return false;
            tf.insert(utf8ToU32(pf.clipboardText()));
            return true;
        default: return false;
    }
}

/// Ввод строки. true — подтверждено, значение в value.
bool Editor::askText(const std::string& title, const std::string& prompt,
                     const std::string& initial, std::string& value) {
    bool done = false;
    bool ok = false;
    const auto backdrop = cardBackdrop();
    dropInput();
    pf_.input().keys.clear();
    pf_.input().presses.clear();
    const int fx = 60;
    const int fy = WIN_H / 2 - 20;
    const int fw = WIN_W - 120;
    const int fh = 30;
    TextField tf;
    tf.text = utf8ToU32(initial);
    tf.caret = tf.anchor = tf.text.size();
    pf_.startTextInput(fx, fy, fw, fh);
    bool selecting = false;
    const int blinkStart = pf_.milliseconds();

    auto width = [&](std::size_t n) {
        return mono_.rawWidth(u32ToUtf8(std::u32string_view(tf.text).substr(0, n)));
    };
    auto posAt = [&](int px) {
        const int x = px - (fx + 6) + tf.scroll;
        std::size_t best = 0;
        int bestD = 1 << 30;
        for (std::size_t i = 0; i <= tf.text.size(); ++i) {
            const int d = std::abs(width(i) - x);
            if (d < bestD) {
                bestD = d;
                best = i;
            }
        }
        return best;
    };

    while (!done && !quitNow_) {
        beginFrameInput();
        Input& in = pf_.input();

        // Мышь: щелчок ставит каретку, протяжка выделяет.
        if (frameClick_ && frameClickButton_ == MB_LEFT && frameClickX_ >= fx &&
            frameClickX_ < fx + fw && frameClickY_ >= fy && frameClickY_ < fy + fh) {
            frameClick_ = false;
            tf.caret = posAt(frameClickX_);
            if (!isKeyPressed(KEY_SHIFT)) tf.anchor = tf.caret;
            selecting = true;
        }
        if (selecting) {
            if (mousePressed()) {
                tf.caret = posAt(mouseX());
            } else {
                selecting = false;
            }
        }
        // Клавиши поля.
        while (!in.presses.empty()) {
            const KeyPress kp = in.presses.front();
            in.presses.pop_front();
            if (kp.vk == KEY_ENTER) {
                ok = true;
                done = true;
            } else if (kp.vk == KEY_ESC) {
                done = true;
            } else {
                editTextField(tf, kp, pf_);
            }
        }
        if (!in.text.empty()) {
            tf.insert(utf8ToU32(in.text));
            in.text.clear();
        }
        in.keys.clear();

        // Каретка держится в видимой части поля.
        const int inner = fw - 12;
        const int cx = width(tf.caret);
        if (cx - tf.scroll > inner) tf.scroll = cx - inner;
        if (cx - tf.scroll < 0) tf.scroll = cx;

        dialogBack(backdrop, fx - 24, fy - 120, fx + fw + 24, fy + 110);
        textCentered(0, fy - 110, WIN_W, fy - 74, title, cText);
        uiLabel(0, fy - 70, WIN_W, 24, prompt, cTextDim);
        uiLabel(0, fy + 34, WIN_W, 20,
                "Enter — готово, Escape — отмена. Работают выделение и буфер обмена", cTextDim);

        fb_.fillRect(fx, fy, fx + fw, fy + fh, cAccent);
        fb_.fillRect(fx + 1, fy + 1, fx + fw - 1, fy + fh - 1, active.field);
        fb_.setClip(gfx::Rect{fx + 4, fy + 2, fx + fw - 4, fy + fh - 2});
        const int tx = fx + 6 - tf.scroll;
        const int ty = fy + (fh - mono_.textHeight()) / 2;
        if (tf.hasSel()) {
            fb_.fillRect(tx + width(tf.selLo()), ty, tx + width(tf.selHi()),
                         ty + mono_.textHeight(), active.fieldSelection);
        }
        mono_.draw(fb_, tx, ty, u32ToUtf8(tf.text), cText);
        if (((pf_.milliseconds() - blinkStart) / 530) % 2 == 0)
            fb_.fillRect(tx + cx, ty, tx + cx + 1, ty + mono_.textHeight(), cText);
        fb_.resetClip();

        if (uiPrimary(WIN_W / 2 - 180, fy + 60, 170, 36, "Готово", true)) {
            ok = true;
            done = true;
        }
        if (uiButton(WIN_W / 2 + 10, fy + 60, 170, 36, "Отмена", true)) done = true;
        redraw();
        idle();
    }
    pf_.stopTextInput();
    if (ok) value = u32ToUtf8(tf.text);
    waitMouseRelease();
    endCard(backdrop);
    m.needRepaint = true;
    return ok;
}

// ---------------------------------------------------------------------------
// Операции над цветом
// ---------------------------------------------------------------------------

void Editor::showColorOpsScreen() {
    bool done = false;
    dropInput();
    pf_.input().keys.clear();
    const int bw = 420;
    const int bh = 30;
    const int stp = 34;
    const int bx = (WIN_W - bw) / 2;

    while (!done && !quitNow_) {
        beginFrameInput();
        fb_.clear(cWindow);
        textCentered(0, 24, WIN_W, 60, "Операции с цветом", cText);
        uiLabel(0, 62, WIN_W, 22, "Первый и второй цвет берутся из панели справа", cTextDim);
        // Образцы обоих рабочих цветов, чтобы не гадать, что с чем меняется.
        fb_.fillRect(bx, 92, bx + 60, 122, blendOver(m.firstColor, cCheckA));
        fb_.frameRect(bx, 92, bx + 60, 122, cBorder);
        uiLabel(bx + 66, 92, 120, 30, "первый", cText);
        fb_.fillRect(bx + 200, 92, bx + 260, 122, blendOver(m.secondColor, cCheckA));
        fb_.frameRect(bx + 200, 92, bx + 260, 122, cBorder);
        uiLabel(bx + 266, 92, 120, 30, "второй", cText);

        int y = 140;
        if (uiButton(bx, y, bw, bh, "Заменить второй цвет первым по всему тайлу", true)) {
            m.pushUndo();
            const int n = m.replaceColorEverywhere(m.secondColor, m.firstColor);
            m.needRepaint = true;
            done = true;
            notify("Замена цвета", "Заменено пикселей: " + std::to_string(n) + ".");
        }
        y += stp;
        if (uiButton(bx, y, bw, bh, "Свести тайл к цветам палитры", true)) {
            m.pushUndo();
            m.quantizeTileToPalette();
            m.needRepaint = true;
            done = true;
        }
        y += stp;
        if (uiButton(bx, y, bw, bh, "Отразить тайл по горизонтали", true)) {
            m.pushUndo();
            m.flipTileH();
            m.needRepaint = true;
        }
        y += stp;
        if (uiButton(bx, y, bw, bh, "Отразить тайл по вертикали", true)) {
            m.pushUndo();
            m.flipTileV();
            m.needRepaint = true;
        }

        // Отступ — чтобы подпись раздела не наезжала на кнопку над ней.
        y += stp + 20;
        uiLabel(0, y - 22, WIN_W, 20, "Индексная палитра", cTextDim);
        if (uiButton(bx, y, bw, bh,
                     m.indexedOn ? "Индексный режим: включён" : "Индексный режим: выключен",
                     true)) {
            toggleIndexed();
        }
        y += stp;
        if (uiButton(bx, y, bw, bh, "Привязать тайл к палитре", true)) {
            const int n = m.rebindTileToPalette();
            m.needRepaint = true;
            done = true;
            showMessage("Привязка к палитре",
                        "Связано пикселей: " + std::to_string(n) + " из " +
                            std::to_string(m.tile.w * m.tile.h) +
                            ". Остальные не совпали ни с одной ячейкой палитры и правкой ячеек не "
                            "изменятся. Свести "
                            "тайл к цветам палитры можно кнопкой выше — тогда свяжутся все.");
        }

        y += stp + 20;
        uiLabel(0, y - 22, WIN_W, 20, "Буфер обмена", cTextDim);
        if (uiButton(bx, y, bw, bh, "Отразить буфер по горизонтали", m.bufferFilled())) {
            m.bufferFlipH();
            m.applyBufferToSelection();
            m.needRepaint = true;
        }
        y += stp;
        if (uiButton(bx, y, bw, bh, "Отразить буфер по вертикали", m.bufferFilled())) {
            m.bufferFlipV();
            m.applyBufferToSelection();
            m.needRepaint = true;
        }
        y += stp;
        if (uiButton(bx, y, bw, bh, "Повернуть буфер на четверть оборота", m.bufferFilled())) {
            m.bufferRotate90();
            m.needRepaint = true;
        }

        if (uiButton(WIN_W / 2 - 85, WIN_H - 56, 170, 36, "Закрыть", true)) done = true;
        redraw();
        if (takeKey() == KEY_ESC) done = true;
        idle();
    }
    waitMouseRelease();
    m.needRepaint = true;
}

// ---------------------------------------------------------------------------
// Карта: проверка и свойства
// ---------------------------------------------------------------------------

void Editor::showMapReport() {
    const std::vector<std::string> report = m.map.validate();
    std::vector<std::string> p;
    p.push_back("Локация: " + m.map.name);
    p.push_back("Размер: " + std::to_string(m.map.w) + " на " + std::to_string(m.map.h) +
                ", объектов: " + std::to_string(m.map.objects.size()));
    p.emplace_back();
    if (report.empty()) {
        p.emplace_back(
            "Замечаний нет: все объекты стоят на проходимых клетках, проходимая часть локации "
            "связна.");
    } else {
        p.emplace_back("Замечания:");
        p.emplace_back();
        for (const auto& line : report)
            p.push_back(line);
    }
    showTextScreen("Проверка локации", p);
}

void Editor::showMapPropertiesScreen() {
    bool done = false;
    dropInput();
    pf_.input().keys.clear();
    const int bx = 60;
    int nw = m.map.w;
    int nh = m.map.h;

    auto sizeRow = [&](int y, const std::string& caption, int& v) {
        uiLabelLeft(bx, y, 30, caption, cText);
        if (uiButton(bx + 160, y, 44, 30, "−", v > kMapMinSide)) --v;
        uiLabel(bx + 208, y, 70, 30, std::to_string(v), cText);
        if (uiButton(bx + 282, y, 44, 30, "+", v < kMapMaxSide)) ++v;
        if (uiButton(bx + 334, y, 60, 30, "−8", v > kMapMinSide + 7)) v -= 8;
        if (uiButton(bx + 400, y, 60, 30, "+8", v < kMapMaxSide - 7)) v += 8;
    };

    while (!done && !quitNow_) {
        beginFrameInput();
        fb_.clear(cWindow);
        textCentered(0, 24, WIN_W, 60, "Свойства локации", cText);
        uiLabel(0, 62, WIN_W, 22, "Название и размер попадут в файл карты как есть", cTextDim);

        int y = 110;
        uiLabelLeft(bx, y, 30, "Название", cText);
        if (active.modern) {
            // Название правится на месте, а не отдельным экраном (UX-28).
            std::string nm = m.map.name;
            if (uiTextLine(bx + 160, y, WIN_W - bx - 220, 30, nm)) {
                m.pushUndo();
                m.map.name = netTrim(nm);
            }
        } else {
            fb_.fillRoundRect(bx + 160, y, WIN_W - 220, y + 30, 4, 4, cFace);
            uiLabelLeft(bx + 168, y, 30, fitText(m.map.name, WIN_W - 400), cText);
            if (uiButton(WIN_W - 210, y, 150, 30, "Изменить", true)) {
                std::string nm = m.map.name;
                if (askText("Название локации", "Строка name в файле карты", nm, nm)) {
                    m.pushUndo();
                    m.map.name = netTrim(nm);
                }
            }
        }

        y += 56;
        if (active.modern) {
            uiLabelLeft(bx, y, 30, "Размер", cText);
            uiNumber(bx + 160, y, 110, 30, nw, kMapMinSide, kMapMaxSide, 1, 8);
            uiLabel(bx + 270, y, 30, 30, "×", cTextDim);
            uiNumber(bx + 300, y, 110, 30, nh, kMapMinSide, kMapMaxSide, 1, 8);
            y += 42;
        } else {
            sizeRow(y, "Ширина", nw);
            y += 42;
            sizeRow(y, "Высота", nh);
        }
        nw = clampI(nw, kMapMinSide, kMapMaxSide);
        nh = clampI(nh, kMapMinSide, kMapMaxSide);

        y += 50;
        if (nw != m.map.w || nh != m.map.h) {
            uiLabel(0, y, WIN_W, 24, "Обрезка уберёт объекты, оказавшиеся за новой границей",
                    cWarn);
            if (uiButton(WIN_W / 2 - 110, y + 28, 220, 34,
                         "Применить размер " + std::to_string(nw) + "x" + std::to_string(nh),
                         true)) {
                m.pushUndo();
                m.map.resize(nw, nh);
                m.resetSelection();
                m.fitScale();
                m.overDirty = true;
                m.needRepaint = true;
            }
        }

        if (uiButton(WIN_W / 2 - 240, WIN_H - 104, 220, 36, "Найти и заменить…   Ctrl+F", true))
            showMapFindScreen();
        if (uiButton(WIN_W / 2 - 240, WIN_H - 60, 220, 36, "Проверить локацию", true))
            showMapReport();
        if (uiButton(WIN_W / 2 + 20, WIN_H - 60, 220, 36, "Закрыть", true)) done = true;
        redraw();
        if (takeKey() == KEY_ESC) done = true;
        idle();
    }
    waitMouseRelease();
    m.needRepaint = true;
}

}  // namespace tjtm
