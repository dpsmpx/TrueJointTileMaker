// Модальные экраны (TJTM.pas: «Модальные экраны», «Ввод строки»,
// «Операции над цветом всего тайла», «Свойства карты»).
//
// Каждый экран — свой цикл: снимок ввода, отрисовка, разбор нажатий,
// пауза. Так было в оригинале, и так перенос остаётся сличимым с ним.

#include <algorithm>

#include "app/editor.h"
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

void Editor::showMessage(const std::string& title, const std::string& text) {
    showTextScreen(title, {text});
}

/// Вопрос с тремя вариантами: 1 — да, 0 — нет, -1 — отмена.
int Editor::ask3(const std::string& title, const std::string& text, const std::string& yes,
                 const std::string& no, const std::string& cancel) {
    int res = -1;
    bool done = false;
    dropInput();
    pf_.input().keys.clear();
    const int H = WIN_H;
    while (!done && !quitNow_) {
        beginFrameInput();
        fb_.clear(cWindow);
        textCentered(0, H / 2 - 90, WIN_W, H / 2 - 50, title, cText);
        textCentered(40, H / 2 - 46, WIN_W - 40, H / 2 + 10, text, cText);
        if (uiButton(WIN_W / 2 - 270, H / 2 + 30, 170, 34, yes, true)) {
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
        redraw();
        if (takeKey() == KEY_ESC) {
            res = -1;
            done = true;
        }
        idle();
    }
    waitMouseRelease();
    m.needRepaint = true;
    return res;
}

/// Вопрос с двумя ответами. Escape равнозначен отказу.
bool Editor::askYesNo(const std::string& title, const std::string& text, const std::string& yes,
                      const std::string& no) {
    bool res = false;
    bool done = false;
    dropInput();
    pf_.input().keys.clear();
    const int H = WIN_H;
    while (!done && !quitNow_) {
        beginFrameInput();
        fb_.clear(cWindow);
        textCentered(0, H / 2 - 90, WIN_W, H / 2 - 50, title, cText);
        textCentered(40, H / 2 - 46, WIN_W - 40, H / 2 + 10, text, cText);
        if (uiButton(WIN_W / 2 - 180, H / 2 + 30, 170, 34, yes, true)) {
            res = true;
            done = true;
        }
        if (uiButton(WIN_W / 2 + 10, H / 2 + 30, 170, 34, no, true)) {
            res = false;
            done = true;
        }
        redraw();
        if (takeKey() == KEY_ESC) {
            res = false;
            done = true;
        }
        idle();
    }
    waitMouseRelease();
    m.needRepaint = true;
    return res;
}

/// Экран размера нового документа. Оригинал звал его и для карты — с
/// заголовком «Создать новый тайл» и пределами тайла 4..512, хотя карта
/// не бывает больше 256; здесь у каждого режима свои заголовок и пределы.
bool Editor::askNewSize(const std::string& title, int& nw, int& nh, int lo, int hi) {
    static const int kPresets[] = {8, 16, 32, 64, 128, 256};
    nw = clampI(nw, lo, hi);
    nh = clampI(nh, lo, hi);
    bool done = false;
    bool ok = false;
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

namespace {

/// Однострочное поле: каретка, выделение мышью и Shift, слова по Ctrl,
/// буфер обмена, раскладки и IME. Оригинал ради этого клал поверх окна
/// настоящий TextBox: строки объектов карты длинные, и их переносят между
/// локациями целиком.
struct TextField {
    std::u32string text;
    std::size_t caret = 0;
    std::size_t anchor = 0;  // начало выделения; == caret — выделения нет
    int scroll = 0;

    [[nodiscard]] bool hasSel() const { return anchor != caret; }
    [[nodiscard]] std::size_t selLo() const { return std::min(anchor, caret); }
    [[nodiscard]] std::size_t selHi() const { return std::max(anchor, caret); }

    void eraseSel() {
        if (!hasSel()) return;
        text.erase(selLo(), selHi() - selLo());
        caret = anchor = selLo();
    }
    void insert(const std::u32string& s) {
        eraseSel();
        std::u32string clean;
        for (char32_t c : s) {
            if (c != U'\n' && c != U'\r') clean.push_back(c == U'\t' ? U' ' : c);
        }
        text.insert(caret, clean);
        caret += clean.size();
        anchor = caret;
    }
    static bool isWord(char32_t c) { return c != U' '; }
    [[nodiscard]] std::size_t wordLeft(std::size_t p) const {
        while (p > 0 && !isWord(text[p - 1]))
            --p;
        while (p > 0 && isWord(text[p - 1]))
            --p;
        return p;
    }
    [[nodiscard]] std::size_t wordRight(std::size_t p) const {
        while (p < text.size() && isWord(text[p]))
            ++p;
        while (p < text.size() && !isWord(text[p]))
            ++p;
        return p;
    }
};

}  // namespace

/// Ввод строки. true — подтверждено, значение в value.
bool Editor::askText(const std::string& title, const std::string& prompt,
                     const std::string& initial, std::string& value) {
    bool done = false;
    bool ok = false;
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
            const bool shift = kp.shift;
            auto moveTo = [&](std::size_t p) {
                tf.caret = p;
                if (!shift) tf.anchor = p;
            };
            switch (kp.vk) {
                case KEY_ENTER:
                    ok = true;
                    done = true;
                    break;
                case KEY_ESC: done = true; break;
                case KEY_LEFT:
                    if (!shift && tf.hasSel() && !kp.ctrl) {
                        moveTo(tf.selLo());
                    } else {
                        moveTo(kp.ctrl ? tf.wordLeft(tf.caret) : (tf.caret > 0 ? tf.caret - 1 : 0));
                    }
                    break;
                case KEY_RIGHT:
                    if (!shift && tf.hasSel() && !kp.ctrl) {
                        moveTo(tf.selHi());
                    } else {
                        moveTo(kp.ctrl ? tf.wordRight(tf.caret)
                                       : std::min(tf.text.size(), tf.caret + 1));
                    }
                    break;
                case KEY_HOME: moveTo(0); break;
                case KEY_END: moveTo(tf.text.size()); break;
                case KEY_BACK:
                    if (tf.hasSel()) {
                        tf.eraseSel();
                    } else if (tf.caret > 0) {
                        const std::size_t from = kp.ctrl ? tf.wordLeft(tf.caret) : tf.caret - 1;
                        tf.text.erase(from, tf.caret - from);
                        tf.caret = tf.anchor = from;
                    }
                    break;
                case KEY_DELETE:
                    if (tf.hasSel()) {
                        tf.eraseSel();
                    } else if (tf.caret < tf.text.size()) {
                        const std::size_t to = kp.ctrl ? tf.wordRight(tf.caret) : tf.caret + 1;
                        tf.text.erase(tf.caret, to - tf.caret);
                    }
                    break;
                case KEY_A:
                    if (kp.ctrl) {
                        tf.anchor = 0;
                        tf.caret = tf.text.size();
                    }
                    break;
                case KEY_C:
                case KEY_X:
                    if (kp.ctrl && tf.hasSel()) {
                        pf_.setClipboardText(
                            u32ToUtf8(tf.text.substr(tf.selLo(), tf.selHi() - tf.selLo())));
                        if (kp.vk == KEY_X) tf.eraseSel();
                    }
                    break;
                case KEY_V:
                    if (kp.ctrl) tf.insert(utf8ToU32(pf_.clipboardText()));
                    break;
                case KEY_INSERT:
                    if (kp.shift) tf.insert(utf8ToU32(pf_.clipboardText()));
                    break;
                default: break;
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

        fb_.clear(cWindow);
        textCentered(0, fy - 110, WIN_W, fy - 74, title, cText);
        uiLabel(0, fy - 70, WIN_W, 24, prompt, cTextDim);
        uiLabel(0, fy + 34, WIN_W, 20,
                "Enter — готово, Escape — отмена. Работают выделение и буфер обмена", cTextDim);

        fb_.fillRect(fx, fy, fx + fw, fy + fh, cAccent);
        fb_.fillRect(fx + 1, fy + 1, fx + fw - 1, fy + fh - 1, rgb(255, 255, 255));
        fb_.setClip(gfx::Rect{fx + 4, fy + 2, fx + fw - 4, fy + fh - 2});
        const int tx = fx + 6 - tf.scroll;
        const int ty = fy + (fh - mono_.textHeight()) / 2;
        if (tf.hasSel()) {
            fb_.fillRect(tx + width(tf.selLo()), ty, tx + width(tf.selHi()),
                         ty + mono_.textHeight(), rgb(173, 214, 255));
        }
        mono_.draw(fb_, tx, ty, u32ToUtf8(tf.text), cText);
        if (((pf_.milliseconds() - blinkStart) / 530) % 2 == 0)
            fb_.fillRect(tx + cx, ty, tx + cx + 1, ty + mono_.textHeight(), cText);
        fb_.resetClip();

        if (uiButton(WIN_W / 2 - 180, fy + 60, 170, 36, "Готово", true)) {
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
            showMessage("Замена цвета", "Заменено пикселей: " + std::to_string(n) + ".");
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
        fb_.fillRoundRect(bx + 160, y, WIN_W - 220, y + 30, 4, 4, cFace);
        uiLabelLeft(bx + 168, y, 30, fitText(m.map.name, WIN_W - 400), cText);
        if (uiButton(WIN_W - 210, y, 150, 30, "Изменить", true)) {
            std::string nm = m.map.name;
            if (askText("Название локации", "Строка name в файле карты", nm, nm)) {
                m.pushUndo();
                m.map.name = netTrim(nm);
            }
        }

        y += 56;
        sizeRow(y, "Ширина", nw);
        y += 42;
        sizeRow(y, "Высота", nh);
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
