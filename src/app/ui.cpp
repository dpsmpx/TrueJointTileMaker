// Мелкие утилиты, ввод и примитивы интерфейса (TJTM.pas: «Мелкие утилиты»,
// «Ввод», «Примитивы интерфейса»).

#include <algorithm>

#include "app/editor.h"
#include "core/fsutil.h"
#include "core/pmath.h"
#include "core/text.h"
#include "core/theme.h"

namespace tjtm {

using namespace theme;

// ---------------------------------------------------------------------------
// Ввод
// ---------------------------------------------------------------------------

/// Короткая пауза с разбором событий. Стоит во всех циклах ожидания:
/// программа не занимает ядро вхолостую, а окно не перестаёт отвечать.
void Editor::idle() {
    pf_.idle();
    checkQuit();
}

/// Снимок щелчка на кадр. Весь кадр — и отрисовка, и попадания — работает
/// с ним, а живой признак снова копит приходящее. Щелчок посреди кадра
/// поэтому не пропадает: он достаётся следующему.
void Editor::beginFrameInput() {
    pf_.pump();
    Input& in = pf_.input();
    frameClick_ = in.clickPending;
    frameClickX_ = in.clickX;
    frameClickY_ = in.clickY;
    frameClickButton_ = in.clickButton;
    in.clickPending = false;
    tipEvaluate();
    checkQuit();
}

/// Забыть и накопленное, и снятое: щелчок, которым открыли экран, не должен
/// тут же нажать кнопку, случайно оказавшуюся под курсором.
void Editor::dropInput() {
    pf_.input().clickPending = false;
    frameClick_ = false;
    numEdit_ = 0;
    numPress_ = 0;
    if (textEditId_ != 0) {
        textEditId_ = 0;
        pf_.stopTextInput();
    }
    // Экраны открываются отсюда: на них — обычная стрелка.
    pf_.setCursor(Platform::CURSOR_ARROW);
}

int Editor::takeKey() {
    Input& in = pf_.input();
    if (in.keys.empty()) return 0;
    const KeyPress kp = in.keys.front();
    in.keys.pop_front();
    lastKey_ = kp;
    const int k = kp.vk;
    return k;
}

int Editor::takeWheel() {
    Input& in = pf_.input();
    const int w = in.wheel;
    in.wheel = 0;
    return w;
}

int Editor::takeWheelX() {
    Input& in = pf_.input();
    const int w = in.wheelX;
    in.wheelX = 0;
    return w;
}

void Editor::waitMouseRelease() {
    while (mousePressed() && !quitNow_)
        idle();
}

void Editor::redraw() {
    drawToast();
    if (!classicUi_) drawTip();
    pf_.present(fb_);
}

void Editor::checkQuit() {
    // Сценарий кончился — закрыться сразу, без вопросов: спросить некого.
    if (pf_.scriptFinished()) {
        quitNow_ = true;
        return;
    }
    Input& in = pf_.input();
    if (!in.quitRequested) return;
    in.quitRequested = false;
    // Крестик окна — тот же «Выход»: с вопросом о несохранённом.
    commandExit();
}

// ---------------------------------------------------------------------------
// Пути
// ---------------------------------------------------------------------------

std::string Editor::resolvePath(const std::string& p) const {
    if (p.empty()) return p;
    return isPathRooted(p) ? p : pathCombine(baseDir_, p);
}

std::string Editor::fileNameOnly(const std::string& p) {
    return p.empty() ? std::string() : pathFileName(p);
}

/// Запоминает файл и его папку в настройках и сразу сохраняет Config.txt.
void Editor::rememberFile(const std::string& p, bool isMap) {
    if (p.empty()) return;
    if (isMap) {
        cfg.lastMapFile = p;
    } else {
        cfg.lastFile = p;
    }
    cfg.lastFolder = pathDirName(p);
    cfg.addRecent(p);
    saveConfig();
}

void Editor::saveConfig() {
    configSave(configPath_, cfg);
}

// ---------------------------------------------------------------------------
// Примитивы интерфейса
// ---------------------------------------------------------------------------

/// Рисует кнопку и сообщает, был ли по ней щелчок в этом кадре.
bool Editor::uiButton(int x, int y, int bw, int bh, const std::string& caption, bool enabled) {
    return uiButtonEx(x, y, bw, bh, caption, enabled, false);
}

bool Editor::uiPrimary(int x, int y, int bw, int bh, const std::string& caption, bool enabled) {
    return uiButtonEx(x, y, bw, bh, caption, enabled, false, Btn::Filled);
}

/// Кнопка-переключатель: включённая обведена цветом выделения и подсвечена,
/// как выбранный инструмент, — чтобы состояние читалось без подписи «вкл».
bool Editor::uiToggle(int x, int y, int bw, int bh, const std::string& caption, bool on) {
    return uiButtonEx(x, y, bw, bh, caption, true, on);
}

bool Editor::uiTab(int x, int y, int bw, int bh, const std::string& caption, bool open) {
    if (!active.modern) {
        // Как в 1.6: открытая — выключенной кнопкой с точкой.
        return uiButton(x, y, bw, bh, open ? "• " + caption : caption, !open);
    }
    const bool over = mouseX() >= x && mouseX() < x + bw && mouseY() >= y && mouseY() < y + bh;
    if (over && !open) fb_.fillRoundRect(x, y, x + bw, y + bh, BTN_R * 2, BTN_R * 2, cHover);
    textCentered(x + 4, y, x + bw - 4, y + bh, fitTextEnd(caption, bw - 8),
                 open ? cAccent : cTextDim);
    fb_.fillRect(x, y + bh - 1, x + bw, y + bh, cBorder);
    if (open) fb_.fillRect(x + 6, y + bh - 3, x + bw - 6, y + bh, cAccent);
    if (frameClick_ && frameClickX_ >= x && frameClickX_ < x + bw && frameClickY_ >= y &&
        frameClickY_ < y + bh) {
        frameClick_ = false;
        return !open && frameClickButton_ == MB_LEFT;
    }
    return false;
}

void Editor::drawButtonModern(int x, int y, int bw, int bh, const std::string& caption,
                              bool enabled, bool on, bool over, bool down, Btn kind) {
    // Цвет поля и подписи по виду, состояние — прозрачным слоем цвета
    // подписи поверх (наведение 8 %, нажатие 10 %), недоступное —
    // приглушено (docs/ui-design-system.md, «Состояния»).
    const theme::Theme& t = active;
    const int r = t.radius * 2;
    bool filled = true;
    Color field = t.back;
    Color content = t.text;
    bool outlined = false;
    switch (kind) {
        case Btn::Filled:
            field = t.accent;
            content = t.onAccent;
            break;
        case Btn::Tonal:
            field = t.accentContainer;
            content = t.onAccentContainer;
            break;
        case Btn::Outlined:
            filled = false;
            outlined = true;
            break;
        case Btn::Text:
            filled = false;
            content = t.accent;
            break;
    }
    if (on && kind != Btn::Filled) {
        filled = true;
        field = t.accentContainer;
        content = t.onAccentContainer;
    }
    if (!enabled) {
        field = argb(31, t.text.r, t.text.g, t.text.b);
        content = argb(97, t.text.r, t.text.g, t.text.b);
    }
    if (filled) fb_.fillRoundRect(x, y, x + bw, y + bh, r, r, field);
    if (enabled && over)
        fb_.fillRoundRect(x, y, x + bw, y + bh, r, r,
                          argb(down ? 26 : 20, content.r, content.g, content.b));
    if (outlined)
        fb_.strokeRoundRect(x, y, x + bw, y + bh, r, r,
                            enabled ? t.outline : argb(31, t.text.r, t.text.g, t.text.b));
    const std::string shown = fitTextEnd(caption, bw - 8);
    textCentered(x + 4, y, x + bw - 4, y + bh, shown, content);
    // Обрезанная подпись — целиком в подсказке.
    if (shown != caption) uiTip(x, y, bw, bh, caption);
}

bool Editor::uiButtonEx(int x, int y, int bw, int bh, const std::string& caption, bool enabled,
                        bool on, Btn kind) {
    const bool over =
        enabled && mouseX() >= x && mouseX() < x + bw && mouseY() >= y && mouseY() < y + bh;
    const bool down = over && mousePressed();

    if (active.modern) {
        drawButtonModern(x, y, bw, bh, caption, enabled, on, over, down, kind);
        return takeButtonClick(x, y, bw, bh, enabled);
    }

    // Мягкая тень на точку ниже отделяет кнопку от фона там, где тихая
    // обводка уже не справляется. У нажатой тени нет: она выглядит
    // вдавленной без сдвига подписи, который сбивал бы прицел.
    if (enabled && !down)
        fb_.fillRoundRect(x + 1, y + 2, x + bw - 1, y + bh + 1, BTN_R, BTN_R, cShadow);

    fb_.fillRoundRect(x, y, x + bw, y + bh, BTN_R, BTN_R, on ? cAccent : cOutline);
    Color face = on ? cHover : cBack;
    if (!enabled || down) {
        face = cFace;
    } else if (over) {
        face = cHover;
    }
    fb_.fillRoundRect(x + 1, y + 1, x + bw - 1, y + bh - 1, BTN_R, BTN_R, face);

    // Под курсором низ обводки набирает цвет: наведение видно и без
    // движения мыши, на снимке экрана тоже.
    if (enabled && over) {
        fb_.fillRoundRect(x, y + bh - 2, x + bw, y + bh, BTN_R, BTN_R, cAccent);
        fb_.fillRoundRect(x + 1, y + bh - 3, x + bw - 1, y + bh - 2, BTN_R, BTN_R, cHover);
    }

    // Подпись шире кнопки обрезается многоточием, а не вылезает за край
    // (UX-32); полный текст покажет подсказка (этап B).
    const std::string shown = classicUi_ ? caption : fitTextEnd(caption, bw - 8);
    textCentered(x + 4, y, x + bw - 4, y + bh, shown, enabled ? cText : cTextDim);
    return takeButtonClick(x, y, bw, bh, enabled);
}

bool Editor::takeButtonClick(int x, int y, int bw, int bh, bool enabled) {
    // Щелчок забирается кнопкой: одно нажатие приводит в действие ровно
    // одну кнопку, даже если экран сменился посреди кадра.
    if (enabled && frameClick_ && frameClickX_ >= x && frameClickX_ < x + bw && frameClickY_ >= y &&
        frameClickY_ < y + bh) {
        frameClick_ = false;
        // Нажимает только левая кнопка мыши: правый щелчок по «Выход» у
        // оригинала тоже выходил, а его ждут как контекстное меню (UX-15).
        return classicUi_ || frameClickButton_ == MB_LEFT;
    }
    return false;
}

bool Editor::uiNumber(int x, int y, int w, int h, int& v, int lo, int hi, int step, int big,
                      const std::string& unit) {
    const int before = v;
    const auto id = reinterpret_cast<std::uintptr_t>(&v);
    Input& in = pf_.input();
    auto inside = [&](int px, int py) { return px >= x && px < x + w && py >= y && py < y + h; };
    const bool shift = isKeyPressed(KEY_SHIFT);
    auto commit = [&]() {
        if (!numText_.empty() && numText_ != "-") v = clampI(std::atoi(numText_.c_str()), lo, hi);
        numEdit_ = 0;
    };
    // Щелчок мимо набираемого поля принимает набранное, но не забирается:
    // он нажимает то, по чему щёлкнули.
    if (numEdit_ == id && frameClick_ && !inside(frameClickX_, frameClickY_)) commit();
    if (frameClick_ && frameClickButton_ == MB_LEFT && inside(frameClickX_, frameClickY_)) {
        frameClick_ = false;
        if (numEdit_ != id) {
            numPress_ = id;
            numPressX_ = frameClickX_;
            numPressV_ = v;
            numDragged_ = false;
        }
    }
    if (numPress_ == id) {
        if (mousePressed()) {
            const int dx = mouseX() - numPressX_;
            if (std::abs(dx) > 3) numDragged_ = true;
            if (numDragged_) v = clampI(numPressV_ + dx / 4 * step, lo, hi);
        } else {
            // Щелчок без протяжки — набор с клавиатуры.
            if (!numDragged_) {
                numEdit_ = id;
                numText_ = std::to_string(v);
                numFresh_ = true;
            }
            numPress_ = 0;
        }
    }
    if (numEdit_ != id && inside(mouseX(), mouseY()) && in.wheel != 0) {
        v = clampI(v + in.wheel * (shift ? big : step), lo, hi);
        in.wheel = 0;
    }
    if (numEdit_ == id) {
        // Набор забирает свои клавиши, чтобы экран их не разобрал: Enter
        // принимает число, а не закрывает экран.
        std::deque<KeyPress> rest;
        for (const KeyPress& k : in.keys) {
            const int d = k.vk >= KEY_0 && k.vk <= KEY_0 + 9         ? k.vk - KEY_0
                          : k.vk >= KEY_NUM0 && k.vk <= KEY_NUM0 + 9 ? k.vk - KEY_NUM0
                                                                     : -1;
            if (d >= 0 && numEdit_ == id) {
                if (numText_ == "0" || numFresh_) numText_.clear();
                if (numText_.size() < 7) numText_ += static_cast<char>('0' + d);
            } else if ((k.vk == KEY_MINUS || k.vk == KEY_NUMMINUS) && lo < 0 && numEdit_ == id) {
                numText_ = numText_.starts_with('-') ? numText_.substr(1) : "-" + numText_;
            } else if (k.vk == KEY_BACK && numEdit_ == id) {
                if (numFresh_) {
                    numText_.clear();
                } else if (!numText_.empty()) {
                    numText_.pop_back();
                }
            } else if ((k.vk == KEY_ENTER || k.vk == KEY_TAB) && numEdit_ == id) {
                commit();
            } else if (k.vk == KEY_ESC && numEdit_ == id) {
                numEdit_ = 0;
            } else if ((k.vk == KEY_UP || k.vk == KEY_DOWN) && numEdit_ == id) {
                commit();
                v = clampI(v + (k.vk == KEY_UP ? 1 : -1) * (k.shift ? big : step), lo, hi);
                numEdit_ = id;
                numText_ = std::to_string(v);
            } else {
                rest.push_back(k);
                continue;
            }
            numFresh_ = false;
        }
        in.keys = std::move(rest);
        in.presses.clear();
    }

    const bool editing = numEdit_ == id;
    const bool over = inside(mouseX(), mouseY());
    const int r = active.radius * 2;
    fb_.fillRoundRect(x, y, x + w, y + h, r, r, active.field);
    fb_.strokeRoundRect(x, y, x + w, y + h, r, r, editing ? cAccent : (over ? cText : cOutline),
                        editing ? 2 : 1);
    if (editing) {
        const int tw = textWidth(numText_);
        const int tx = x + (w - tw) / 2;
        textOut(tx, y + (h - textHeight()) / 2, numText_, cText);
        if ((pf_.milliseconds() / 530) % 2 == 0)
            fb_.fillRect(tx + tw + 1, y + 6, tx + tw + 2, y + h - 6, cText);
    } else {
        textCentered(x, y, x + w, y + h, std::to_string(v) + unit, cText);
    }
    uiTip(x, y, w, h,
          "Щелчок — набрать число, ролик или протяжка — изменить\nShift — шаг " +
              std::to_string(big));
    return v != before;
}

bool Editor::uiTextLine(int x, int y, int w, int h, std::string& value) {
    const auto id = reinterpret_cast<std::uintptr_t>(&value);
    Input& in = pf_.input();
    auto inside = [&](int px, int py) { return px >= x && px < x + w && py >= y && py < y + h; };
    bool committed = false;
    auto finish = [&](bool keep) {
        if (keep) {
            const std::string v = u32ToUtf8(textEdit_.text);
            committed = v != value;
            value = v;
        }
        textEditId_ = 0;
        pf_.stopTextInput();
    };
    if (textEditId_ == id && frameClick_ && !inside(frameClickX_, frameClickY_)) finish(true);
    if (textEditId_ != id && frameClick_ && frameClickButton_ == MB_LEFT &&
        inside(frameClickX_, frameClickY_)) {
        frameClick_ = false;
        textEditId_ = id;
        textEdit_ = TextField{};
        textEdit_.text = utf8ToU32(value);
        textEdit_.caret = textEdit_.text.size();
        textEdit_.anchor = 0;
        pf_.startTextInput(x, y, w, h);
        in.presses.clear();
        in.text.clear();
    }
    if (textEditId_ == id) {
        while (!in.presses.empty()) {
            const KeyPress kp = in.presses.front();
            in.presses.pop_front();
            if (kp.vk == KEY_ENTER) {
                finish(true);
                break;
            }
            if (kp.vk == KEY_ESC) {
                finish(false);
                break;
            }
            editTextField(textEdit_, kp, pf_);
        }
        if (textEditId_ == id && !in.text.empty()) {
            textEdit_.insert(utf8ToU32(in.text));
            in.text.clear();
        }
        // Клавиши поля экран разбирать не должен: Escape — отмена правки, а
        // не закрытие экрана.
        in.keys.clear();
    }
    const bool editing = textEditId_ == id;
    const int r = active.radius * 2;
    fb_.fillRoundRect(x, y, x + w, y + h, r, r, active.field);
    fb_.strokeRoundRect(x, y, x + w, y + h, r, r,
                        editing ? cAccent : (inside(mouseX(), mouseY()) ? cText : cOutline),
                        editing ? 2 : 1);
    const std::string shown = editing ? u32ToUtf8(textEdit_.text) : value;
    const int ty = y + (h - textHeight()) / 2;
    fb_.setClip(gfx::Rect{x + 4, y + 2, x + w - 4, y + h - 2});
    if (editing) {
        auto widthOf = [&](std::size_t n) {
            return textWidth(u32ToUtf8(std::u32string_view(textEdit_.text).substr(0, n)));
        };
        const int scroll = std::max(0, widthOf(textEdit_.caret) - (w - 20));
        const int tx = x + 8 - scroll;
        if (textEdit_.hasSel())
            fb_.fillRect(tx + widthOf(textEdit_.selLo()), ty, tx + widthOf(textEdit_.selHi()),
                         ty + textHeight(), active.fieldSelection);
        textOut(tx, ty, shown, cText);
        if ((pf_.milliseconds() / 530) % 2 == 0) {
            const int cx = tx + widthOf(textEdit_.caret);
            fb_.fillRect(cx, ty, cx + 1, ty + textHeight(), cText);
        }
    } else {
        textOut(x + 8, ty, fitTextEnd(shown, w - 16), cText);
    }
    fb_.resetClip();
    return committed;
}

bool Editor::uiSwitch(int x, int y, int w, const std::string& caption, bool& on) {
    // Дорожка 36×20 справа, подпись слева; щелчок по всей строке.
    constexpr int kTw = 36;
    constexpr int kTh = 20;
    const int h = 30;
    uiLabelLeft(x, y, h, caption, cText);
    const int tx = x + w - kTw;
    const int ty = y + (h - kTh) / 2;
    const bool over = mouseX() >= x && mouseX() < x + w && mouseY() >= y && mouseY() < y + h;
    fb_.fillRoundRect(tx, ty, tx + kTw, ty + kTh, kTh, kTh, on ? cAccent : cFace);
    fb_.strokeRoundRect(tx, ty, tx + kTw, ty + kTh, kTh, kTh, on ? cAccent : cOutline);
    const int d = on ? kTh - 6 : kTh - 10;
    const int cx = on ? tx + kTw - 3 - d : tx + 5;
    const int cy = ty + (kTh - d) / 2;
    fb_.fillRoundRect(cx, cy, cx + d, cy + d, d, d, on ? active.onAccent : cOutline);
    if (over)
        fb_.fillRoundRect(tx - 2, ty - 2, tx + kTw + 2, ty + kTh + 2, kTh + 4, kTh + 4,
                          argb(20, cText.r, cText.g, cText.b));
    if (takeButtonClick(x, y, w, h, true)) {
        on = !on;
        return true;
    }
    return false;
}

bool Editor::uiStepper(int x, int y, const std::string& label, int& v, int lo, int hi, int big,
                       const std::string& unit) {
    const int before = v;
    if (active.modern) {
        // Один элемент «число» вместо четырёх кнопок ±1 и ±big (UX-26):
        // «−» и «+» по краям, Shift с ними — крупный шаг.
        uiLabelLeft(x, y, 30, label, cText);
        const bool shift = isKeyPressed(KEY_SHIFT);
        if (uiButton(x + 130, y, 30, 30, "−", v > lo)) v = std::max(lo, v - (shift ? big : 1));
        uiNumber(x + 164, y, 160, 30, v, lo, hi, 1, big, unit);
        if (uiButton(x + 328, y, 30, 30, "+", v < hi)) v = std::min(hi, v + (shift ? big : 1));
        return v != before;
    }
    uiLabelLeft(x, y, 30, label, cText);
    const std::string b = std::to_string(big);
    if (uiButton(x + 130, y, 44, 30, "−" + b, v > lo)) v = std::max(lo, v - big);
    if (uiButton(x + 178, y, 36, 30, "−1", v > lo)) v = std::max(lo, v - 1);
    fb_.fillRoundRect(x + 218, y, x + 278, y + 30, 6, 6, cFace);
    uiLabel(x + 218, y, 60, 30, std::to_string(v) + unit, cText);
    if (uiButton(x + 282, y, 36, 30, "+1", v < hi)) v = std::min(hi, v + 1);
    if (uiButton(x + 322, y, 44, 30, "+" + b, v < hi)) v = std::min(hi, v + big);
    return v != before;
}

void Editor::uiLabel(int x, int y, int bw, int bh, const std::string& caption, Color c) {
    textCentered(x, y, x + bw, y + bh, caption, c);
}

/// Подпись слева. Оригинал выводил её через TextOut, а тот заливал место
/// под текстом текущим цветом кисти — оставшимся от последней кнопки; за
/// подписями настроек от этого стояли светлые плашки. Здесь — без них.
void Editor::uiLabelLeft(int x, int y, int bh, const std::string& caption, Color c) {
    textOut(x, y + (bh - textHeight()) / 2, caption, c);
}

void Editor::textCentered(int x0, int y0, int x1, int y1, const std::string& s, Color c) {
    font_.drawCentered(fb_, x0, y0, x1, y1, s, c);
}

void Editor::textOut(int x, int y, const std::string& s, Color c) {
    font_.draw(fb_, x, y, s, c);
}

/// Обрезает строку слева до ширины maxW, ставя многоточие.
std::string Editor::fitText(const std::string& s, int maxW) const {
    if (textWidth(s) <= maxW) return s;
    std::u32string u = utf8ToU32(s);
    while (u.size() > 1 && textWidth("…" + u32ToUtf8(u)) > maxW)
        u.erase(0, 1);
    return "…" + u32ToUtf8(u);
}

/// Обрезает строку справа до ширины maxW, ставя многоточие. Строка из
/// нескольких строк (\n) не трогается: её переносит рисование.
std::string Editor::fitTextEnd(const std::string& s, int maxW) const {
    if (s.find('\n') != std::string::npos || textWidth(s) <= maxW) return s;
    std::u32string u = utf8ToU32(s);
    while (u.size() > 1 && textWidth(u32ToUtf8(u) + "…") > maxW)
        u.pop_back();
    return u32ToUtf8(u) + "…";
}

/// Разворачивает абзацы в строки не шире maxWidth, перенося по словам.
std::vector<std::string> Editor::layoutText(const std::vector<std::string>& paragraphs,
                                            int maxWidth) const {
    std::vector<std::string> out;
    for (const std::string& p : paragraphs) {
        const std::vector<std::string> words = splitWords(p);
        if (words.empty()) {
            out.emplace_back();
            continue;
        }
        std::string cur;
        for (const std::string& w : words) {
            std::string probe = cur;
            if (!probe.empty()) probe += ' ';
            probe += w;
            if (!cur.empty() && textWidth(probe) > maxWidth) {
                out.push_back(cur);
                cur = w;
            } else {
                cur = probe;
            }
        }
        out.push_back(cur);
    }
    return out;
}

}  // namespace tjtm
