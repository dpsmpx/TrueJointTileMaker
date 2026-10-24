// Этап 2 RoadToProLevel в окне: экран палитры, файлы палитр, включение
// строгого индексного режима.

#include <algorithm>
#include <cstdio>

#include "app/editor.h"
#include "core/fsutil.h"
#include "core/palette.h"
#include "core/pmath.h"
#include "core/text.h"
#include "core/theme.h"

namespace tjtm {

using namespace theme;

namespace {

const std::vector<FileFilter> kPaletteFilter = {
    {"Палитры (PNG, GIMP, HEX, JASC)", "png;gpl;hex;pal"},
    {"Все файлы", "*"},
};

std::string hexOf(Color c) {
    char buf[16];
    std::snprintf(buf, sizeof buf, "#%02X%02X%02X", c.r, c.g, c.b);
    return buf;
}

}  // namespace

// ---------------------------------------------------------------------------
// Индексный режим
// ---------------------------------------------------------------------------

void Editor::toggleIndexed() {
    if (m.docMode != DOC_TILE) return;
    if (m.indexedOn) {
        m.indexedOn = false;
        m.needRepaint = true;
        return;
    }
    // Строгий режим: каждая точка получит номер. Цвета не из палитры надо
    // либо свести к ближайшим, либо добавить в палитру — решать человеку.
    const int unmatched = m.unmatchedPixelCount();
    bool addMissing = false;
    if (unmatched > 0) {
        const int a = ask3("Индексный режим",
                           "Точек, чей цвет не совпадает ни с одной ячейкой палитры: " +
                               std::to_string(unmatched) +
                               ". Свести их к ближайшим цветам палитры или добавить их цвета в "
                               "палитру?",
                           "Свести", "Добавить цвета", "Отмена");
        if (a == 2) return;
        addMissing = a == 1;
    }
    m.enableIndexed(addMissing);
    m.needRepaint = true;
}

// ---------------------------------------------------------------------------
// Файлы палитр
// ---------------------------------------------------------------------------

bool Editor::loadPaletteFile(const std::string& path, bool quiet, bool merge) {
    std::vector<Color> p;
    const std::string ext = pathExtLower(path);
    if (ext == ".gpl" || ext == ".hex" || ext == ".pal") {
        std::string error;
        const auto text = readFile(path);
        if (!text) {
            if (!quiet) showMessage("Палитра не прочитана", "Не открыть файл " + path);
            return false;
        }
        const bool ok = ext == ".gpl"   ? pal::parseGpl(*text, p, error)
                        : ext == ".hex" ? pal::parseHex(*text, p, error)
                                        : pal::parseJasc(*text, p, error);
        if (!ok) {
            if (!quiet) showMessage("Палитра не прочитана", error);
            return false;
        }
    } else {
        Grid<Color> g;
        if (!readImageGrid(path, g, quiet)) return false;
        p = pal::fromImage(g);
    }
    if (p.empty()) return false;
    if (merge) {
        // Добавляются только цвета, которых в палитре ещё нет.
        std::vector<Color> np = m.palette;
        for (const Color c : p) {
            if (static_cast<int>(np.size()) >= pal::MAX_PALETTE) break;
            if (std::find(np.begin(), np.end(), c) == np.end()) np.push_back(c);
        }
        p = std::move(np);
    }
    if (m.docMode == DOC_TILE && !m.tile.empty()) m.pushUndo();
    m.setPalette(std::move(p));
    return true;
}

bool Editor::savePaletteFile(const std::string& path) {
    const std::string ext = pathExtLower(path);
    bool ok = false;
    if (ext == ".gpl") {
        ok = writeFile(path, pal::writeGpl(m.palette, pathFileName(path)));
    } else if (ext == ".hex") {
        ok = writeFile(path, pal::writeHex(m.palette));
    } else if (ext == ".pal") {
        ok = writeFile(path, pal::writeJasc(m.palette));
    } else {
        // Картинка — по точке на ячейку: ровными рядами по восемь, если
        // делится, иначе полосой. Так её прочтёт и редактор, и Lospec.
        const int n = m.palCount();
        Grid<Color> g = n % PAL_COLS == 0 ? Grid<Color>(PAL_COLS, n / PAL_COLS) : Grid<Color>(n, 1);
        for (int k = 0; k < n; ++k)
            g.d[static_cast<std::size_t>(k)] = m.palette[static_cast<std::size_t>(k)];
        ok = saveGridTo(g, path);
        return ok;
    }
    if (!ok) showMessage("Палитра не сохранена", "Не записать файл " + path);
    return ok;
}

// ---------------------------------------------------------------------------
// Экран палитры
// ---------------------------------------------------------------------------

void Editor::showPaletteScreen() {
    if (m.docMode != DOC_TILE) return;
    bool done = false;
    dropInput();
    pf_.input().keys.clear();
    constexpr int GX = 24;
    constexpr int GY = 44;
    constexpr int CELL = 22;
    constexpr int COLS = 16;
    constexpr int RX = 400;
    constexpr int BW = 170;
    constexpr int BH = 28;
    palSelA_ = clampI(palSelA_, 0, m.palCount() - 1);
    palSelB_ = clampI(palSelB_, 0, m.palCount() - 1);

    while (!done && !quitNow_) {
        beginFrameInput();
        fb_.clear(cWindow);
        const int n = m.palCount();
        palSelA_ = clampI(palSelA_, 0, n - 1);
        palSelB_ = clampI(palSelB_, 0, n - 1);
        const int lo = std::min(palSelA_, palSelB_);
        const int hi = std::max(palSelA_, palSelB_);
        textCentered(0, 8, WIN_W, 34,
                     "Палитра — " + std::to_string(n) + " " +
                         (m.indexedOn ? "· индексный режим" : "· свободные цвета"),
                     cText);

        // --- сетка ячеек ---
        for (int k = 0; k < pal::MAX_PALETTE; ++k) {
            const int x0 = GX + (k % COLS) * CELL;
            const int y0 = GY + (k / COLS) * CELL;
            if (k >= n) {
                fb_.fillRect(x0, y0, x0 + CELL - 1, y0 + CELL - 1, cFace);
                if (k == n) textCentered(x0, y0, x0 + CELL - 1, y0 + CELL - 1, "+", cTextDim);
                continue;
            }
            const Color c = m.palette[static_cast<std::size_t>(k)];
            fb_.fillRect(x0, y0, x0 + CELL - 1, y0 + CELL - 1,
                         blendOver(c, m.checkerColor(k % 2, k / COLS)));
            if (k == m.transparentIdx) {
                for (int q = 0; q < CELL - 1; q += 5) {
                    for (int r = 0; r < CELL - 1; r += 5)
                        fb_.fillRect(x0 + q, y0 + r, std::min(x0 + q + 5, x0 + CELL - 1),
                                     std::min(y0 + r + 5, y0 + CELL - 1),
                                     ((q + r) / 5) % 2 == 0 ? cCheckA : cCheckB);
                }
                fb_.fillRect(x0, y0, x0 + 8, y0 + 8, argb(255, c.r, c.g, c.b));
            }
            if (k >= lo && k <= hi) {
                fb_.frameRect(x0 - 1, y0 - 1, x0 + CELL, y0 + CELL, cAccent);
                fb_.frameRect(x0, y0, x0 + CELL - 1, y0 + CELL - 1, cAccent);
            }
            if (k == m.firstIdx)
                fb_.frameRect(x0 + 2, y0 + 2, x0 + CELL - 3, y0 + CELL - 3, rgb(255, 255, 255));
        }
        // Щелчки по сетке: левая — выбрать (с Shift — до неё), правая —
        // взять цветом, левая по «+» — добавить первый цвет.
        if (frameClick_ && frameClickX_ >= GX && frameClickX_ < GX + COLS * CELL &&
            frameClickY_ >= GY && frameClickY_ < GY + 16 * CELL) {
            const int k = (frameClickY_ - GY) / CELL * COLS + (frameClickX_ - GX) / CELL;
            frameClick_ = false;
            if (k < n) {
                if (frameClickButton_ == MB_RIGHT) {
                    m.pickColor(m.effectiveColor(static_cast<PalIndex>(k)));
                    m.firstIdx = static_cast<PalIndex>(k);
                } else if (isKeyPressed(KEY_SHIFT)) {
                    palSelB_ = k;
                } else {
                    palSelA_ = palSelB_ = k;
                }
            } else if (k == n && frameClickButton_ == MB_LEFT) {
                const int at = m.addPaletteColor(m.firstColor, -1);
                if (at >= 0) palSelA_ = palSelB_ = at;
            }
        }
        const Color cur = m.palette[static_cast<std::size_t>(palSelA_)];
        std::string info = "Ячейка " + std::to_string(palSelA_) + "   " + hexOf(cur);
        if (cur.a < 255) info += "   α" + std::to_string(cur.a);
        if (hi > lo) info += "   ·   выбрано " + std::to_string(lo) + "–" + std::to_string(hi);
        if (m.transparentIdx != IDX_NONE)
            info += "   ·   прозрачный " + std::to_string(m.transparentIdx);
        uiLabelLeft(GX, GY + 16 * CELL + 6, 22, info, cText);
        uiLabelLeft(GX, GY + 16 * CELL + 28, 20,
                    "Левая — выбрать, с Shift — до неё; правая — взять цветом.", cTextDim);

        const int by = GY + 16 * CELL + 56;
        if (uiToggle(GX, by, BW, BH, "Индексный режим  (I)", m.indexedOn)) toggleIndexed();
        const bool isT = m.transparentIdx == palSelA_;
        if (uiToggle(GX + BW + 8, by, BW + 8, BH, isT ? "Прозрачный: снять" : "Сделать прозрачным",
                     isT))
            m.setTransparentIndex(isT ? IDX_NONE : static_cast<PalIndex>(palSelA_));

        // --- ячейки ---
        int y = 44;
        uiLabelLeft(RX, y, 20, "Ячейки", cTextDim);
        y += 22;
        if (uiButton(RX, y, BW, BH, "Первый цвет в ячейку", true)) {
            m.pushUndo();
            const Color old = m.palette[static_cast<std::size_t>(palSelA_)];
            m.palette[static_cast<std::size_t>(palSelA_)] = m.firstColor;
            if (m.indexedOn)
                m.recolorPaletteEntry(static_cast<PalIndex>(palSelA_), old, m.firstColor);
            m.enforceIndexed();
            m.tileBmpDirty = true;
            m.overDirty = true;
        }
        if (uiButton(RX + BW + 6, y, BW, BH, "Добавить первый цвет", n < pal::MAX_PALETTE)) {
            const int at = m.addPaletteColor(m.firstColor, hi + 1);
            if (at >= 0) palSelA_ = palSelB_ = at;
        }
        y += 32;
        if (uiButton(RX, y, BW, BH, "Дублировать", n < pal::MAX_PALETTE) &&
            m.duplicatePaletteEntry(palSelA_))
            palSelA_ = palSelB_ = palSelA_ + 1;
        if (uiButton(RX + BW + 6, y, BW, BH, "Удалить", n > 1)) {
            // Удаляется весь выбранный ряд — с конца, чтобы номера не съезжали.
            for (int k = hi; k >= lo && m.palCount() > 1; --k)
                m.deletePaletteEntry(k);
            palSelA_ = palSelB_ = std::min(lo, m.palCount() - 1);
        }
        y += 32;
        if (uiButton(RX, y, BW, BH, "← Влево", palSelA_ > 0) &&
            m.movePaletteEntry(palSelA_, palSelA_ - 1))
            palSelA_ = palSelB_ = palSelA_ - 1;
        if (uiButton(RX + BW + 6, y, BW, BH, "Вправо →", palSelA_ < n - 1) &&
            m.movePaletteEntry(palSelA_, palSelA_ + 1))
            palSelA_ = palSelB_ = palSelA_ + 1;
        y += 32;
        if (uiButton(RX, y, BW, BH, "Убрать повторы", true)) {
            const int removed = m.removeDuplicateEntries();
            if (removed > 0)
                showMessage("Повторы", "Убрано ячеек: " + std::to_string(removed) + ".");
        }
        if (uiButton(RX + BW + 6, y, BW, BH, "Стандартная", true)) {
            m.pushUndo();
            Model def;
            m.setPalette(def.palette);
        }

        // --- сортировка ---
        y += 38;
        uiLabelLeft(RX, y, 20, hi > lo ? "Сортировать выбранное по" : "Сортировать всю палитру по",
                    cTextDim);
        y += 22;
        static const char* kKeys[pal::SORT_KEYS] = {"Тону",   "Насыщ.", "Яркости", "Светлоте",
                                                    "Красн.", "Зелён.", "Синему",  "Альфе"};
        for (int key = 0; key < pal::SORT_KEYS; ++key) {
            if (uiButton(RX + (key % 4) * 87, y + (key / 4) * 32, 82, BH, kKeys[key], n > 1))
                m.sortPalette(key, lo, hi);
        }

        // --- ряды и тон ---
        y += 70;
        uiLabelLeft(RX, y, 20, "Ряды и тон", cTextDim);
        y += 22;
        if (uiButton(RX, y, 2 * BW + 6, BH, "Ряд от первой выбранной до последней…", hi > lo)) {
            int steps = std::max(3, hi - lo + 1);
            int bend = 0;
            if (askTwoNumbers("Ряд цветов",
                              "Промежуточные встанут после первой ячейки; изгиб тона — в градусах",
                              "Ступеней", steps, 3, 32, "Изгиб тона", bend, -90, 90, "Построить",
                              nullptr)) {
                const int added = m.insertRamp(lo, hi, steps, bend);
                palSelB_ = hi + added;
            }
        }
        y += 32;
        if (uiButton(RX, y, 2 * BW + 6, BH, "Тон, насыщенность, яркость…", true)) {
            int dh = 0;
            int ds = 0;
            int dv = 0;
            if (askNumbers(
                    "Тон, насыщенность, яркость",
                    hi > lo ? "Для выбранных ячеек; точки картинки перекрасятся следом"
                            : "Для ячейки " + std::to_string(lo) + "; её точки перекрасятся следом",
                    {{"Тон, градусы", &dh, -180, 180},
                     {"Насыщенность, %", &ds, -100, 100},
                     {"Яркость, %", &dv, -100, 100}},
                    "Применить", nullptr))
                m.adjustPaletteEntries(lo, hi, dh, ds, dv);
        }
        y += 32;
        if (uiButton(RX, y, 2 * BW + 6, BH,
                     m.hasSelection ? "Извлечь из выделения…" : "Извлечь из картинки…", true)) {
            int maxColors = std::min(pal::MAX_PALETTE, std::max(2, n));
            if (askNumbers(
                    "Палитра из картинки", "Цвета без повторов; лишние сводятся медианным разрезом",
                    {{"Не больше цветов", &maxColors, 2, pal::MAX_PALETTE}}, "Извлечь", nullptr)) {
                const int got = m.extractPaletteFromTile(maxColors);
                showMessage("Палитра из картинки",
                            "Цветов в палитре: " + std::to_string(got) + ".");
            }
        }

        // --- файл ---
        y += 38;
        uiLabelLeft(RX, y, 20, "Файл: PNG, GIMP .gpl, Lospec .hex, JASC .pal", cTextDim);
        y += 22;
        if (uiButton(RX, y, BW, BH, "Загрузить…", true)) {
            const std::string path = pf_.openFileDialog("Загрузить палитру", kPaletteFilter,
                                                        resolvePath(cfg.lastFolder));
            waitMouseRelease();
            if (!path.empty()) loadPaletteFile(path, false, false);
        }
        if (uiButton(RX + BW + 6, y, BW, BH, "Добавить из файла…", n < pal::MAX_PALETTE)) {
            const std::string path = pf_.openFileDialog("Добавить цвета из палитры", kPaletteFilter,
                                                        resolvePath(cfg.lastFolder));
            waitMouseRelease();
            if (!path.empty()) loadPaletteFile(path, false, true);
        }
        y += 32;
        if (uiButton(RX, y, BW, BH, "Сохранить…", true)) {
            std::string path = pf_.saveFileDialog("Сохранить палитру", kPaletteFilter,
                                                  resolvePath(cfg.lastFolder), "palette.gpl");
            waitMouseRelease();
            if (!path.empty()) {
                if (pathExtLower(path).empty()) path += ".gpl";
                savePaletteFile(path);
            }
        }
        if (uiButton(RX + BW + 6, y, BW, BH, "Закрыть", true)) done = true;

        redraw();
        const int key = takeKey();
        if (key == KEY_ESC) done = true;
        if (key == KEY_DELETE && n > 1) {
            for (int k = hi; k >= lo && m.palCount() > 1; --k)
                m.deletePaletteEntry(k);
            palSelA_ = palSelB_ = std::min(lo, m.palCount() - 1);
        }
        if (key == KEY_LEFT) palSelA_ = palSelB_ = std::max(0, palSelA_ - 1);
        if (key == KEY_RIGHT) palSelA_ = palSelB_ = std::min(m.palCount() - 1, palSelA_ + 1);
        idle();
    }
    waitMouseRelease();
    m.needRepaint = true;
}

}  // namespace tjtm
