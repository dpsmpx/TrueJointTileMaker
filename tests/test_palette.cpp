// Этап 2 RoadToProLevel: палитра произвольного размера.

#include <gtest/gtest.h>

#include <algorithm>

#include "core/model.h"
#include "core/palette.h"

namespace tjtm {
namespace {

TEST(Palette, SortIsStableAndByKey) {
    const std::vector<Color> p = {rgb(0, 0, 255), rgb(255, 0, 0), rgb(0, 255, 0),
                                  rgb(128, 128, 128), rgb(255, 0, 0)};
    // По тону: красные (0°), зелёный (120°), синий (240°), серый — в конце.
    EXPECT_EQ(pal::sortOrder(p, pal::BY_HUE, 0, 4), (std::vector<int>{1, 4, 2, 0, 3}));
    // По красному каналу; равные остаются в прежнем порядке.
    EXPECT_EQ(pal::sortOrder(p, pal::BY_RED, 0, 4), (std::vector<int>{0, 2, 3, 1, 4}));
    // Только часть палитры.
    EXPECT_EQ(pal::sortOrder(p, pal::BY_BLUE, 1, 2), (std::vector<int>{1, 2}));
    EXPECT_EQ(pal::sortOrder(p, pal::BY_LUMA, 0, 2), (std::vector<int>{0, 1, 2}));
}

TEST(Palette, RampKeepsEndsAndBendsHue) {
    const Color a = rgb(40, 20, 80);
    const Color b = rgb(250, 230, 160);
    const auto r = pal::ramp(a, b, 5, 0);
    ASSERT_EQ(r.size(), 5u);
    EXPECT_EQ(r.front(), a);
    EXPECT_EQ(r.back(), b);
    // Яркость (наибольший канал) растёт монотонно.
    auto v = [](Color c) { return std::max({c.r, c.g, c.b}); };
    for (std::size_t k = 1; k < r.size(); ++k)
        EXPECT_GT(v(r[k]), v(r[k - 1]));
    // Изгиб тона меняет середину, но не концы.
    const auto bent = pal::ramp(a, b, 5, 30);
    EXPECT_EQ(bent.front(), a);
    EXPECT_EQ(bent.back(), b);
    EXPECT_NE(bent[2], r[2]);
    EXPECT_EQ(pal::ramp(a, b, 1, 0).size(), 1u);
}

TEST(Palette, AdjustHsv) {
    EXPECT_EQ(pal::adjustHSV(rgb(255, 0, 0), 120, 0, 0), rgb(0, 255, 0));
    EXPECT_EQ(pal::adjustHSV(rgb(255, 0, 0), 0, -100, 0), rgb(255, 255, 255));
    EXPECT_EQ(pal::adjustHSV(argb(77, 200, 100, 50), 0, 0, -100), argb(77, 0, 0, 0));
}

TEST(Palette, ExtractKeepsOrderAndReduces) {
    Grid<Color> g(4, 1);
    g.d = {rgb(10, 10, 10), argb(0, 0, 0, 0), rgb(200, 0, 0), rgb(10, 10, 10)};
    EXPECT_EQ(pal::extract(g, 16), (std::vector<Color>{rgb(10, 10, 10), rgb(200, 0, 0)}));
    // 64 оттенка серого сводятся к 4 — по одному на четверть.
    Grid<Color> grey(64, 1);
    for (int x = 0; x < 64; ++x)
        grey.at(x, 0) = rgb(x * 4, x * 4, x * 4);
    const auto four = pal::extract(grey, 4);
    ASSERT_EQ(four.size(), 4u);
    std::vector<int> v;
    for (const Color c : four)
        v.push_back(c.r);
    std::sort(v.begin(), v.end());
    EXPECT_NEAR(v[0], 30, 4);
    EXPECT_NEAR(v[3], 222, 4);
}

TEST(Palette, FromImage) {
    Grid<Color> strip(8, 8, rgb(1, 2, 3));
    EXPECT_EQ(pal::fromImage(strip).size(), 64u);  // каждая точка — ячейка, как Palitra.png
    Grid<Color> big(32, 32, rgb(5, 5, 5));
    EXPECT_EQ(pal::fromImage(big).size(), 1u);  // большая картинка — её цвета
}

TEST(Palette, NearestByRgb) {
    const std::vector<Color> p = {rgb(0, 0, 0), rgb(255, 255, 255), rgb(255, 0, 0)};
    EXPECT_EQ(pal::nearest(p, rgb(200, 30, 30)), 2);
    EXPECT_EQ(pal::nearest(p, rgb(100, 100, 100)), 0);
    EXPECT_EQ(pal::nearest({}, rgb(1, 1, 1)), -1);
}

TEST(Palette, FileFormatsRoundTrip) {
    const std::vector<Color> p = {rgb(0, 0, 0), rgb(255, 128, 1), rgb(18, 52, 86)};
    std::vector<Color> back;
    std::string err;
    ASSERT_TRUE(pal::parseGpl(pal::writeGpl(p, "проба"), back, err)) << err;
    EXPECT_EQ(back, p);
    ASSERT_TRUE(pal::parseHex(pal::writeHex(p), back, err)) << err;
    EXPECT_EQ(back, p);
    ASSERT_TRUE(pal::parseJasc(pal::writeJasc(p), back, err)) << err;
    EXPECT_EQ(back, p);
    // Чужие файлы: «#», комментарии, BOM, восемь цифр с прозрачностью.
    ASSERT_TRUE(pal::parseHex("\xEF\xBB\xBF#ff0000\n; комментарий\n80112233\n", back, err)) << err;
    EXPECT_EQ(back, (std::vector<Color>{rgb(255, 0, 0), argb(0x80, 0x11, 0x22, 0x33)}));
    EXPECT_FALSE(pal::parseHex("zz\n", back, err));
    EXPECT_FALSE(pal::parseGpl("not a palette\n", back, err));
    ASSERT_TRUE(pal::parseGpl("GIMP Palette\nName: x\n#\n  1   2   3\tUntitled\n", back, err));
    EXPECT_EQ(back, (std::vector<Color>{rgb(1, 2, 3)}));
}

// --- модель -------------------------------------------------------------------

/// Тайл 4x4 в строгом индексном режиме с палитрой красный, зелёный, синий.
Model indexedModel() {
    Model m;
    m.strictIndexed = true;
    m.transparentBlank = true;
    m.newTileOfSize(4, 4);
    m.setPalette({rgb(255, 0, 0), rgb(0, 255, 0), rgb(0, 0, 255)});
    m.tile.at(0, 0) = rgb(255, 0, 0);
    m.tile.at(1, 0) = rgb(0, 255, 0);
    m.tile.at(2, 0) = rgb(0, 0, 255);
    m.tile.at(3, 0) = rgb(250, 10, 10);  // почти красный — не из палитры
    m.enableIndexed(false);
    return m;
}

TEST(ModelPal, EnableIndexedSnapsToPalette) {
    Model m = indexedModel();
    EXPECT_EQ(m.tile.at(3, 0), rgb(255, 0, 0));
    EXPECT_EQ(m.tileIdx.at(3, 0), 0);
    EXPECT_EQ(m.tileIdx.at(2, 0), 2);
    EXPECT_EQ(m.tileIdx.at(0, 1), IDX_NONE);  // прозрачное без прозрачного номера свободно
    // С добавлением недостающих цвет остаётся, а палитра растёт.
    Model a;
    a.strictIndexed = true;
    a.newTileOfSize(4, 4);
    a.setPalette({rgb(255, 0, 0)});
    a.tile.at(0, 0) = rgb(1, 2, 3);
    a.enableIndexed(true);
    EXPECT_EQ(a.palCount(), 3);  // красный, белый фон, rgb(1,2,3)
    EXPECT_EQ(a.tile.at(0, 0), rgb(1, 2, 3));
}

TEST(ModelPal, PermutationsKeepPixelsBound) {
    Model m = indexedModel();
    m.sortPalette(pal::BY_BLUE, 0, 2);  // синий — в конец, порядок прочих тот же
    EXPECT_EQ(m.palette[2], rgb(0, 0, 255));
    for (int x = 0; x < 4; ++x)
        EXPECT_EQ(m.effectiveColor(m.tileIdx.at(x, 0)), m.tile.at(x, 0)) << x;
    ASSERT_TRUE(m.movePaletteEntry(0, 2));
    EXPECT_EQ(m.palette[2], rgb(255, 0, 0));
    EXPECT_EQ(m.tileIdx.at(0, 0), 2);
    EXPECT_EQ(m.tile.at(0, 0), rgb(255, 0, 0));
    // Удаление ячейки: её точки в строгом режиме — к ближайшей.
    ASSERT_TRUE(m.deletePaletteEntry(2));
    EXPECT_EQ(m.palCount(), 2);
    EXPECT_NE(m.tileIdx.at(0, 0), IDX_NONE);
    EXPECT_EQ(m.tile.at(0, 0), m.effectiveColor(m.tileIdx.at(0, 0)));
    // Всё это — шаги истории.
    m.doUndo();
    m.doUndo();
    EXPECT_EQ(m.palCount(), 3);
    EXPECT_EQ(m.tile.at(0, 0), rgb(255, 0, 0));
}

TEST(ModelPal, AddDuplicateAndDedupe) {
    Model m = indexedModel();
    EXPECT_EQ(m.addPaletteColor(rgb(9, 9, 9), 0), 0);
    EXPECT_EQ(m.tileIdx.at(0, 0), 1);  // красный сдвинулся на номер вперёд
    ASSERT_TRUE(m.duplicatePaletteEntry(1));
    EXPECT_EQ(m.palCount(), 5);
    EXPECT_EQ(m.removeDuplicateEntries(), 1);
    EXPECT_EQ(m.palCount(), 4);
    EXPECT_EQ(m.palette[m.tileIdx.at(0, 0)], rgb(255, 0, 0));
}

TEST(ModelPal, RampInsertsBetween) {
    Model m = indexedModel();
    EXPECT_EQ(m.insertRamp(0, 2, 5, 0), 3);
    EXPECT_EQ(m.palCount(), 6);
    EXPECT_EQ(m.palette[0], rgb(255, 0, 0));
    EXPECT_EQ(m.palette[4], rgb(0, 255, 0));  // зелёный сдвинулся за ряд
    EXPECT_EQ(m.tileIdx.at(1, 0), 4);
    EXPECT_EQ(m.tileIdx.at(2, 0), 5);
}

TEST(ModelPal, TransparentIndexHidesPixels) {
    Model m = indexedModel();
    m.setTransparentIndex(1);  // зелёный — прозрачный
    EXPECT_EQ(m.tile.at(1, 0).a, 0);
    EXPECT_EQ(m.tile.at(0, 0).a, 255);
    // Ластик в индексном режиме пишет прозрачный номер.
    m.toolTile = T_ERASER;
    m.beginStroke();
    m.emitPixel(2, 0);
    m.endStroke();
    EXPECT_EQ(m.tileIdx.at(2, 0), 1);
    EXPECT_EQ(m.tile.at(2, 0).a, 0);
    m.setTransparentIndex(IDX_NONE);
    EXPECT_EQ(m.tile.at(1, 0), rgb(0, 255, 0));
}

TEST(ModelPal, StrictBrushUsesPaletteColour) {
    Model m = indexedModel();
    m.firstColor = rgb(10, 20, 240);  // набранный ползунками, не из палитры
    m.firstIdx = IDX_NONE;
    m.brushReplace = false;  // смешивание в индексном режиме недоступно
    m.beginStroke();
    m.emitPixel(1, 1);
    m.endStroke();
    EXPECT_EQ(m.tile.at(1, 1), rgb(0, 0, 255));
    EXPECT_EQ(m.tileIdx.at(1, 1), 2);
    EXPECT_EQ(m.fillColor(), rgb(0, 0, 255));
}

TEST(ModelPal, AdjustEntriesRecolorsPixels) {
    Model m = indexedModel();
    m.adjustPaletteEntries(0, 0, 120, 0, 0);  // красный → зелёный
    EXPECT_EQ(m.palette[0], rgb(0, 255, 0));
    EXPECT_EQ(m.tile.at(0, 0), rgb(0, 255, 0));
    EXPECT_EQ(m.tile.at(3, 0), rgb(0, 255, 0));
}

TEST(ModelPal, ExtractAndScroll) {
    Model m;
    m.newTileOfSize(16, 16);
    for (int x = 0; x < 16; ++x) {
        for (int y = 0; y < 16; ++y)
            m.tile.at(x, y) = rgb(x * 16, y * 16, 0);
    }
    EXPECT_EQ(m.extractPaletteFromTile(256), 256);
    EXPECT_EQ(m.palCount(), 256);
    EXPECT_EQ(m.palMaxScroll(), 32 - 8);
    m.scrollPalette(100);
    EXPECT_EQ(m.palScroll, 24);
    EXPECT_EQ(m.palCellIndex(0, 0), 24 * 8);
    EXPECT_EQ(m.palCellIndex(7, 7), 255);
    m.setPalette({rgb(1, 1, 1)});
    EXPECT_EQ(m.palScroll, 0);
    EXPECT_EQ(m.palCellIndex(1, 0), -1);
    EXPECT_EQ(m.extractPaletteFromTile(4), 4);
}

}  // namespace
}  // namespace tjtm
