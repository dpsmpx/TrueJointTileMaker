// Этап 8 RoadToProLevel: фильтры, цветокоррекция, плавные преобразования,
// сглаживание фигур, аэрограф и сведение итога к палитре.

#include <gtest/gtest.h>

#include <set>

#include "core/filters.h"
#include "core/imageops.h"
#include "core/model.h"
#include "core/vector.h"

namespace tjtm {
namespace {

constexpr Color kRed = argb(255, 255, 0, 0);
constexpr Color kGreen = argb(255, 0, 255, 0);
constexpr Color kBlue = argb(255, 0, 0, 255);
constexpr Color kClear = argb(0, 0, 0, 0);

Grid<Color> pattern(int w, int h) {
    Grid<Color> g(w, h, kClear);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if ((x * 3 + y * 5) % 7 < 3) g.at(x, y) = argb(255, x * 13 % 256, y * 29 % 256, 90);
        }
    }
    return g;
}

Model spriteModel(int w = 16, int h = 16) {
    Model m;
    m.transparentBlank = true;
    m.brushReplace = true;
    m.newTileOfSize(w, h);
    return m;
}

TEST(Filters, BlurKeepsUniformAndDoesNotLeakHiddenColor) {
    const Grid<Color> flat(8, 8, argb(255, 40, 80, 120));
    EXPECT_EQ(fx::blur(flat, 3, false).d, flat.d);
    EXPECT_EQ(fx::blur(flat, 3, true).d, flat.d);
    // Прозрачные точки помнят синий, но его не видно — и в размытие он не
    // попадает: край красной точки остаётся красным, только прозрачнее.
    Grid<Color> g(9, 9, argb(0, 0, 0, 255));
    g.at(4, 4) = kRed;
    const Grid<Color> b = fx::blur(g, 2, false);
    EXPECT_LT(b.at(4, 4).a, 255);
    EXPECT_GT(b.at(5, 4).a, 0);
    for (const Color c : b.d) {
        if (c.a == 0) continue;
        EXPECT_EQ(c.r, 255);
        EXPECT_EQ(c.b, 0);
    }
}

TEST(Filters, WrappedBlurStaysSeamless) {
    // По кругу размытие не видит края: сдвиг до и после — одно и то же.
    const Grid<Color> g = pattern(12, 10);
    const Grid<Color> a = img::offsetWrap(fx::blur(g, 2, true), 5, 3);
    const Grid<Color> b = fx::blur(img::offsetWrap(g, 5, 3), 2, true);
    EXPECT_EQ(a.d, b.d);
    const Grid<Color> sa = img::offsetWrap(fx::sharpen(g, 150, 2, true), 4, 7);
    const Grid<Color> sb = fx::sharpen(img::offsetWrap(g, 4, 7), 150, 2, true);
    EXPECT_EQ(sa.d, sb.d);
}

TEST(Filters, SharpenRaisesContrastAndKeepsAlpha) {
    Grid<Color> g(8, 1, argb(255, 100, 100, 100));
    for (int x = 4; x < 8; ++x)
        g.at(x, 0) = argb(255, 160, 160, 160);
    const Grid<Color> s = fx::sharpen(g, 100, 1.5, false);
    EXPECT_LT(s.at(3, 0).r, 100);
    EXPECT_GT(s.at(4, 0).r, 160);
    EXPECT_EQ(s.at(0, 0).r, 100);
    for (std::size_t k = 0; k < g.d.size(); ++k)
        EXPECT_EQ(s.d[k].a, g.d[k].a);
}

TEST(Filters, EdgesAndEmbossOnFlatAreDark) {
    const Grid<Color> flat(6, 6, argb(255, 90, 90, 90));
    for (const Color c : fx::edges(flat, true).d)
        EXPECT_EQ(c, argb(255, 0, 0, 0));
    for (const Color c : fx::emboss(flat, true).d)
        EXPECT_EQ(c, argb(255, 128, 128, 128));
    Grid<Color> step(6, 6, argb(255, 0, 0, 0));
    for (int y = 0; y < 6; ++y)
        step.at(3, y) = argb(255, 255, 255, 255);
    const Grid<Color> e = fx::edges(step, false);
    EXPECT_GT(e.at(2, 3).r, 100);
    EXPECT_EQ(e.at(0, 3).r, 0);
}

TEST(Filters, NoiseIsDeterministicAndBounded) {
    const Grid<Color> g = pattern(16, 16);
    const Grid<Color> a = fx::noise(g, 20, false, 7);
    EXPECT_EQ(a.d, fx::noise(g, 20, false, 7).d);
    EXPECT_NE(a.d, fx::noise(g, 20, false, 8).d);
    for (std::size_t k = 0; k < g.d.size(); ++k) {
        if (g.d[k].a == 0) {
            EXPECT_EQ(a.d[k], g.d[k]);
            continue;
        }
        EXPECT_LE(std::abs(a.d[k].r - g.d[k].r), 20);
        EXPECT_LE(std::abs(a.d[k].b - g.d[k].b), 20);
    }
    const Grid<Color> mono = fx::noise(Grid<Color>(4, 4, argb(255, 128, 128, 128)), 30, true, 1);
    for (const Color c : mono.d) {
        EXPECT_EQ(c.r, c.g);
        EXPECT_EQ(c.g, c.b);
    }
}

TEST(Filters, ColorAdjustments) {
    const Grid<Color> g = pattern(10, 10);
    EXPECT_EQ(fx::brightnessContrast(g, 0, 0, 0).d, g.d);
    EXPECT_EQ(fx::invertColors(fx::invertColors(g)).d, g.d);
    EXPECT_EQ(fx::levels(g, fx::Levels{}).d, g.d);
    EXPECT_EQ(fx::curves(g, {{0, 0}, {255, 255}}, {}, {}, {}).d, g.d);
    for (const Color c : fx::grayscale(g).d) {
        EXPECT_EQ(c.r, c.g);
        EXPECT_EQ(c.g, c.b);
    }
    const Grid<Color> gray = fx::brightnessContrast(g, 0, 0, -100);
    for (const Color c : gray.d) {
        if (c.a == 0) continue;
        EXPECT_LE(std::abs(c.r - c.b), 1);
    }
    const Grid<Color> dark =
        fx::brightnessContrast(Grid<Color>(1, 1, argb(255, 200, 200, 200)), -50, 0, 0);
    EXPECT_LT(dark.d[0].r, 200);
    const Grid<Color> inv = fx::invertColors(Grid<Color>(1, 1, argb(77, 10, 20, 30)));
    EXPECT_EQ(inv.d[0], argb(77, 245, 235, 225));
}

TEST(Filters, LevelsAndCurveLuts) {
    fx::Levels l;
    l.inLo = 50;
    l.inHi = 200;
    const auto lut = fx::levelsLut(l);
    EXPECT_EQ(lut[0], 0);
    EXPECT_EQ(lut[50], 0);
    EXPECT_EQ(lut[200], 255);
    EXPECT_EQ(lut[255], 255);
    l = fx::Levels{};
    l.gamma = 2.0;
    EXPECT_GT(fx::levelsLut(l)[64], 64);

    // Кривая проходит через свои точки и не выбрасывает горбов.
    const fx::Curve c = {{0, 0}, {64, 100}, {128, 140}, {255, 255}};
    const auto cl = fx::curveLut(c);
    EXPECT_EQ(cl[0], 0);
    EXPECT_EQ(cl[64], 100);
    EXPECT_EQ(cl[128], 140);
    EXPECT_EQ(cl[255], 255);
    for (int v = 1; v < 256; ++v)
        EXPECT_GE(cl[static_cast<std::size_t>(v)], cl[static_cast<std::size_t>(v - 1)]) << v;
    // Плато не прогибается между равными точками.
    const auto flat = fx::curveLut({{0, 0}, {100, 120}, {160, 120}, {255, 255}});
    for (int v = 100; v <= 160; ++v)
        EXPECT_EQ(flat[static_cast<std::size_t>(v)], 120) << v;
    const auto hist = fx::histogram(Grid<Color>(3, 2, argb(255, 255, 255, 255)));
    EXPECT_EQ(hist[255], 6);
}

TEST(Filters, SmoothTransformsKeepIdentity) {
    const Grid<Color> g = pattern(9, 7);
    EXPECT_EQ(fx::scaleSmooth(g, 9, 7).d, g.d);
    EXPECT_EQ(fx::rotateSmooth(g, 0, true).d, g.d);
    EXPECT_EQ(fx::rotateSmooth(g, 90, true).d, img::rotate90(g).d);
    EXPECT_EQ(fx::skew(g, 0, 0, false).d, g.d);
    EXPECT_EQ(fx::perspective(g, fx::identityQuad(g.w, g.h)).d, g.d);
}

TEST(Filters, ScaleSmoothAveragesAndInterpolates) {
    Grid<Color> g(4, 4, kClear);
    for (int y = 0; y < 4; ++y) {
        for (int x = 0; x < 4; ++x)
            g.at(x, y) = (x + y) % 2 == 0 ? argb(255, 0, 0, 0) : argb(255, 200, 200, 200);
    }
    const Grid<Color> half = fx::scaleSmooth(g, 2, 2);
    for (const Color c : half.d)
        EXPECT_EQ(c, argb(255, 100, 100, 100));
    const Grid<Color> up = fx::scaleSmooth(Grid<Color>(3, 3, kRed), 7, 5);
    for (const Color c : up.d)
        EXPECT_EQ(c, kRed);
    // Половина прозрачна: среднее — полупрозрачный красный, а не тёмный.
    Grid<Color> two(2, 1, kClear);
    two.at(0, 0) = kRed;
    const Grid<Color> one = fx::scaleSmooth(two, 1, 1);
    EXPECT_EQ(one.d[0].r, 255);
    EXPECT_NEAR(one.d[0].a, 128, 1);
}

TEST(Filters, PerspectiveAndSkewMoveContent) {
    const Grid<Color> g(10, 10, kBlue);
    fx::Quad q = fx::identityQuad(10, 10);
    q.x[1] = 5;
    q.x[2] = 5;
    const Grid<Color> p = fx::perspective(g, q);
    EXPECT_EQ(p.at(2, 5), kBlue);
    EXPECT_EQ(p.at(8, 5).a, 0);
    const Grid<Color> s = fx::skew(g, 30, 0, false);
    EXPECT_EQ(s.at(5, 5), kBlue);
    // Верх уходит влево, низ — вправо: пустеют правый верхний и левый
    // нижний углы.
    EXPECT_EQ(s.at(9, 0).a, 0);
    EXPECT_EQ(s.at(0, 9).a, 0);
    EXPECT_EQ(s.at(0, 0), kBlue);
}

TEST(Filters, SoftDab) {
    const Grid<float> d = fx::softDab(9, 0);
    EXPECT_FLOAT_EQ(d.at(4, 4), 1.0F);
    EXPECT_FLOAT_EQ(d.at(0, 0), 0.0F);
    EXPECT_GT(d.at(2, 4), 0.0F);
    EXPECT_LT(d.at(2, 4), 1.0F);
    const Grid<float> hard = fx::softDab(9, 100);
    for (const float v : hard.d)
        EXPECT_TRUE(v == 0.0F || v == 1.0F);
    EXPECT_FLOAT_EQ(fx::softDab(1, 0).at(0, 0), 1.0F);
}

TEST(SmoothShapes, HorizontalLineStaysCrispDiagonalGetsCoverage) {
    VecShape h;
    h.type = VS_POLYLINE;
    h.pts = {{2.5, 4.5}, {12.5, 4.5}};
    h.stroke = kRed;
    h.smooth = true;
    Grid<Color> g(16, 10, kClear);
    vec::rasterize(h, g);
    for (int x = 3; x <= 12; ++x) {
        EXPECT_EQ(g.at(x, 4), kRed) << x;
        EXPECT_EQ(g.at(x, 3).a, 0) << x;
        EXPECT_EQ(g.at(x, 5).a, 0) << x;
    }
    VecShape d = h;
    d.pts = {{1.5, 1.5}, {13.5, 7.5}};
    Grid<Color> dg(16, 10, kClear);
    vec::rasterize(d, dg);
    int partial = 0;
    for (const Color c : dg.d) {
        if (c.a > 0 && c.a < 255) ++partial;
        if (c.a > 0) {
            EXPECT_EQ(c.r, 255);
        }
    }
    EXPECT_GT(partial, 10);

    VecShape e;
    e.type = VS_ELLIPSE;
    e.pts = {{2.5, 1.5}, {13.5, 8.5}};
    e.stroke = kBlue;
    e.fill = kBlue;
    e.smooth = true;
    Grid<Color> eg(16, 10, kClear);
    vec::rasterize(e, eg);
    EXPECT_EQ(eg.at(8, 5), kBlue);
    EXPECT_EQ(eg.at(0, 0).a, 0);
    bool soft = false;
    for (const Color c : eg.d)
        soft = soft || (c.a > 0 && c.a < 255);
    EXPECT_TRUE(soft);
}

TEST(ModelFx, FilterRespectsSelectionAndUndo) {
    Model m = spriteModel(8, 8);
    for (int x = 0; x < 8; ++x)
        m.tile.at(x, 3) = kRed;
    const Grid<Color> before = m.tile;
    m.applySelection(img::maskRect(8, 8, 0, 0, 3, 7), img::SEL_REPLACE);
    EXPECT_GT(m.applyFilter([](const Grid<Color>& g) { return fx::blur(g, 2, false); }, false), 0);
    for (int y = 0; y < 8; ++y) {
        for (int x = 4; x < 8; ++x)
            EXPECT_EQ(m.tile.at(x, y), before.at(x, y)) << x << "," << y;
    }
    EXPECT_NE(m.tile.at(1, 2), before.at(1, 2));
    m.doUndo();
    EXPECT_EQ(m.tile.d, before.d);
}

TEST(ModelFx, StrictIndexedSnapsFiltersToPalette) {
    Model m = spriteModel(12, 12);
    m.strictIndexed = true;
    m.setPalette({kRed, kGreen, kBlue, argb(255, 255, 255, 255)});
    for (int y = 2; y < 10; ++y) {
        for (int x = 2; x < 10; ++x)
            m.tile.at(x, y) = (x < 6) ? kRed : kBlue;
    }
    m.enableIndexed(false);
    m.applyFilter([](const Grid<Color>& g) { return fx::blur(g, 3, false); }, false);
    m.applyFilter([](const Grid<Color>& g) { return fx::noise(g, 40, false, 3); }, false);
    ASSERT_TRUE(m.rotateImageBy(30, img::ROT_SMOOTH, true));
    ASSERT_TRUE(m.scaleImageTo(20, 17, true));
    std::set<std::uint32_t> pal;
    for (const Color c : m.palette)
        pal.insert(c.argb());
    for (const Color c : m.tile.d) {
        if (c.a == 0) continue;
        EXPECT_TRUE(pal.count(c.argb()) == 1) << std::hex << c.argb();
    }
}

TEST(ModelFx, AirbrushBuildsUpAlongStroke) {
    Model m = spriteModel(32, 16);
    m.firstColor = kRed;
    m.penMode = PEN_AIRBRUSH;
    m.airSize = 6;
    m.airHardness = 0;
    m.airFlow = 30;
    m.pushUndo();
    m.airbrushBegin(4, 8);
    m.airbrushTo(28, 8);
    m.airbrushEnd();
    // Середина мазка — красная и плотнее края; за мазком — пусто.
    EXPECT_EQ(m.tile.at(16, 8).r, 255);
    EXPECT_GT(m.tile.at(16, 8).a, m.tile.at(16, 10).a);
    EXPECT_GT(m.tile.at(16, 8).a, 100);
    EXPECT_EQ(m.tile.at(16, 0).a, 0);
    // Тот же путь двумя кусками кладёт те же отпечатки: напор не зависит
    // от того, как мышь разбила движение.
    Model n = spriteModel(32, 16);
    n.firstColor = kRed;
    n.penMode = PEN_AIRBRUSH;
    n.airSize = 6;
    n.airHardness = 0;
    n.airFlow = 30;
    n.airbrushBegin(4, 8);
    n.airbrushTo(15, 8);
    n.airbrushTo(28, 8);
    n.airbrushEnd();
    EXPECT_EQ(n.tile.d, m.tile.d);
}

TEST(ModelFx, AirbrushInStrictIndexedPaintsOnlyPaletteColors) {
    Model m = spriteModel(16, 16);
    m.strictIndexed = true;
    m.setPalette({kRed, kGreen, kBlue});
    m.enableIndexed(false);
    m.firstColor = kGreen;
    m.firstIdx = 1;
    m.airSize = 8;
    m.airFlow = 20;
    // Напор 20% копится, пока водят туда-сюда: смеси нет, точка просто
    // становится зелёной, когда напор перевалит за половину.
    m.airbrushBegin(6, 8);
    for (int k = 0; k < 6; ++k)
        m.airbrushTo(k % 2 == 0 ? 10 : 6, 8);
    m.airbrushEnd();
    int painted = 0;
    for (const Color c : m.tile.d) {
        if (c.a == 0) continue;
        EXPECT_EQ(c, kGreen);
        ++painted;
    }
    EXPECT_GT(painted, 0);
}

}  // namespace
}  // namespace tjtm
