// План «Интерфейс» (docs/roadmap.md): то, что из него живёт в модели.

#include <gtest/gtest.h>

#include "core/model.h"

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

}  // namespace
}  // namespace tjtm
