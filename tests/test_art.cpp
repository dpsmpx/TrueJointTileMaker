// Этап 9: кисть из выделения, затенение по ряду палитры, градиент узором
// Байера, слой-образец.

#include <gtest/gtest.h>

#include <algorithm>
#include <set>

#include "core/imageops.h"
#include "core/model.h"
#include "core/project.h"

namespace tjtm {
namespace {

constexpr Color kRed = argb(255, 255, 0, 0);
constexpr Color kBlue = argb(255, 0, 0, 255);

Model spriteModel(int w = 16, int h = 16) {
    Model m;
    m.transparentBlank = true;
    m.brushReplace = true;
    m.newTileOfSize(w, h);
    return m;
}

/// Ряд из восьми ступеней серого от тёмного к светлому и второй ряд из двух.
std::vector<Color> rampPalette() {
    std::vector<Color> p;
    for (int k = 0; k < 8; ++k)
        p.push_back(argb(255, 20 + k * 30, 20 + k * 30, 20 + k * 30));
    p.push_back(kRed);
    p.push_back(kBlue);
    return p;
}

void stroke(Model& m, const std::vector<std::pair<int, int>>& pts) {
    m.beginStroke();
    for (std::size_t k = 0; k < pts.size(); ++k) {
        if (k == 0) {
            m.emitPixel(pts[k].first, pts[k].second);
        } else {
            m.shapeLine(pts[k - 1].first, pts[k - 1].second, pts[k].first, pts[k].second);
        }
    }
    m.endStroke();
}

TEST(Stamp, BrushFromSelectionKeepsShapeAndSkipsTransparent) {
    Model m = spriteModel();
    // Г-образная кисть 2x2: правый нижний угол прозрачный.
    m.tile.at(0, 0) = kRed;
    m.tile.at(1, 0) = kBlue;
    m.tile.at(0, 1) = kRed;
    m.applySelection(img::maskRect(16, 16, 0, 0, 1, 1), img::SEL_REPLACE);
    ASSERT_TRUE(m.brushFromSelection());
    EXPECT_EQ(m.penMode, PEN_STAMP);
    ASSERT_EQ(m.customBrush.w, 2);
    ASSERT_EQ(m.customBrush.h, 2);
    m.deselect();
    m.setTool(T_PEN);
    m.tile.at(9, 9) = kBlue;
    stroke(m, {{8, 8}});
    // Середина кисти 2x2 — её левая верхняя точка.
    EXPECT_EQ(m.tile.at(8, 8), kRed);
    EXPECT_EQ(m.tile.at(9, 8), kBlue);
    EXPECT_EQ(m.tile.at(8, 9), kRed);
    EXPECT_EQ(m.tile.at(9, 9), kBlue) << "прозрачная точка кисти ничего не кладёт";

    Model e = spriteModel();
    EXPECT_FALSE(e.brushFromSelection()) << "без выделения кисти нет";
    e.applySelection(img::maskRect(16, 16, 2, 2, 5, 5), img::SEL_REPLACE);
    EXPECT_FALSE(e.brushFromSelection()) << "из пустоты — тоже";
}

TEST(Stamp, MirrorReflectsTheBrush) {
    Model m = spriteModel(16, 8);
    m.tile.at(0, 0) = kRed;
    m.tile.at(1, 0) = kBlue;
    m.applySelection(img::maskRect(16, 8, 0, 0, 1, 0), img::SEL_REPLACE);
    ASSERT_TRUE(m.brushFromSelection());
    m.deselect();
    m.tile = Grid<Color>(16, 8, kClear);
    m.setTool(T_PEN);
    m.symX = true;
    stroke(m, {{2, 3}});
    EXPECT_EQ(m.tile.at(2, 3), kRed);
    EXPECT_EQ(m.tile.at(3, 3), kBlue);
    // Ось посредине (8): зеркало кладёт кисть задом наперёд.
    EXPECT_EQ(m.tile.at(13, 3), kRed);
    EXPECT_EQ(m.tile.at(12, 3), kBlue);
}

TEST(Shade, TargetFollowsRowLightness) {
    Model m = spriteModel();
    m.setPalette(rampPalette());
    EXPECT_EQ(m.shadeTarget(3, false), 4);
    EXPECT_EQ(m.shadeTarget(3, true), 2);
    EXPECT_EQ(m.shadeTarget(7, false), 7) << "светлее светлого конца не бывает";
    EXPECT_EQ(m.shadeTarget(0, true), 0);
    // Второй ряд — красный и синий: синий темнее, светлый конец слева.
    EXPECT_EQ(m.shadeTarget(9, false), 8);
    EXPECT_EQ(m.shadeTarget(8, true), 9);
    // Ряд задом наперёд: светлый конец слева, «светлее» — к началу.
    std::vector<Color> rev = rampPalette();
    std::reverse(rev.begin(), rev.begin() + 8);
    m.setPalette(rev);
    EXPECT_EQ(m.shadeTarget(3, false), 2);
}

TEST(Shade, StrokeShiftsEachPixelOnce) {
    Model m = spriteModel();
    const std::vector<Color> pal = rampPalette();
    m.setPalette(pal);
    for (int x = 0; x < 6; ++x)
        m.tile.at(x, 2) = pal[3];
    m.tile.at(6, 2) = argb(255, 1, 2, 3);  // не из палитры
    m.setTool(T_PEN);
    m.penMode = PEN_SHADE;
    // Мазок туда и обратно по тем же точкам: каждая сдвигается один раз.
    stroke(m, {{0, 2}, {6, 2}, {0, 2}});
    for (int x = 0; x < 6; ++x)
        EXPECT_EQ(m.tile.at(x, 2), pal[4]) << x;
    EXPECT_EQ(m.tile.at(6, 2), argb(255, 1, 2, 3)) << "цвет не из палитры не меняется";
    m.shadeDarker = true;
    stroke(m, {{0, 2}, {2, 2}});
    EXPECT_EQ(m.tile.at(1, 2), pal[3]);
    EXPECT_EQ(m.tile.at(4, 2), pal[4]);
    EXPECT_EQ(m.tile.at(0, 3), kClear) << "прозрачное не затеняется";
}

TEST(Gradient, BayerMixesOnlyTwoColorsAndFollowsDirection) {
    Model m = spriteModel(32, 8);
    m.firstColor = kRed;
    m.secondColor = kBlue;
    const Grid<std::uint8_t> region = m.gradientRegion(0, 0);
    ASSERT_EQ(img::maskCount(region), 32 * 8);
    EXPECT_EQ(m.applyGradient(region, 0, 4, 31, 4), 32 * 8);
    std::set<std::uint32_t> colors;
    for (const Color c : m.tile.d)
        colors.insert(c.argb());
    EXPECT_EQ(colors.size(), 2U) << "только два цвета кисти";
    // Узор 8x8: доля второго цвета растёт от блока к блоку столбцов.
    int prev = -1;
    for (int b = 0; b < 4; ++b) {
        int blue = 0;
        for (int x = b * 8; x < b * 8 + 8; ++x) {
            for (int y = 0; y < 8; ++y)
                blue += m.tile.at(x, y) == kBlue ? 1 : 0;
        }
        EXPECT_GT(blue, prev) << b;
        prev = blue;
    }
    EXPECT_EQ(m.tile.at(0, 0), kRed);
    EXPECT_EQ(m.tile.at(31, 7), kBlue);

    Model r = spriteModel(8, 8);
    r.firstColor = kRed;
    r.secondColor = kBlue;
    r.gradientRadial = true;
    r.applyGradient(r.gradientRegion(0, 0), 0, 0, 7, 7);
    EXPECT_EQ(r.tile.at(0, 0), kRed) << "у кругового в точке нажатия — первый цвет";
    EXPECT_EQ(r.tile.at(7, 7), kBlue) << "на радиусе — второй";

    // Щелчок без протяжки — заливка первым цветом; выделение режет область.
    Model c = spriteModel(8, 8);
    c.firstColor = kRed;
    c.secondColor = kBlue;
    c.clipToSelection = true;  // как у окна: кисть и заливка не выходят за выделение
    c.applySelection(img::maskRect(8, 8, 0, 0, 3, 7), img::SEL_REPLACE);
    c.applyGradient(c.gradientRegion(1, 1), 1, 1, 1, 1);
    EXPECT_EQ(c.tile.at(0, 0), kRed);
    EXPECT_EQ(c.tile.at(3, 7), kRed);
    EXPECT_EQ(c.tile.at(4, 0), kClear);
}

TEST(Reference, FitsShowsButIsNotExportedOrEdited) {
    Model m = spriteModel(16, 16);
    m.tile.at(0, 0) = kRed;
    Grid<Color> ref(32, 16, kBlue);
    ASSERT_EQ(m.addReferenceLayer(ref, "эскиз"), 0);
    ASSERT_EQ(m.doc.layerCount(), 2);
    EXPECT_EQ(m.doc.layers[0].kind, LK_REFERENCE);
    EXPECT_EQ(m.doc.layers[0].name, "эскиз");
    EXPECT_EQ(m.activeLayer, 1) << "рисунок остаётся активным";
    EXPECT_EQ(m.tile.at(0, 0), kRed);
    // Вписан с сохранением пропорций: 32x16 → 16x8 посредине.
    const CelPtr c = m.doc.cel(0, 0);
    ASSERT_TRUE(c);
    EXPECT_EQ(c->img.at(5, 3).a, 0);
    EXPECT_EQ(c->img.at(5, 4), kBlue);
    EXPECT_EQ(c->img.at(5, 11), kBlue);
    EXPECT_EQ(c->img.at(5, 12).a, 0);
    // На холсте — полупрозрачный, в выгрузке — нет.
    EXPECT_EQ(m.compositeFrame(0).at(5, 6).a, 0);
    const Color view = m.viewColorAt(5, 6);
    EXPECT_GT(view.a, 100);
    EXPECT_LT(view.a, 160);
    EXPECT_EQ(m.compositeFrame(0).at(0, 0), kRed);
    // Тот же образец во всех кадрах.
    m.addFrame(false);
    EXPECT_GT(m.viewColorAt(5, 6).a, 100);

    // Активный образец не правится.
    m.activeLayer = 0;
    m.loadActiveCel();
    EXPECT_FALSE(m.activeLayerEditable());
}

TEST(Reference, PaletteOpsSkipItAndProjectKeepsIt) {
    Model m = spriteModel(8, 8);
    m.tile.at(1, 1) = kRed;
    Grid<Color> photo(8, 8, argb(255, 123, 45, 67));
    m.addReferenceLayer(photo, "фото");
    m.strictIndexed = true;
    m.setPalette({kRed, kBlue});
    m.enableIndexed(false);
    // Строгий индексный режим сводит рисунок к палитре, а образец — нет.
    EXPECT_EQ(m.doc.cel(0, 0)->img.at(4, 4), argb(255, 123, 45, 67));
    EXPECT_EQ(m.unmatchedPixelCount(), 0) << "цвета образца палитре не мешают";

    // Без рисунка документ не остаётся: образец — не рисунок.
    EXPECT_FALSE(m.deleteLayer());

    std::string err;
    Model back;
    ASSERT_TRUE(projectDecode(projectEncode(m), back, err)) << err;
    ASSERT_EQ(back.doc.layerCount(), 2);
    EXPECT_EQ(back.doc.layers[0].kind, LK_REFERENCE);
    EXPECT_EQ(back.doc.layers[0].opacity, 128);
    EXPECT_EQ(back.compositeFrame(0).at(4, 4).a, 0);
}

}  // namespace
}  // namespace tjtm
