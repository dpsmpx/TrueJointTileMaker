// Этап 1 RoadToProLevel: операции над картинкой и масками, команды модели.
//
// Эталона у этих функций нет — оригинал их не умел, — поэтому проверяются
// свойства: обратимость преобразований, точный результат на малых
// картинках, где его видно глазами, и то, что поведение 0.6.0 без
// включённых новинок не меняется.

#include <gtest/gtest.h>

#include "core/imageops.h"
#include "core/model.h"

namespace tjtm {
namespace {

using img::Mask;

Grid<int> numbered(int w, int h) {
    Grid<int> g(w, h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x)
            g.at(x, y) = y * 100 + x;
    }
    return g;
}

/// Картинка из строк: '.' — прозрачный, '#' — чёрный, 'r' — красный,
/// 'g' — зелёный, 'w' — белый.
Grid<Color> pic(std::initializer_list<const char*> rows) {
    const int h = static_cast<int>(rows.size());
    const int w = static_cast<int>(std::char_traits<char>::length(*rows.begin()));
    Grid<Color> g(w, h);
    int y = 0;
    for (const char* r : rows) {
        for (int x = 0; x < w; ++x) {
            switch (r[x]) {
                case '#': g.at(x, y) = argb(255, 0, 0, 0); break;
                case 'r': g.at(x, y) = argb(255, 255, 0, 0); break;
                case 'g': g.at(x, y) = argb(255, 0, 255, 0); break;
                case 'w': g.at(x, y) = argb(255, 255, 255, 255); break;
                default: g.at(x, y) = argb(0, 0, 0, 0); break;
            }
        }
        ++y;
    }
    return g;
}

Mask maskOf(std::initializer_list<const char*> rows) {
    const int h = static_cast<int>(rows.size());
    const int w = static_cast<int>(std::char_traits<char>::length(*rows.begin()));
    Mask m(w, h);
    int y = 0;
    for (const char* r : rows) {
        for (int x = 0; x < w; ++x)
            m.at(x, y) = r[x] == '#' ? 1 : 0;
        ++y;
    }
    return m;
}

TEST(ImageOps, RotationsAndFlipsCompose) {
    const Grid<int> g = numbered(5, 3);
    EXPECT_EQ(img::rotate90(img::rotate90(img::rotate90(img::rotate90(g)))), g);
    EXPECT_EQ(img::rotate270(img::rotate90(g)), g);
    EXPECT_EQ(img::rotate180(g), img::flipH(img::flipV(g)));
    EXPECT_EQ(img::rotate90(img::rotate90(g)), img::rotate180(g));
    const Grid<int> r = img::rotate90(g);
    EXPECT_EQ(r.w, 3);
    EXPECT_EQ(r.h, 5);
    // По часовой: левый нижний угол уходит в левый верхний.
    EXPECT_EQ(r.at(0, 0), g.at(0, 2));
    EXPECT_EQ(r.at(2, 0), g.at(0, 0));
}

TEST(ImageOps, CropAndCanvasAnchors) {
    const Grid<int> g = numbered(4, 4);
    const Grid<int> c = img::crop(g, 1, 2, 3, 3, -1);
    EXPECT_EQ(c.at(0, 0), 201);
    EXPECT_EQ(c.at(2, 1), 303);
    EXPECT_EQ(c.at(0, 2), -1);  // за краем исходной
    // Холст 6x6 по центру: картинка сдвинута на (1, 1).
    const Grid<int> big = img::resizeCanvas(g, 6, 6, 1, 1, -1);
    EXPECT_EQ(big.at(0, 0), -1);
    EXPECT_EQ(big.at(1, 1), 0);
    EXPECT_EQ(big.at(4, 4), 303);
    // К правому нижнему краю.
    const Grid<int> br = img::resizeCanvas(g, 6, 5, 2, 2, -1);
    EXPECT_EQ(br.at(2, 1), 0);
    EXPECT_EQ(br.at(5, 4), 303);
    // Уменьшение по центру срезает поровну.
    const Grid<int> small = img::resizeCanvas(g, 2, 2, 1, 1, -1);
    EXPECT_EQ(small.at(0, 0), 101);
    EXPECT_EQ(small.at(1, 1), 202);
}

TEST(ImageOps, OffsetWrapsAround) {
    const Grid<int> g = numbered(4, 2);
    const Grid<int> o = img::offsetWrap(g, 1, 0);
    EXPECT_EQ(o.at(1, 0), g.at(0, 0));
    EXPECT_EQ(o.at(0, 0), g.at(3, 0));
    EXPECT_EQ(img::offsetWrap(img::offsetWrap(g, 2, 1), 2, 1), g);
    EXPECT_EQ(img::offsetWrap(g, -5, 3), img::offsetWrap(g, 3, 1));
}

TEST(ImageOps, ScaleNearestIsExactForIntegers) {
    const Grid<int> g = numbered(3, 2);
    const Grid<int> up = img::scaleNearest(g, 9, 6);
    for (int y = 0; y < 6; ++y) {
        for (int x = 0; x < 9; ++x)
            EXPECT_EQ(up.at(x, y), g.at(x / 3, y / 3));
    }
    EXPECT_EQ(img::scaleNearest(up, 3, 2), g);
}

TEST(ImageOps, MaskShapes) {
    EXPECT_EQ(img::maskCount(img::maskRect(10, 10, 7, 8, 2, 3)), 6 * 6);
    // Овал симметричен и вписан в рамку.
    const Mask e = img::maskEllipse(9, 9, 0, 0, 8, 8);
    EXPECT_EQ(e, img::flipH(e));
    EXPECT_EQ(e, img::flipV(e));
    EXPECT_EQ(e.at(4, 0), 1);
    EXPECT_EQ(e.at(0, 0), 0);
    // Лассо по углам квадрата — сам квадрат.
    const Mask p = img::maskPolygon(8, 8, {{1, 1}, {5, 1}, {5, 5}, {1, 5}});
    EXPECT_EQ(p, img::maskRect(8, 8, 1, 1, 5, 5));
    // Треугольник: путь сплошной, острый угол не теряется.
    const Mask t = img::maskPolygon(8, 8, {{0, 0}, {7, 0}, {0, 7}});
    EXPECT_EQ(t.at(0, 7), 1);
    EXPECT_EQ(t.at(7, 0), 1);
    EXPECT_EQ(t.at(7, 7), 0);
    EXPECT_EQ(t.at(2, 2), 1);
}

TEST(ImageOps, MaskCombineAndGrow) {
    const Mask a = maskOf({"##..", "##..", "....", "...."});
    const Mask b = maskOf({".##.", ".##.", "....", "...."});
    Mask m = a;
    img::combine(m, b, img::SEL_ADD);
    EXPECT_EQ(m, maskOf({"###.", "###.", "....", "...."}));
    m = a;
    img::combine(m, b, img::SEL_SUBTRACT);
    EXPECT_EQ(m, maskOf({"#...", "#...", "....", "...."}));
    m = a;
    img::combine(m, b, img::SEL_INTERSECT);
    EXPECT_EQ(m, maskOf({".#..", ".#..", "....", "...."}));
    m = a;
    img::combine(m, b, img::SEL_REPLACE);
    EXPECT_EQ(m, b);
    // Вычесть из пустоты — пустота.
    Mask none;
    img::combine(none, b, img::SEL_SUBTRACT);
    EXPECT_EQ(img::maskCount(none), 0);

    EXPECT_EQ(img::invert(img::invert(a)), a);
    const Mask r = img::maskRect(7, 7, 1, 1, 5, 5);
    EXPECT_EQ(img::grow(r, -1), img::maskRect(7, 7, 2, 2, 4, 4));
    EXPECT_EQ(img::grow(img::grow(r, -1), 1), r);
    // Сужение у края картинки: край считается невыделенным.
    EXPECT_EQ(img::grow(img::maskRect(3, 3, 0, 0, 2, 2), -1), img::maskRect(3, 3, 1, 1, 1, 1));
}

TEST(ImageOps, MaskByColor) {
    const Grid<Color> g = pic({"rr.r", "r..r", "rrrr", "g..g"});
    const Mask wand = img::maskByColor(g, 0, 0, true);
    EXPECT_EQ(wand, maskOf({"##.#", "#..#", "####", "...."}));
    const Mask glob = img::maskByColor(g, 0, 3, false);
    EXPECT_EQ(glob, maskOf({"....", "....", "....", "#..#"}));
    // Палочка не перепрыгивает: зелёные точки не связаны.
    EXPECT_EQ(img::maskByColor(g, 0, 3, true), maskOf({"....", "....", "....", "#..."}));
    EXPECT_EQ(img::maskOpaque(g), maskOf({"##.#", "#..#", "####", "#..#"}));
}

TEST(ImageOps, ContentBounds) {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    EXPECT_FALSE(img::contentBounds(pic({"...", "..."}), x, y, w, h));
    ASSERT_TRUE(img::contentBounds(pic({".....", "..#..", ".#...", "....."}), x, y, w, h));
    EXPECT_EQ(x, 1);
    EXPECT_EQ(y, 1);
    EXPECT_EQ(w, 2);
    EXPECT_EQ(h, 2);
}

TEST(ImageOps, Outline) {
    Grid<Color> g = pic({".....", ".....", "..#..", ".....", "....."});
    EXPECT_EQ(img::outline(g, Mask{}, argb(255, 255, 0, 0), false, true), 4);
    EXPECT_EQ(g, pic({".....", "..r..", ".r#r.", "..r..", "....."}));
    g = pic({".....", ".....", "..#..", ".....", "....."});
    EXPECT_EQ(img::outline(g, Mask{}, argb(255, 255, 0, 0), true, true), 8);
    // Внутренний контур квадрата 3x3 — его рамка.
    g = pic({".....", ".###.", ".###.", ".###.", "....."});
    EXPECT_EQ(img::outline(g, Mask{}, argb(255, 255, 0, 0), false, false), 8);
    EXPECT_EQ(g, pic({".....", ".rrr.", ".r#r.", ".rrr.", "....."}));
    // Только в пределах маски.
    g = pic({".....", ".....", "..#..", ".....", "....."});
    EXPECT_EQ(img::outline(g, img::maskRect(5, 5, 0, 0, 4, 1), argb(255, 255, 0, 0), false, true),
              1);
}

TEST(ImageOps, RemoveIsolated) {
    Grid<Color> g = pic({"www", "w#w", "www"});
    EXPECT_EQ(img::removeIsolated(g, Mask{}), 1);
    EXPECT_EQ(g, pic({"www", "www", "www"}));
    // Пара точек — уже не шум.
    g = pic({"wwww", "w##w", "wwww"});
    EXPECT_EQ(img::removeIsolated(g, Mask{}), 0);
    // Точка в углу: соседей три, все белые.
    g = pic({"#w", "ww"});
    EXPECT_EQ(img::removeIsolated(g, Mask{}), 1);
}

TEST(ImageOps, DitherPatterns) {
    ASSERT_GE(img::ditherPatternCount(), 12);
    // Нулевой — шахматка оригинала.
    for (int y = -3; y < 9; ++y) {
        for (int x = -3; x < 9; ++x)
            EXPECT_EQ(img::ditherFirst(0, x, y), ((x + y) % 2 + 2) % 2 == 0) << x << "," << y;
    }
    // Доли первого цвета на периоде 24x24 (кратен всем узорам).
    auto share = [](int p) {
        int n = 0;
        for (int y = 0; y < 24; ++y) {
            for (int x = 0; x < 24; ++x)
                n += img::ditherFirst(p, x, y) ? 1 : 0;
        }
        return n * 100 / (24 * 24);
    };
    EXPECT_EQ(share(0), 50);
    EXPECT_EQ(share(1), 25);
    EXPECT_EQ(share(2), 75);
    EXPECT_EQ(share(3), 12);
    EXPECT_EQ(share(4), 87);
    for (int p = 0; p < img::ditherPatternCount(); ++p)
        EXPECT_FALSE(img::ditherPatternName(p).empty());
}

TEST(ImageOps, PixelPerfectCorner) {
    EXPECT_TRUE(img::isPixelPerfectCorner(0, 0, 1, 0, 1, 1));
    EXPECT_TRUE(img::isPixelPerfectCorner(0, 0, 0, 1, 1, 1));
    EXPECT_FALSE(img::isPixelPerfectCorner(0, 0, 1, 0, 2, 0));  // прямая
    EXPECT_FALSE(img::isPixelPerfectCorner(0, 0, 1, 1, 2, 2));  // диагональ
    EXPECT_FALSE(img::isPixelPerfectCorner(0, 0, 1, 0, 0, 0));  // назад
}

// --- модель -------------------------------------------------------------------

/// Модель с новинками этапа 1, как её включает окно, и прозрачным тайлом.
Model tileModel(int w, int h) {
    Model m;
    m.clipToSelection = true;
    m.transparentBlank = true;
    m.newTileOfSize(w, h);
    return m;
}

TEST(ModelPro, DefaultsKeepOriginalBehaviour) {
    Model m;
    m.newTileOfSize(8, 8);
    EXPECT_EQ(m.tile.at(0, 0), argb(255, 255, 255, 255));
    m.transparentBlank = true;
    m.clearTile();
    EXPECT_EQ(m.tile.at(3, 3).a, 0);
    EXPECT_EQ(m.tileIdx.at(3, 3), IDX_NONE);
    EXPECT_EQ(m.mirrorX(0), 7);
    EXPECT_EQ(m.mirrorY(2), 5);
}

TEST(ModelPro, SelectionMaskStoresRectAsRect) {
    Model m = tileModel(8, 8);
    m.setSelectionMask(img::maskRect(8, 8, 2, 3, 4, 6));
    EXPECT_TRUE(m.hasSelection);
    EXPECT_TRUE(m.selMask.empty());
    EXPECT_EQ(m.selX, 2);
    EXPECT_EQ(m.selH, 4);
    m.applySelection(img::maskRect(8, 8, 4, 3, 6, 3), img::SEL_ADD);
    EXPECT_FALSE(m.selMask.empty());
    EXPECT_EQ(m.selW, 5);
    EXPECT_TRUE(m.isSelected(6, 3));
    EXPECT_FALSE(m.isSelected(6, 4));
    m.applySelection(img::maskRect(8, 8, 0, 0, 7, 7), img::SEL_SUBTRACT);
    EXPECT_FALSE(m.hasSelection);
    m.selectAll();
    EXPECT_EQ(img::maskCount(m.selectionMask()), 64);
    m.deselect();
    EXPECT_FALSE(m.hasSelection);
    EXPECT_TRUE(m.reselect());
    EXPECT_EQ(img::maskCount(m.selectionMask()), 64);
    m.invertSelection();
    EXPECT_FALSE(m.hasSelection);
}

TEST(ModelPro, PaintAndFillStayInsideSelection) {
    Model m = tileModel(6, 6);
    m.brushReplace = true;
    m.setSelectionMask(img::maskRect(6, 6, 1, 1, 3, 3));
    m.beginStroke();
    m.shapeLine(0, 2, 5, 2);
    m.endStroke();
    for (int x = 0; x < 6; ++x)
        EXPECT_EQ(m.tile.at(x, 2).a == 255, x >= 1 && x <= 3) << x;
    m.floodFillTile(2, 1, argb(255, 255, 0, 0));
    EXPECT_EQ(m.tile.at(1, 1), argb(255, 255, 0, 0));
    EXPECT_EQ(m.tile.at(0, 0).a, 0);  // за выделением заливка не пошла
    // Без выделения — как в оригинале.
    m.deselect();
    m.floodFillTile(0, 0, argb(255, 0, 255, 0));
    EXPECT_EQ(m.tile.at(5, 5), argb(255, 0, 255, 0));
}

TEST(ModelPro, CutPasteAndErase) {
    Model m = tileModel(6, 6);
    m.tile.at(1, 1) = argb(255, 255, 0, 0);
    m.tile.at(2, 1) = argb(255, 0, 0, 255);
    m.setSelectionMask(img::maskRect(6, 6, 1, 1, 2, 1));
    ASSERT_TRUE(m.cutSelection());
    EXPECT_EQ(m.tile.at(1, 1).a, 0);
    EXPECT_EQ(m.buffer.w, 2);
    m.pasteAsSelection(3, 4);
    EXPECT_EQ(m.tile.at(3, 4), argb(255, 255, 0, 0));
    EXPECT_EQ(m.tile.at(4, 4), argb(255, 0, 0, 255));
    EXPECT_TRUE(m.hasSelection);
    EXPECT_EQ(m.selX, 3);
    EXPECT_EQ(m.selW, 2);
    m.doUndo();
    m.doUndo();
    EXPECT_EQ(m.tile.at(1, 1), argb(255, 255, 0, 0));
}

TEST(ModelPro, TransformWholeImageAndSelection) {
    Model m = tileModel(6, 4);
    m.tile.at(0, 0) = argb(255, 255, 0, 0);
    m.ensureTileIdx();
    m.tileIdx.at(0, 0) = 7;
    m.transformImage(TF_ROT90);
    EXPECT_EQ(m.tile.w, 4);
    EXPECT_EQ(m.tile.h, 6);
    EXPECT_EQ(m.tile.at(3, 0), argb(255, 255, 0, 0));
    EXPECT_EQ(m.tileIdx.at(3, 0), 7);  // привязка едет вместе с цветом
    m.doUndo();
    EXPECT_EQ(m.tile.w, 6);

    // Отражение выделения — в его же рамке, остальное не тронуто.
    Model s = tileModel(6, 6);
    s.tile.at(1, 2) = argb(255, 255, 0, 0);
    s.tile.at(5, 5) = argb(255, 0, 255, 0);
    s.setSelectionMask(img::maskRect(6, 6, 1, 2, 3, 2));
    s.transformImage(TF_FLIP_H);
    EXPECT_EQ(s.tile.at(1, 2).a, 0);
    EXPECT_EQ(s.tile.at(3, 2), argb(255, 255, 0, 0));
    EXPECT_EQ(s.tile.at(5, 5), argb(255, 0, 255, 0));
    EXPECT_TRUE(s.hasSelection);
    EXPECT_EQ(s.selX, 1);
}

TEST(ModelPro, MovedPieceFloatsOverPicture) {
    Model m = tileModel(8, 4);
    for (int x = 0; x < 8; ++x)
        m.tile.at(x, 0) = argb(255, 0, 255, 0);  // зелёная полоса — «картинка»
    m.tile.at(0, 1) = argb(255, 255, 0, 0);      // красная точка — «кусок»
    m.setSelectionMask(img::maskRect(8, 4, 0, 1, 0, 1));
    Grid<Color> c;
    Grid<std::uint8_t> k;
    m.liftSelection(true, c, k);
    m.placeFloating(c, k, 3, 0);  // положили на полосу
    EXPECT_EQ(m.tile.at(3, 0), argb(255, 255, 0, 0));
    m.liftSelection(true, c, k);
    m.placeFloating(c, k, 5, 0);                       // и сдвинули дальше
    EXPECT_EQ(m.tile.at(3, 0), argb(255, 0, 255, 0));  // полоса под первой вставкой цела
    EXPECT_EQ(m.tile.at(5, 0), argb(255, 255, 0, 0));
    EXPECT_EQ(m.tile.at(0, 1).a, 0);  // а исходное место — пустое
    // Кто-то порисовал — кусок больше не «плавает», под ним дыра.
    m.tile.at(7, 1) = argb(255, 0, 0, 255);
    m.liftSelection(true, c, k);
    EXPECT_EQ(m.tile.at(5, 0).a, 0);
}

TEST(ModelPro, ScaleCropAndCanvas) {
    Model m = tileModel(8, 8);
    m.tile.at(2, 3) = argb(255, 255, 0, 0);
    ASSERT_TRUE(m.scaleBy(2, 1));
    EXPECT_EQ(m.tile.w, 16);
    EXPECT_EQ(m.tile.at(5, 7), argb(255, 255, 0, 0));
    ASSERT_TRUE(m.cropToContent());
    EXPECT_EQ(m.tile.w, 2);
    EXPECT_EQ(m.tile.h, 2);
    ASSERT_TRUE(m.resizeCanvas(6, 6, 1, 1));
    EXPECT_EQ(m.tile.at(2, 2), argb(255, 255, 0, 0));
    EXPECT_EQ(m.tile.at(0, 0).a, 0);
    EXPECT_FALSE(m.scaleBy(1000, 1));  // больше предела — отказ без шага истории
    m.setSelectionMask(img::maskRect(6, 6, 2, 2, 3, 3));
    ASSERT_TRUE(m.cropToSelection());
    EXPECT_EQ(m.tile.w, 2);
    EXPECT_FALSE(m.hasSelection);
}

TEST(ModelPro, PixelPerfectStrokeDropsCorners) {
    Model m = tileModel(8, 8);
    m.brushReplace = true;
    m.pixelPerfect = true;
    m.pushUndo();
    m.beginStroke();
    m.emitPixel(0, 0);
    m.shapeLine(0, 0, 1, 0);
    m.shapeLine(1, 0, 1, 1);
    m.shapeLine(1, 1, 2, 1);
    m.shapeLine(2, 1, 2, 2);
    m.endStroke();
    // Лесенка без углов «Г»: остались только диагональные шаги.
    EXPECT_EQ(m.tile.at(0, 0).a, 255);
    EXPECT_EQ(m.tile.at(1, 0).a, 0);
    EXPECT_EQ(m.tile.at(1, 1).a, 255);
    EXPECT_EQ(m.tile.at(2, 1).a, 0);
    EXPECT_EQ(m.tile.at(2, 2).a, 255);

    // Без режима — как в оригинале, все точки на месте.
    Model o = tileModel(8, 8);
    o.brushReplace = true;
    o.beginStroke();
    o.shapeLine(0, 0, 1, 0);
    o.shapeLine(1, 0, 1, 1);
    o.endStroke();
    EXPECT_EQ(o.tile.at(1, 0).a, 255);
}

TEST(ModelPro, PixelPerfectRestoresMirrorsToo) {
    Model m = tileModel(8, 8);
    m.brushReplace = true;
    m.pixelPerfect = true;
    m.symX = true;
    m.beginStroke();
    m.shapeLine(0, 0, 1, 0);
    m.shapeLine(1, 0, 1, 1);
    m.endStroke();
    EXPECT_EQ(m.tile.at(1, 0).a, 0);
    EXPECT_EQ(m.tile.at(6, 0).a, 0);  // зеркало угла тоже снято
    EXPECT_EQ(m.tile.at(7, 0).a, 255);
    EXPECT_EQ(m.tile.at(6, 1).a, 255);
}

TEST(ModelPro, MovableSymmetryAxis) {
    Model m = tileModel(8, 8);
    m.brushReplace = true;
    m.symX = true;
    m.symAxisX2 = 5;  // ось через середину точки 2
    m.beginStroke();
    m.emitPixel(0, 0);
    m.emitPixel(2, 1);
    m.endStroke();
    EXPECT_EQ(m.tile.at(4, 0).a, 255);
    EXPECT_EQ(m.tile.at(2, 1).a, 255);  // точка на оси отражается сама в себя
    EXPECT_EQ(m.mirrorX(4), 0);
}

TEST(ModelPro, DitherPatternBrush) {
    Model m = tileModel(4, 4);
    m.brushReplace = true;
    m.toolTile = T_DITHER;
    m.ditherPattern = 5;  // строки
    m.firstColor = argb(255, 255, 0, 0);
    m.secondColor = argb(255, 0, 0, 255);
    m.beginStroke();
    m.shapeLine(0, 0, 0, 3);
    m.endStroke();
    EXPECT_EQ(m.tile.at(0, 0), m.firstColor);
    EXPECT_EQ(m.tile.at(0, 1), m.secondColor);
    EXPECT_EQ(m.tile.at(0, 2), m.firstColor);
}

TEST(ModelPro, OutlineAndCleanupMakeOneUndoStep) {
    Model m = tileModel(5, 5);
    m.tile.at(2, 2) = argb(255, 0, 0, 0);
    const std::size_t before = m.undoStack.size();
    EXPECT_EQ(m.outlineImage(argb(255, 255, 0, 0), false, true), 4);
    EXPECT_EQ(m.undoStack.size(), before + 1);
    EXPECT_EQ(m.removeIsolatedPixels(), 0);  // нечего чистить — шага нет
    EXPECT_EQ(m.undoStack.size(), before + 1);
}

}  // namespace
}  // namespace tjtm
