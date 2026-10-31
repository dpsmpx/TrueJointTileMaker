// Мелкие утилиты, ввод и примитивы интерфейса (TJTM.pas: «Мелкие утилиты»,
// «Ввод», «Примитивы интерфейса»).

#include <algorithm>

#include "app/editor.h"
#include "core/fsutil.h"
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
    checkQuit();
}

/// Забыть и накопленное, и снятое: щелчок, которым открыли экран, не должен
/// тут же нажать кнопку, случайно оказавшуюся под курсором.
void Editor::dropInput() {
    pf_.input().clickPending = false;
    frameClick_ = false;
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

/// Кнопка-переключатель: включённая обведена цветом выделения и подсвечена,
/// как выбранный инструмент, — чтобы состояние читалось без подписи «вкл».
bool Editor::uiToggle(int x, int y, int bw, int bh, const std::string& caption, bool on) {
    return uiButtonEx(x, y, bw, bh, caption, true, on);
}

bool Editor::uiButtonEx(int x, int y, int bw, int bh, const std::string& caption, bool enabled,
                        bool on) {
    const bool over =
        enabled && mouseX() >= x && mouseX() < x + bw && mouseY() >= y && mouseY() < y + bh;
    const bool down = over && mousePressed();

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

bool Editor::uiStepper(int x, int y, const std::string& label, int& v, int lo, int hi, int big,
                       const std::string& unit) {
    const int before = v;
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
