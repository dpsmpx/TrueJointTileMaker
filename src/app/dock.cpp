// Боковая панель — док между холстом и панелью инструментов (план
// «Интерфейс», C3, UX-25, UX-39, UX-41). У 1.6 слои правили на экране из
// сорока кнопок или на ленте кадров, а историю не видел никто: сколько
// шагов можно отменить и что за ними — оставалось догадкой. В доке —
// разделы «Слои» (глаз, замок, имя, выбор щелчком, порядок протяжкой) и
// «История» (названия шагов, щелчок — перейти к шагу). Заголовок раздела
// сворачивает его.
//
// Док — в новых темах, когда окно шире 1010 точек: холст остаётся не уже
// 600, как у 1.6. Включается в меню «Вид».

#include <algorithm>

#include "app/editor.h"
#include "core/theme.h"

namespace tjtm {

using namespace theme;

namespace {

constexpr int kDockW = 220;
constexpr int kHeadH = 26;
constexpr int kLayerH = 26;
constexpr int kHistH = 22;

}  // namespace

void Editor::applyDock() {
    const bool on =
        !classicUi_ && active.modern && cfg.dock && gLayout.w - PANEL_W - kDockW >= CANVAS_W;
    const int want = on ? kDockW : 0;
    if (gLayout.dock != want) {
        gLayout.dock = want;
        m.needRepaint = true;
    }
}

Editor::DockLayout Editor::dockLayout() const {
    DockLayout d;
    d.x0 = panelX() - gLayout.dock;
    d.x1 = panelX();
    const int y0 = gLayout.top;
    const int y1 = gLayout.h - gLayout.bottom;
    const bool layers = m.docMode == DOC_TILE;
    // Слои — сверху, история — под ними; открытые делят высоту поровну.
    d.layersHead = y0;
    int y = y0;
    if (layers) {
        y += kHeadH;
        const bool both = dockLayersOpen_ && dockHistoryOpen_;
        const int body = y1 - y0 - 2 * kHeadH;
        d.layersY0 = y;
        d.layersY1 = dockLayersOpen_ ? y + (both ? body / 2 : body) : y;
        y = d.layersY1;
    }
    d.historyHead = y;
    d.historyY0 = y + kHeadH;
    d.historyY1 = dockHistoryOpen_ ? y1 : d.historyY0;
    return d;
}

void Editor::drawDock() {
    if (gLayout.dock == 0) return;
    const DockLayout d = dockLayout();
    const int y1 = gLayout.h - gLayout.bottom;
    fb_.fillRect(d.x0, gLayout.top, d.x1, y1, cPanel);
    fb_.fillRect(d.x0, gLayout.top, d.x0 + 1, y1, cBorder);
    auto header = [&](int y, const char* title, bool open) {
        fb_.fillRect(d.x0 + 1, y, d.x1, y + kHeadH, cFace);
        fb_.fillRect(d.x0 + 1, y + kHeadH - 1, d.x1, y + kHeadH, cBorder);
        // Треугольник: вниз — раздел открыт, вправо — свёрнут.
        const int cx = d.x0 + 12;
        const int cy = y + kHeadH / 2;
        for (int k = 0; k < 4; ++k) {
            if (open) {
                fb_.fillRect(cx - 3 + k, cy - 2 + k, cx + 4 - k, cy - 1 + k, cText);
            } else {
                fb_.fillRect(cx - 2 + k, cy - 3 + k, cx - 1 + k, cy + 4 - k, cText);
            }
        }
        textOut(d.x0 + 24, y + (kHeadH - textHeight()) / 2, title, cText);
    };

    if (m.docMode == DOC_TILE) {
        header(d.layersHead, "Слои", dockLayersOpen_);
        fb_.setClip(gfx::Rect{d.x0 + 1, d.layersY0, d.x1, d.layersY1});
        const std::vector<int> rows = timelineRows();
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const int l = rows[i];
            const Layer& layer = m.doc.layers[static_cast<std::size_t>(l)];
            const int y = d.layersY0 + static_cast<int>(i) * kLayerH;
            if (y >= d.layersY1) break;
            if (l == m.activeLayer)
                fb_.fillRect(d.x0 + 1, y, d.x1, y + kLayerH, active.accentContainer);
            const Color tc = l == m.activeLayer ? active.onAccentContainer : cText;
            const int ty = y + (kLayerH - textHeight()) / 2;
            // Глаз и замок — знаками: видно, что включено, без экрана слоёв.
            textOut(d.x0 + 8, ty, layer.visible ? "●" : "○", layer.visible ? tc : cTextDim);
            if (layer.locked) {
                fb_.frameRect(d.x0 + 27, ty + 6, d.x0 + 36, ty + 13, tc);
                fb_.frameRect(d.x0 + 29, ty + 2, d.x0 + 34, ty + 7, tc);
            }
            const int nx = d.x0 + 44 + std::min(layer.depth, 6) * 10;
            drawLayerKindMark(nx, y + kLayerH / 2, layer);
            textOut(nx + 14, ty, fitTextEnd(layer.name, d.x1 - nx - 20),
                    layer.visible ? tc : cTextDim);
            fb_.fillRect(d.x0 + 1, y + kLayerH - 1, d.x1, y + kLayerH, cLine);
        }
        fb_.resetClip();
    }

    header(d.historyHead, "История", dockHistoryOpen_);
    if (d.historyY1 > d.historyY0) {
        fb_.setClip(gfx::Rect{d.x0 + 1, d.historyY0, d.x1, d.historyY1});
        int done = 0;
        const std::vector<std::string> steps = m.historyLabels(done);
        // Строка 0 — состояние до первого шага; текущая — всегда на виду.
        const int n = static_cast<int>(steps.size()) + 1;
        const int fit = std::max(1, (d.historyY1 - d.historyY0) / kHistH);
        const int first = std::clamp(done - fit + 2, 0, std::max(0, n - fit));
        for (int k = first; k < n && k < first + fit; ++k) {
            const int y = d.historyY0 + (k - first) * kHistH;
            const bool cur = k == done;
            if (cur) fb_.fillRect(d.x0 + 1, y, d.x1, y + kHistH, active.accentContainer);
            const std::string text = k == 0 ? "Начало" : steps[static_cast<std::size_t>(k - 1)];
            // Отменённое — бледно: его ещё можно повторить.
            const Color c = cur ? active.onAccentContainer : (k > done ? cTextDim : cText);
            textOut(d.x0 + 12, y + (kHistH - textHeight()) / 2, fitTextEnd(text, d.x1 - d.x0 - 20),
                    c);
        }
        fb_.resetClip();
    }
}

void Editor::handleDockClick() {
    const DockLayout d = dockLayout();
    const int x = frameClickX_;
    const int y = frameClickY_;
    if (m.docMode == DOC_TILE && y >= d.layersHead && y < d.layersHead + kHeadH) {
        dockLayersOpen_ = !dockLayersOpen_;
        m.needRepaint = true;
        return;
    }
    if (y >= d.historyHead && y < d.historyHead + kHeadH) {
        dockHistoryOpen_ = !dockHistoryOpen_;
        m.needRepaint = true;
        return;
    }
    if (m.docMode == DOC_TILE && y >= d.layersY0 && y < d.layersY1) {
        const std::vector<int> rows = timelineRows();
        const int row = (y - d.layersY0) / kLayerH;
        if (row < 0 || row >= static_cast<int>(rows.size())) return;
        const int l = rows[static_cast<std::size_t>(row)];
        const Layer& layer = m.doc.layers[static_cast<std::size_t>(l)];
        if (x < d.x0 + 24) {
            m.actionName = "Видимость слоя";
            m.setLayerVisible(l, !layer.visible);
            return;
        }
        if (x < d.x0 + 40) {
            m.setLayerLocked(l, !layer.locked);
            return;
        }
        stopPlayback();
        const int now = pf_.milliseconds();
        const bool dbl = l == tlLastClickLayer_ && now - tlLastClickMs_ <= DOUBLE_CLICK_MS;
        tlLastClickMs_ = dbl ? -100000 : now;
        tlLastClickLayer_ = l;
        m.setActiveCel(l, m.activeFrame);
        if (dbl || frameClickButton_ == MB_RIGHT) {
            renameLayerAsk(l);
            return;
        }
        // Протяжка по строкам — порядок слоёв (UX-41): слой идёт за мышью
        // среди соседей своей глубины, шаг за шагом.
        m.actionName = "Порядок слоёв";
        while (mousePressed() && !quitNow_) {
            const std::vector<int> order = timelineRows();
            const auto at = std::find(order.begin(), order.end(), m.activeLayer);
            const int cur = static_cast<int>(at - order.begin());
            const int want = std::clamp((mouseY() - d.layersY0) / kLayerH, 0,
                                        static_cast<int>(order.size()) - 1);
            // Строки — сверху вниз, слои — снизу вверх.
            const int dir = want < cur ? 1 : (want > cur ? -1 : 0);
            if (dir != 0 && m.moveLayer(dir)) repaint();
            idle();
        }
        m.needRepaint = true;
        return;
    }
    if (y >= d.historyY0 && y < d.historyY1) {
        int done = 0;
        const int n = static_cast<int>(m.historyLabels(done).size()) + 1;
        const int fit = std::max(1, (d.historyY1 - d.historyY0) / kHistH);
        const int first = std::clamp(done - fit + 2, 0, std::max(0, n - fit));
        const int k = first + (y - d.historyY0) / kHistH;
        if (k < n) {
            m.historyGoTo(k);
            m.needRepaint = true;
        }
    }
}

}  // namespace tjtm
