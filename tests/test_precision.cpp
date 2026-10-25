// Этап 5 RoadToProLevel: поворот на любой угол, контур с толщиной, край,
// сокращение цветов, радиальная симметрия, сила кисти, направляющие в .tjt.

#include <gtest/gtest.h>

#include <set>

#include "core/anim.h"
#include "core/imageops.h"
#include "core/model.h"
#include "core/project.h"

namespace tjtm {
namespace {

constexpr Color kRed = argb(255, 255, 0, 0);
constexpr Color kBlue = argb(255, 0, 0, 255);
constexpr Color kClear = argb(0, 0, 0, 0);

Model spriteModel(int w = 16, int h = 16) {
    Model m;
    m.transparentBlank = true;
    m.brushReplace = true;
    m.newTileOfSize(w, h);
    return m;
}

Grid<Color> arrow() {
    // Несимметричная фигура: поворот виден, а не угадан.
    Grid<Color> g(7, 5, kClear);
    for (int x = 0; x < 6; ++x)
        g.at(x, 2) = kRed;
    g.at(5, 1) = kRed;
    g.at(5, 3) = kRed;
    g.at(6, 2) = kBlue;
    return g;
}

TEST(Rotate, QuarterTurnsMatchExactRotations) {
    const Grid<Color> g = arrow();
    EXPECT_EQ(img::rotateAny(g, 90, img::ROT_NEAREST, true, kClear).d, img::rotate90(g).d);
    EXPECT_EQ(img::rotateAny(g, 180, img::ROT_NEAREST, true, kClear).d, img::rotate180(g).d);
    EXPECT_EQ(img::rotateAny(g, -90, img::ROT_NEAREST, true, kClear).d, img::rotate270(g).d);
    EXPECT_EQ(img::rotateAny(g, 0, img::ROT_SPRITE, true, kClear).d, g.d);
}

TEST(Rotate, AnyAngleAddsNoNewColorsAndGrowsCanvas) {
    const Grid<Color> g = arrow();
    for (const int method : {img::ROT_NEAREST, img::ROT_SPRITE}) {
        const Grid<Color> r = img::rotateAny(g, 30, method, true, kClear);
        int nw = 0;
        int nh = 0;
        img::rotatedSize(g.w, g.h, 30, nw, nh);
        EXPECT_EQ(r.w, nw);
        EXPECT_EQ(r.h, nh);
        EXPECT_GT(r.w, g.w);
        std::set<std::uint32_t> colors;
        for (const Color c : r.d)
            colors.insert(c.argb());
        for (const std::uint32_t c : colors)
            EXPECT_TRUE(c == kClear.argb() || c == kRed.argb() || c == kBlue.argb())
                << std::hex << c;
        EXPECT_GT(colors.count(kRed.argb()), 0U);
    }
}

TEST(Rotate, ModelRotatesEveryCelAndSelection) {
    Model m = spriteModel(8, 8);
    m.tile.at(0, 0) = kRed;
    m.addFrame(false);
    m.tile.at(7, 0) = kBlue;
    ASSERT_TRUE(m.rotateImageBy(90, img::ROT_NEAREST, true));
    EXPECT_EQ(m.tile.at(7, 7).argb(), kBlue.argb());
    m.setActiveCel(0, 0);
    EXPECT_EQ(m.tile.at(7, 0).argb(), kRed.argb());
    // Повёрнутое больше предела — отказ, а не порча.
    Model big = spriteModel(MAX_TILE, MAX_TILE / 2);
    EXPECT_FALSE(big.rotateImageBy(45, img::ROT_NEAREST, true));
}

TEST(Outline, ThicknessOutsideAndInside) {
    Grid<Color> g(9, 9, kClear);
    g.at(4, 4) = kRed;
    EXPECT_EQ(img::outlineThick(g, {}, kBlue, true, true, 2), 8 + 16);
    EXPECT_EQ(g.at(2, 2).argb(), kBlue.argb());
    EXPECT_EQ(g.at(1, 4).a, 0);
    Grid<Color> sq(8, 8, kRed);
    EXPECT_EQ(img::outlineThick(sq, {}, kBlue, false, false, 2), 64 - 16);
    EXPECT_EQ(sq.at(1, 1).argb(), kBlue.argb());
    EXPECT_EQ(sq.at(3, 3).argb(), kRed.argb());
}

TEST(Outline, GrowAndShrinkEdge) {
    Grid<Color> g(5, 5, kClear);
    g.at(2, 2) = kRed;
    EXPECT_EQ(img::growEdge(g, {}, false), 4);
    EXPECT_EQ(g.at(2, 1).argb(), kRed.argb());
    EXPECT_EQ(g.at(1, 1).a, 0);
    EXPECT_EQ(img::shrinkEdge(g, {}, false), 4);
    EXPECT_EQ(g.at(2, 2).argb(), kRed.argb());
    EXPECT_EQ(g.at(2, 1).a, 0);
}

TEST(Colors, ReduceKeepsAlphaAndCount) {
    Model m = spriteModel(16, 1);
    for (int x = 0; x < 16; ++x)
        m.tile.at(x, 0) = argb(x == 0 ? 128 : 255, static_cast<std::uint8_t>(x * 16), 0, 0);
    EXPECT_GT(m.reduceImageColors(2), 0);
    // Тайл не ниже MIN_TILE: строки под первой пусты и в счёт не идут.
    std::set<std::uint32_t> rgbs;
    for (const Color c : m.tile.d) {
        if (c.a != 0) rgbs.insert(c.argb() & 0xFFFFFF);
    }
    EXPECT_EQ(rgbs.size(), 2U);
    EXPECT_EQ(m.tile.at(0, 0).a, 128);
    m.doUndo();
    EXPECT_EQ(m.tile.at(5, 0).r, 80);
}

TEST(Symmetry, RadialFourRaysPaintsFourPoints) {
    Model m = spriteModel(8, 8);
    m.symRadial = 4;
    m.firstColor = kRed;
    m.beginStroke();
    m.emitPixel(4, 0);
    m.endStroke();
    // Центр — середина документа; луч вверх уходит вправо, вниз и влево.
    EXPECT_EQ(m.tile.at(4, 0).argb(), kRed.argb());
    EXPECT_EQ(m.tile.at(7, 4).argb(), kRed.argb());
    EXPECT_EQ(m.tile.at(3, 7).argb(), kRed.argb());
    EXPECT_EQ(m.tile.at(0, 3).argb(), kRed.argb());
    int painted = 0;
    for (const Color c : m.tile.d)
        painted += c.a != 0 ? 1 : 0;
    EXPECT_EQ(painted, 4);
}

TEST(Brush, OpacityBlendsOncePerStroke) {
    Model m = spriteModel(4, 1);
    m.tile.at(0, 0) = argb(255, 255, 255, 255);
    m.brushOpacity = 128;
    m.firstColor = argb(255, 0, 0, 0);
    m.beginStroke();
    m.emitPixel(0, 0);
    m.emitPixel(0, 0);  // повтор в том же мазке не сгущает цвет
    m.endStroke();
    EXPECT_NEAR(m.tile.at(0, 0).r, 127, 1);
    m.toolTile = T_ERASER;
    m.beginStroke();
    m.emitPixel(0, 0);
    m.endStroke();
    EXPECT_NEAR(m.tile.at(0, 0).a, 127, 1);
}

TEST(Sheet, PaddingAndBackground) {
    std::vector<AnimFrame> frames = {AnimFrame{Grid<Color>(2, 2, kRed), 100},
                                     AnimFrame{Grid<Color>(2, 2, kClear), 100}};
    SheetOptions o;
    o.padding = 1;
    o.background = kBlue;
    const SpriteSheet s = buildSheet(frames, o);
    EXPECT_EQ(s.image.w, 2 * 4);
    EXPECT_EQ(s.image.h, 4);
    EXPECT_EQ(s.cells[0].x, 1);
    EXPECT_EQ(s.cells[1].x, 5);
    EXPECT_EQ(s.image.at(0, 0).argb(), kBlue.argb());
    EXPECT_EQ(s.image.at(1, 1).argb(), kRed.argb());
    EXPECT_EQ(s.image.at(5, 1).argb(), kBlue.argb());  // фон под прозрачным кадром
}

TEST(Project, GuidesOriginAndMetaRoundTrip) {
    Model m = spriteModel(16, 16);
    m.guidesX2 = {8, 17};
    m.guidesY2 = {32};
    m.originX = 4;
    m.originY = -2;
    m.majorGridW = 8;
    m.majorGridH = 4;
    m.guideCenter = true;
    m.symRadial = 6;
    m.brushOpacity = 77;
    m.setMeta("tileset.slot", "wall");
    m.setMeta("будущее", "значение");
    const auto bytes = projectEncode(m);
    Model b;
    std::string err;
    ASSERT_TRUE(projectDecode(bytes, b, err)) << err;
    EXPECT_EQ(b.guidesX2, m.guidesX2);
    EXPECT_EQ(b.guidesY2, m.guidesY2);
    EXPECT_EQ(b.originX, 4);
    EXPECT_EQ(b.originY, -2);
    EXPECT_EQ(b.majorGridW, 8);
    EXPECT_EQ(b.majorGridH, 4);
    EXPECT_TRUE(b.guideCenter);
    EXPECT_EQ(b.symRadial, 6);
    EXPECT_EQ(b.brushOpacity, 77);
    EXPECT_EQ(b.metaValue("tileset.slot"), "wall");
    EXPECT_EQ(b.metaValue("будущее"), "значение");
    // Новый документ своих направляющих не наследует.
    b.newTileOfSize(8, 8);
    EXPECT_TRUE(b.guidesX2.empty());
    EXPECT_TRUE(b.metaValue("tileset.slot").empty());
}

}  // namespace
}  // namespace tjtm
