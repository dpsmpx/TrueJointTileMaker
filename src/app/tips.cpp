// Всплывающие подсказки (план «Интерфейс», этап B, UX-07): у 1.6 их не было
// нигде — значки без названий, «Зам.», «ПП», «Лук», и узнать, что это и
// какая клавиша, можно было только из справки на 250 строк.
//
// Устроено под immediate-mode: за кадр элемент под курсором называет себя
// (uiTip), в начале следующего кадра (tipEvaluate) видно, тот же ли он и
// сколько курсор над ним стоит; через полсекунды подсказка рисуется поверх
// кадра при выводе (redraw). Экраны перерисовываются каждый кадр, главное
// окно — по необходимости, поэтому для него показ и снятие подсказки
// заказывает appStep (editorTipFrame).

#include <algorithm>

#include "app/editor.h"
#include "core/text.h"
#include "core/theme.h"

namespace tjtm {

using namespace theme;

namespace {

/// Через столько курсор, стоящий над элементом, вызывает подсказку.
constexpr int kTipDelayMs = 500;

}  // namespace

void Editor::uiTip(int x, int y, int w, int h, const std::string& text) {
    if (classicUi_ || !cfg.tooltips || text.empty()) return;
    if (mouseX() >= x && mouseX() < x + w && mouseY() >= y && mouseY() < y + h) tipCand_ = text;
}

void Editor::tipEvaluate() {
    if (classicUi_) return;
    const int now = pf_.milliseconds();
    // Щелчок начинает отсчёт заново: подсказка не встаёт поверх только что
    // нажатого.
    if (tipCand_ != tipLast_ || frameClick_) {
        tipLast_ = tipCand_;
        tipSince_ = now;
    }
    const std::string want = !tipLast_.empty() && now - tipSince_ >= kTipDelayMs && !mousePressed()
                                 ? tipLast_
                                 : std::string();
    // Место подсказки — там, где она появилась: курсор, шевелящийся над тем
    // же элементом, её не таскает.
    if (!want.empty() && want != tipWant_) {
        tipX_ = mouseX();
        tipY_ = mouseY();
    }
    tipWant_ = want;
    tipCand_.clear();
}

void Editor::drawTip() {
    if (tipWant_.empty()) return;
    const std::vector<std::string> lines = splitLines(tipWant_);
    int w = 0;
    for (const std::string& l : lines)
        w = std::max(w, textWidth(l));
    w += 16;
    const int lh = textHeight();
    const int h = static_cast<int>(lines.size()) * lh + 10;
    // Под курсором и правее, а у края окна — с другой стороны.
    int x = tipX_ + 14;
    int y = tipY_ + 20;
    if (x + w > WIN_W - 4) x = std::max(4, WIN_W - 4 - w);
    if (y + h > WIN_H - 4) y = std::max(4, tipY_ - h - 8);
    fb_.fillRoundRect(x, y, x + w, y + h, 8, 8, active.inverseSurface);
    for (std::size_t i = 0; i < lines.size(); ++i)
        textOut(x + 8, y + 5 + static_cast<int>(i) * lh, lines[i], active.inverseOnSurface);
}

std::string Editor::toolTip(int i) const {
    if (m.docMode == DOC_MAP) {
        switch (i) {
            case M_PAINT:
                return "Местность · 1\nЛевая — рисовать, правая — взять местность\nПравая по "
                       "кнопке — кисть смесью";
            case M_FILL: return "Заливка местностью · 2\nПравая по кнопке — смесью";
            case M_RECT: return "Прямоугольник местности · 3\nПравая по кнопке — смесью";
            case M_OBJ:
                return "Объекты · 4\nЛевая — поставить или перенести, правая — убрать\nДвойной "
                       "щелчок или Enter — форма объекта";
            default: return "Выделение · 5\nПротяжка — выделить, щелчок — вставить";
        }
    }
    switch (i) {
        case T_PEN:
            return "Карандаш · 1\nЛевая — рисовать, правая или Alt — взять цвет; [ и ] — "
                   "размер\nПравая по кнопке — вид: точки, аэрограф, кисть, затенение";
        case T_ERASER: return "Ластик · 2\nСтирает до прозрачного; [ и ] — размер";
        case T_FILL: return "Заливка · 3\nПравая по кнопке — градиент узором";
        case T_PICK: return "Пипетка · 4\nБерёт цвет из картинки";
        case T_LINE: return "Линия · 5\nПравая по кнопке — параметры фигур";
        case T_RECT: return "Прямоугольник · 6\nCtrl — залитый, Shift — квадрат";
        case T_ELLIPSE: return "Овал · 7\nCtrl — залитый, Shift — круг";
        case T_DITHER: return "Узор · 8\n[ и ] — размер, Shift+[ и ] — сменить узор";
        default:
            return "Выделение · 9\nПротяжка — выделить, по выделенному — перенести\nВид "
                   "выделения — в параметрах (O)";
    }
}

void Editor::registerEditorTips() {
    if (classicUi_ || !cfg.tooltips) return;
    const int px = mouseX();
    const int py = mouseY();
    int idx = 0;
    if (px >= PANEL_X) {
        if (py >= TOOLS_Y0 && py < TOOLS_Y1) {
            if (m.toolHit(px, py, idx)) {
                tipCand_ = toolTip(idx);
            } else if (m.docMode == DOC_TILE) {
                static const char* const kChips[] = {
                    "Замена цвета или смешивание · E",
                    "Пиксель-перфект · Q\nУбирает углы «Г» в мазке; только при размере кисти 1",
                    "Параметры инструментов · O"};
                const int chip = chipHit(px, py);
                if (chip >= 0 && chip < 3) tipCand_ = kChips[chip];
            }
        } else if (py >= OVER_Y0 && py < OVER_Y1) {
            tipCand_ = "Обзор документа\nЩелчок — показать это место, протяжка средней — двигать";
        } else if (py >= MENU_Y0 && py < MENU_Y1) {
            tipCand_ = "Главное меню · Escape";
        } else if (m.docMode == DOC_MAP) {
            if (py >= SLOT1_Y0 && py < SLOT1_Y1 && Model::terrHit(px, py, idx)) {
                tipCand_ = "Местность «" + mapTileName(idx) + "»\nКисть, заливка и прямоугольник";
            } else if (py >= SLOT2_Y0 && py < ALPHA_Y1 && Model::objHit(px, py, idx)) {
                tipCand_ =
                    "Вид объекта «" + mapKindName(idx) + "»\nИнструмент объектов (4) ставит его";
            }
        } else if (py >= PAL_Y0 && py < PAL_Y1) {
            tipCand_ =
                "Палитра\nЛевая — первый цвет, правая — второй, Ctrl+щелчок — записать\nU "
                "или средняя — экран палитры, I — индексный режим";
        } else if (py >= SLOT2_Y0 && py < SLOT2_Y0 + 64 && px < PANEL_X + 36) {
            tipCand_ = "Первый и второй цвет · X — поменять";
        } else if (py >= HSV_Y0 && py < HSV_Y1 && px >= HSV_X0) {
            tipCand_ = "Насыщенность и яркость\nP или средняя — точный выбор цвета";
        } else if (py >= HUE_Y0 && py < HUE_Y1) {
            tipCand_ = "Тон первого цвета";
        } else if (py >= ALPHA_Y0 && py < ALPHA_Y1) {
            tipCand_ = "Непрозрачность первого цвета";
        }
        return;
    }
    if (timelineShown() && py >= m.H) {
        const int b = timelineButtonAt(px, py);
        static const char* const kTb[] = {"Проиграть анимацию · Enter",
                                          "Новый кадр",
                                          "Копия кадра",
                                          "Удалить кадр",
                                          "Новый слой",
                                          "Удалить слой",
                                          "Луковая кожа · F3\nСоседние кадры — полупрозрачно",
                                          "Метка анимации\nКадры, которые проигрываются вместе",
                                          "Слои, кадры и экспорт"};
        if (b >= 0 && b < static_cast<int>(std::size(kTb))) tipCand_ = kTb[b];
    }
}

void Editor::editorTipFrame() {
    if (classicUi_) return;
    registerEditorTips();
    if (tipWant_ == tipShown_) return;
    // Снятая подсказка стирается полной перерисовкой, новая рисуется
    // поверх кадра при выводе.
    if (!tipShown_.empty()) m.needRepaint = true;
    tipShown_ = tipWant_;
    if (!tipShown_.empty() && !m.needRepaint) redraw();
}

}  // namespace tjtm
