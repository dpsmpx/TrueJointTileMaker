// Предпросмотр на холсте и диалог — панелью сбоку (план «Интерфейс», C5,
// UX-04, шаг 2). У 1.6 «Повернуть на любой угол» спрашивал градусы на
// сером экране, не показывая картинку, и посмотреть результат можно было,
// только применив его и отменив. Здесь параметры — в карточке у правого
// края холста, а сам холст показывает итог при каждой правке.
//
// Приём общий для любой операции, которая сама кладёт шаг истории:
// параметр сменился — прежний предпросмотр отменяется и операция
// применяется заново; «Отмена» откатывает последний, «Применить»
// оставляет его — одним шагом истории.

#include <algorithm>

#include "app/editor.h"
#include "core/imageops.h"
#include "core/theme.h"

namespace tjtm {

using namespace theme;

bool Editor::previewPanel(const std::string& title, int height,
                          const std::function<bool(int x, int y)>& controls,
                          const std::function<bool()>& apply, const std::string& okText) {
    constexpr int kW = 340;
    bool applied = false;
    bool dirty = true;
    bool done = false;
    bool ok = false;
    gfx::Surface under;
    // Предпросмотр и его отмена документ не меняют: пометка «изменён» —
    // какой была.
    const bool wasModified = m.tileModified;
    dropInput();
    pf_.input().keys.clear();
    while (!done && !quitNow_) {
        // Координаты — главного окна: панель лежит на холсте, а не на
        // странице по середине.
        beginFrameInput(false);
        const int x1 = m.W - 12;
        const int x0 = std::max(m.X0 + 8, x1 - kW);
        const int y0 = m.Y0 + 12;
        const int y1 = y0 + height;
        if (dirty) {
            dirty = false;
            if (applied) m.doUndo();
            applied = apply();
            // Холст с итогом — подложка панели, пока параметры не сменятся.
            drawCanvas();
            drawTimeline();
            drawPanel();
            drawDock();
            drawMenuBar();
            drawOptionsBar();
            drawStatusBar();
            under = fb_;
        }
        fb_ = under;
        fb_.fillRoundRect(x0 + 2, y0 + 3, x1 + 2, y1 + 4, 16, 16, active.shadow);
        fb_.fillRoundRect(x0, y0, x1, y1, 16, 16, cBorder);
        fb_.fillRoundRect(x0 + 1, y0 + 1, x1 - 1, y1 - 1, 14, 14, cFace);
        uiTitle(x0, y0 + 8, x1, y0 + 36, title);
        if (controls(x0 + 16, y0 + 44)) dirty = true;
        if (uiPrimary(x0 + 16, y1 - 48, (kW - 40) / 2, 34, okText, true)) {
            ok = true;
            done = true;
        }
        if (uiButton(x0 + 24 + (kW - 40) / 2, y1 - 48, (kW - 40) / 2, 34, "Отмена", true))
            done = true;
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
    if (!ok && applied) {
        m.doUndo();
        // Отменённый предпросмотр — не шаг, который можно повторить.
        if (m.docMode == DOC_TILE && !m.redoStack.empty()) m.redoStack.pop_back();
        m.tileModified = wasModified;
    }
    m.needRepaint = true;
    return ok && applied;
}

bool Editor::rotatePreviewPanel() {
    int deg = rotateDeg_;
    const bool sel = m.hasSelection;
    auto controls = [&](int x, int y) {
        const int before = deg;
        const int beforeMethod = rotateMethod_;
        const bool beforeExpand = rotateExpand_;
        uiLabelLeft(x, y, 30, "Угол, по часовой", cText);
        y += 32;
        if (uiButton(x, y, 52, 30, "−15", deg > -180)) deg = std::max(-180, deg - 15);
        uiNumber(x + 58, y, 90, 30, deg, -180, 180, 1, 15, "°");
        if (uiButton(x + 154, y, 52, 30, "+15", deg < 180)) deg = std::min(180, deg + 15);
        if (uiButton(x + 212, y, 96, 30, "Без поворота", deg != 0)) deg = 0;
        y += 42;
        uiLabelLeft(x, y, 30, "Способ", cText);
        y += 32;
        if (uiToggle(x, y, 100, 30, "RotSprite", rotateMethod_ == img::ROT_SPRITE))
            rotateMethod_ = img::ROT_SPRITE;
        if (uiToggle(x + 104, y, 100, 30, "Ближайший", rotateMethod_ == img::ROT_NEAREST))
            rotateMethod_ = img::ROT_NEAREST;
        if (uiToggle(x + 208, y, 100, 30, "Плавно", rotateMethod_ == img::ROT_SMOOTH))
            rotateMethod_ = img::ROT_SMOOTH;
        y += 36;
        const char* how = rotateMethod_ == img::ROT_SPRITE ? "Держит линии и контур целыми"
                          : rotateMethod_ == img::ROT_NEAREST ? "Быстро, но рвёт тонкие линии"
                                                              : "Не пиксель-арт: новые цвета";
        uiLabelLeft(x, y, 20, how, rotateMethod_ == img::ROT_SMOOTH ? cWarn : cTextDim);
        if (!sel) {
            y += 30;
            if (uiToggle(x, y, 150, 30, "Вырастить холст", rotateExpand_)) rotateExpand_ = true;
            if (uiToggle(x + 158, y, 150, 30, "Тот же размер", !rotateExpand_))
                rotateExpand_ = false;
        }
        return deg != before || rotateMethod_ != beforeMethod || rotateExpand_ != beforeExpand;
    };
    auto apply = [&]() {
        if (deg == 0) return false;
        m.actionName = "Поворот на " + std::to_string(deg) + "°";
        return m.rotateImageBy(deg, rotateMethod_, rotateExpand_);
    };
    const bool done =
        previewPanel("Поворот на любой угол", sel ? 270 : 304, controls, apply, "Повернуть");
    rotateDeg_ = deg;
    return done;
}

bool Editor::outlinePreviewPanel() {
    auto controls = [&](int x, int y) {
        const bool o = outlineOutside_;
        const bool e = outlineEight_;
        const int t = outlineThick_;
        uiLabelLeft(x, y, 30, "Где", cText);
        if (uiToggle(x + 96, y, 104, 30, "Снаружи", outlineOutside_)) outlineOutside_ = true;
        if (uiToggle(x + 204, y, 104, 30, "Внутри", !outlineOutside_)) outlineOutside_ = false;
        y += 38;
        uiLabelLeft(x, y, 30, "Соседи", cText);
        if (uiToggle(x + 96, y, 104, 30, "Четыре", !outlineEight_)) outlineEight_ = false;
        if (uiToggle(x + 204, y, 104, 30, "Восемь", outlineEight_)) outlineEight_ = true;
        y += 38;
        uiLabelLeft(x, y, 30, "Толщина", cText);
        uiNumber(x + 96, y, 90, 30, outlineThick_, 1, 16, 1, 4);
        y += 38;
        uiLabelLeft(x, y, 30, "Цвет", cText);
        fb_.fillRect(x + 96, y + 2, x + 150, y + 28, blendOver(m.firstColor, cCheckA));
        fb_.frameRect(x + 96, y + 2, x + 150, y + 28, cBorder);
        uiLabelLeft(x + 160, y, 30, "первый рабочий", cTextDim);
        return o != outlineOutside_ || e != outlineEight_ || t != outlineThick_;
    };
    auto apply = [&]() {
        m.actionName = "Контур";
        return m.outlineImage(m.firstColor, outlineEight_, outlineOutside_, outlineThick_) > 0;
    };
    return previewPanel("Контур первым цветом", 260, controls, apply, "Обвести");
}

}  // namespace tjtm
