// План «Интерфейс» (docs/roadmap.md): то, что из него живёт в модели.

#include <gtest/gtest.h>

#include <cmath>

#include "core/model.h"
#include "core/theme.h"

namespace tjtm {
namespace {

Model tileModel(int w = 16, int h = 16) {
    Model m;
    m.setDefaultPalette();
    m.newTileOfSize(w, h);
    m.map.reset(20, 12);
    return m;
}

void selectRect(Model& m, int x, int y, int w, int h) {
    m.selX = x;
    m.selY = y;
    m.selW = w;
    m.selH = h;
    m.hasSelection = true;
}

// A2: Tab больше не снимает выделение — оно остаётся за документом.
TEST(UiPlan, SelectionStaysWithItsDocument) {
    Model m = tileModel();
    m.keepSelectionPerDoc = true;
    selectRect(m, 2, 3, 4, 5);
    m.switchMode(DOC_MAP);
    EXPECT_FALSE(m.hasSelection) << "у карты своего выделения ещё нет";
    selectRect(m, 1, 1, 6, 2);
    m.switchMode(DOC_TILE);
    ASSERT_TRUE(m.hasSelection);
    EXPECT_EQ(m.selX, 2);
    EXPECT_EQ(m.selH, 5);
    m.switchMode(DOC_MAP);
    ASSERT_TRUE(m.hasSelection);
    EXPECT_EQ(m.selW, 6);
}

TEST(UiPlan, OriginalDropsSelectionOnModeSwitch) {
    Model m = tileModel();
    selectRect(m, 2, 3, 4, 5);
    m.switchMode(DOC_MAP);
    m.switchMode(DOC_TILE);
    EXPECT_FALSE(m.hasSelection);
}

TEST(UiPlan, ReplacedDocumentForgetsItsSelection) {
    Model m = tileModel();
    m.keepSelectionPerDoc = true;
    selectRect(m, 2, 3, 4, 5);
    m.switchMode(DOC_MAP);
    // Тайл с карты открыт заново: прежнее выделение к нему не относится.
    m.setTile(Grid<Color>(16, 16, argb(255, 1, 2, 3)), "other.png");
    m.switchMode(DOC_MAP);
    m.switchMode(DOC_TILE);
    EXPECT_FALSE(m.hasSelection);
}

TEST(UiPlan, SavedSelectionMustFitTheDocument) {
    Model m = tileModel();
    m.keepSelectionPerDoc = true;
    m.switchMode(DOC_MAP);
    selectRect(m, 10, 5, 8, 6);
    m.switchMode(DOC_TILE);
    // Карта стала меньше, пока её не было на экране.
    m.map.reset(12, 8);
    m.switchMode(DOC_MAP);
    EXPECT_FALSE(m.hasSelection);
}

// A1: правая кнопка по ячейке берёт второй цвет с точной привязкой.
TEST(UiPlan, SecondColorFromPaletteCell) {
    Model m = tileModel();
    m.palette[3] = argb(255, 10, 20, 30);
    m.palette[5] = argb(255, 10, 20, 30);
    m.pickSecondFromPalette(5);
    EXPECT_EQ(m.secondColor.argb(), argb(255, 10, 20, 30).argb());
    EXPECT_EQ(m.secondIdx, 5) << "цвет стоит дважды — привязка к той ячейке, где щёлкнули";
}

// A1: запись в ячейку — шаг истории и вне индексного режима.
TEST(UiPlan, PaletteCellWriteIsUndone) {
    Model m = tileModel();
    const Color before = m.palette[1];
    m.pushUndo();
    m.palette[1] = argb(255, 0, 0, 0);
    m.doUndo();
    EXPECT_EQ(m.palette[1].argb(), before.argb());
}

// A6: контраст по WCAG 2.1 — относительная яркость и отношение (L1 + 0,05) /
// (L2 + 0,05).
double luminance(Color c) {
    auto lin = [](int v) {
        const double s = v / 255.0;
        return s <= 0.04045 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * lin(c.r) + 0.7152 * lin(c.g) + 0.0722 * lin(c.b);
}

double contrast(Color a, Color b) {
    const double la = luminance(a);
    const double lb = luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

TEST(UiTheme, LightThemeMeetsWcag) {
    const theme::Theme t = theme::lightTheme();
    EXPECT_GE(contrast(t.text, t.back), 4.5);
    EXPECT_GE(contrast(t.text, t.window), 4.5);
    EXPECT_GE(contrast(t.textDim, t.window), 4.5) << "подсказки — мелкий текст";
    EXPECT_GE(contrast(t.textDim, t.back), 4.5);
    EXPECT_GE(contrast(t.outline, t.window), 3.0) << "граница нажимаемого";
    EXPECT_GE(contrast(t.outline, t.back), 3.0);
    EXPECT_GE(contrast(t.onAccent, t.accent), 4.5);
    EXPECT_GE(contrast(t.accent, t.window), 4.5) << "акцентом пишут «режим тайла»";
}

TEST(UiTheme, ClassicKeepsTheLookOf16) {
    // На «Классике» сверяются сценарии оригинала: её цвета не меняются.
    const theme::Theme t = theme::classicTheme();
    EXPECT_EQ(t.window.argb(), rgb(240, 242, 246).argb());
    EXPECT_EQ(t.border.argb(), rgb(205, 210, 218).argb());
    EXPECT_EQ(t.outline.argb(), t.border.argb());
    EXPECT_EQ(t.textDim.argb(), rgb(122, 130, 141).argb());
    EXPECT_EQ(t.checkA.argb(), rgb(214, 214, 218).argb());
    EXPECT_EQ(t.radius, 7);
    EXPECT_LT(contrast(t.textDim, t.window), 4.5) << "в 1.6 подсказки были бледнее нормы";
}

}  // namespace
}  // namespace tjtm
