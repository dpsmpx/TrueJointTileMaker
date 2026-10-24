// Этап 6 RoadToProLevel: группы слоёв, векторные фигуры, слой-тайлкарта.

#include <gtest/gtest.h>

#include "core/model.h"
#include "core/project.h"
#include "core/tilemap.h"
#include "core/vector.h"

namespace tjtm {
namespace {

constexpr Color kRed = argb(255, 255, 0, 0);
constexpr Color kGreen = argb(255, 0, 255, 0);
constexpr Color kBlue = argb(255, 0, 0, 255);
constexpr Color kClear = argb(0, 0, 0, 0);

Model sprite(int w = 8, int h = 8) {
    Model m;
    m.transparentBlank = true;
    m.brushReplace = true;
    m.newTileOfSize(w, h);
    return m;
}

void dot(Model& m, int x, int y, Color c) {
    m.firstColor = c;
    m.firstIdx = IDX_NONE;
    m.beginStroke();
    m.emitPixel(x, y);
    m.endStroke();
}

// --- группы ------------------------------------------------------------------

TEST(Groups, GroupHidesLocksAndBlendsChildren) {
    Model m = sprite();
    dot(m, 0, 0, kRed);  // слой 1
    m.addLayer();
    dot(m, 0, 0, kBlue);           // слой 2 поверх
    const int g = m.groupLayer();  // слой 2 — в группе
    ASSERT_EQ(g, 2);
    EXPECT_EQ(m.doc.layers[1].depth, 1);
    EXPECT_EQ(m.layerParent(1), 2);
    EXPECT_EQ(m.compositeAt(0, 0, 0).argb(), kBlue.argb());
    // Скрытая группа прячет детей, запертая — запирает.
    m.setLayerVisible(g, false);
    EXPECT_EQ(m.compositeAt(0, 0, 0).argb(), kRed.argb());
    m.setLayerVisible(g, true);
    m.setLayerLocked(g, true);
    m.setActiveCel(1, 0);
    EXPECT_FALSE(m.activeLayerEditable());
    m.setLayerLocked(g, false);
    EXPECT_TRUE(m.activeLayerEditable());
    // Непрозрачность группы действует на сведение детей разом.
    m.setLayerOpacity(g, 128);
    const Color c = m.compositeAt(0, 0, 0);
    EXPECT_GT(c.r, 100);
    EXPECT_GT(c.b, 100);
}

TEST(Groups, UngroupIndentOutdentAndMoveBlocks) {
    Model m = sprite();
    m.addLayer();  // 1
    m.addLayer();  // 2
    m.setActiveCel(1, 0);
    const int g = m.groupLayer();  // слой 1 в группу: [0][1 в группе][группа 2][3]
    EXPECT_EQ(m.doc.layerCount(), 4);
    EXPECT_EQ(m.doc.layers[static_cast<std::size_t>(g)].kind, LK_GROUP);
    // Верхний слой 3 — в группу под ним нельзя (группа ниже), а слой 0
    // внутрь — тоже: группа над ним, но сначала идёт её ребёнок.
    m.setActiveCel(3, 0);
    EXPECT_FALSE(m.indentLayer());
    // Группа уходит вниз целиком, вместе с ребёнком.
    m.setActiveCel(g, 0);
    ASSERT_TRUE(m.moveLayer(-1));
    EXPECT_EQ(m.doc.layers[0].depth, 1);
    EXPECT_EQ(m.doc.layers[1].kind, LK_GROUP);
    // Слой 2 (бывший 0) — в группу под ним нельзя, над ним — слой 3.
    m.setActiveCel(2, 0);
    ASSERT_TRUE(m.moveLayer(-1));  // меняется с группой
    EXPECT_EQ(m.doc.layers[0].depth, 0);
    EXPECT_EQ(m.doc.layers[2].kind, LK_GROUP);
    ASSERT_TRUE(m.indentLayer());  // слой 0 — в группу над ним, верхним
    EXPECT_EQ(m.doc.layers[1].depth, 1);
    EXPECT_EQ(m.layerParent(1), 2);
    ASSERT_TRUE(m.outdentLayer());
    EXPECT_EQ(m.doc.layers[2].depth, 0);
    m.setActiveCel(1, 0);
    ASSERT_EQ(m.doc.layers[1].kind, LK_GROUP);
    ASSERT_TRUE(m.ungroupLayer());
    for (const Layer& l : m.doc.layers)
        EXPECT_EQ(l.depth, 0);
    m.doUndo();
    EXPECT_EQ(m.doc.layers[1].kind, LK_GROUP);
}

TEST(Groups, DeleteDuplicateAndFlattenSubtree) {
    Model m = sprite();
    m.addLayer();
    dot(m, 1, 1, kGreen);
    m.groupLayer();
    m.setLayerOpacity(2, 128);
    const Color shown = m.compositeAt(0, 1, 1);
    ASSERT_EQ(m.duplicateLayer(), 4);  // копия группы с ребёнком
    EXPECT_EQ(m.doc.layerCount(), 5);
    EXPECT_EQ(m.doc.layers[3].depth, 1);
    ASSERT_TRUE(m.deleteLayer());
    EXPECT_EQ(m.doc.layerCount(), 3);
    m.setActiveCel(2, 0);
    ASSERT_TRUE(m.flattenGroup());
    EXPECT_EQ(m.doc.layerCount(), 2);
    EXPECT_EQ(m.doc.layers[1].kind, LK_RASTER);
    EXPECT_EQ(m.compositeAt(0, 1, 1).argb(), shown.argb());
}

// --- вектор ------------------------------------------------------------------

VecShape line(Color c, std::initializer_list<VecPoint> pts, int type = VS_POLYLINE) {
    VecShape s;
    s.type = type;
    s.pts = pts;
    s.stroke = c;
    return s;
}

TEST(Vector, ShapesRasterizeAsPixels) {
    Grid<Color> g(10, 10, kClear);
    vec::rasterize(line(kRed, {{0.5, 0.5}, {9.5, 0.5}}), g);
    for (int x = 0; x < 10; ++x)
        EXPECT_EQ(g.at(x, 0).argb(), kRed.argb());
    // Диагональ без углов «Г»: по точке в строке.
    Grid<Color> d(6, 6, kClear);
    vec::rasterize(line(kRed, {{0.5, 0.5}, {5.5, 5.5}}), d);
    int n = 0;
    for (const Color c : d.d)
        n += c.a != 0 ? 1 : 0;
    EXPECT_EQ(n, 6);
    // Залитый прямоугольник без контура включает край.
    VecShape r = line(kClear, {{1.5, 1.5}, {4.5, 3.5}}, VS_RECT);
    r.fill = kBlue;
    Grid<Color> f(8, 8, kClear);
    vec::rasterize(r, f);
    EXPECT_EQ(f.at(1, 1).argb(), kBlue.argb());
    EXPECT_EQ(f.at(4, 3).argb(), kBlue.argb());
    EXPECT_EQ(f.at(5, 3).a, 0);
    // Кривая проходит через свои точки.
    Grid<Color> c(16, 16, kClear);
    vec::rasterize(line(kGreen, {{1.5, 8.5}, {8.5, 2.5}, {14.5, 8.5}}, VS_CURVE), c);
    EXPECT_EQ(c.at(8, 2).argb(), kGreen.argb());
    EXPECT_EQ(c.at(14, 8).argb(), kGreen.argb());
    // Овал и звезда — замкнутые, попадание — по пути.
    VecShape e = line(kRed, {{0.5, 0.5}, {9.5, 5.5}}, VS_ELLIPSE);
    Grid<Color> eg(10, 6, kClear);
    vec::rasterize(e, eg);
    EXPECT_EQ(eg.at(5, 0).argb(), kRed.argb());
    EXPECT_EQ(eg.at(0, 0).a, 0);
    EXPECT_EQ(vec::hitShape({e}, 5.0, 0.6, 1.0), 0);
    int shape = -1;
    int vertex = -1;
    EXPECT_TRUE(vec::hitVertex({e}, 9.4, 5.4, 0.5, shape, vertex));
    EXPECT_EQ(vertex, 1);
}

TEST(Vector, LayerKeepsShapesAndTransformsThem) {
    Model m = sprite(10, 10);
    m.addVectorLayer();
    ASSERT_EQ(m.activeKind(), LK_VECTOR);
    ASSERT_EQ(m.addVectorShape(line(kRed, {{0.5, 0.5}, {3.5, 0.5}})), 0);
    EXPECT_EQ(m.tile.at(3, 0).argb(), kRed.argb());
    VecShape moved = m.tileShapes[0];
    vec::translate(moved, 0, 2);
    m.setVectorShape(0, moved);
    EXPECT_EQ(m.tile.at(3, 2).argb(), kRed.argb());
    EXPECT_EQ(m.tile.at(3, 0).a, 0);
    // Отражение двигает точки фигур, а не только растр.
    m.transformImage(TF_FLIP_H);
    ASSERT_EQ(m.tileShapes.size(), 1U);
    EXPECT_DOUBLE_EQ(m.tileShapes[0].pts[0].x, 9.5);
    m.rerenderActiveVector();
    EXPECT_EQ(m.tile.at(9, 2).argb(), kRed.argb());
    // Сдвиг с переносом фигуры бы разорвал: слой становится растром.
    m.offsetImage(1, 0);
    EXPECT_EQ(m.activeKind(), LK_RASTER);
    EXPECT_TRUE(m.tileShapes.empty());
    m.doUndo();
    EXPECT_EQ(m.activeKind(), LK_VECTOR);
    ASSERT_TRUE(m.deleteVectorShape(0));
    EXPECT_EQ(m.tile.at(9, 2).a, 0);
}

TEST(Vector, RasterLayerTakesShapeAsPixels) {
    Model m = sprite(8, 8);
    EXPECT_EQ(m.addVectorShape(line(kBlue, {{0.5, 2.5}, {7.5, 2.5}})), -1);
    EXPECT_EQ(m.tile.at(7, 2).argb(), kBlue.argb());
    EXPECT_TRUE(m.tileShapes.empty());
}

// --- тайлкарта ---------------------------------------------------------------

TEST(Tilemap, ConvertDedupesAndPixelEditsReachCopies) {
    Model m = sprite(8, 4);
    // Две одинаковые клетки 4x4 и одна пустая не нужна: тайл 1 дважды.
    for (int cx = 0; cx < 2; ++cx) {
        m.tile.at(cx * 4 + 1, 1) = kRed;
    }
    ASSERT_TRUE(m.convertLayerToTilemap(4, 4));
    ASSERT_EQ(m.activeKind(), LK_TILEMAP);
    const Tileset* ts = m.activeTileset();
    ASSERT_NE(ts, nullptr);
    EXPECT_EQ(ts->tiles.size(), 2U);  // пустой и один настоящий
    EXPECT_EQ(m.tileMap.at(0, 0), 1);
    EXPECT_EQ(m.tileMap.at(1, 0), 1);
    // Правка точками одной копии меняет обе.
    m.tilemapPixels = true;
    m.pushUndo();
    m.tile.at(2, 2) = kGreen;
    m.syncActiveTilemap();
    EXPECT_EQ(m.tile.at(6, 2).argb(), kGreen.argb());
    m.doUndo();
    EXPECT_EQ(m.tile.at(6, 2).a, 0);
}

TEST(Tilemap, StampFillPickAndDelete) {
    Model m = sprite(8, 8);
    m.addTilemapLayer(4, 4);
    const int t = m.addTileToActiveSet(Grid<Color>(4, 4, kBlue));
    ASSERT_EQ(t, 1);
    m.tileBrush = 1;
    m.toolTile = T_PEN;
    m.beginStroke();
    m.emitPixel(5, 1);
    m.endStroke();
    EXPECT_EQ(m.tileIndexAt(5, 1), 1);
    EXPECT_EQ(m.tile.at(4, 0).argb(), kBlue.argb());
    m.fillTilesAt(0, 7);
    for (int cy = 0; cy < 2; ++cy) {
        for (int cx = 0; cx < 2; ++cx)
            EXPECT_EQ(m.tileMap.at(cx, cy), 1);
    }
    ASSERT_TRUE(m.deleteTileFromActiveSet(1));
    EXPECT_EQ(m.tileIndexAt(0, 0), 0);
    EXPECT_EQ(m.tile.at(0, 0).a, 0);
}

TEST(Tilemap, ColorCommandRecolorsTilesetAndShapes) {
    Model m = sprite(8, 8);
    m.addTilemapLayer(4, 4);
    m.addTileToActiveSet(Grid<Color>(4, 4, kBlue));
    m.setTileCell(0, 0, 1);
    m.addVectorLayer();
    m.addVectorShape(line(kBlue, {{0.5, 7.5}, {7.5, 7.5}}));
    // Цветовая правка по всему документу: синий → красный.
    m.forEachCel([](Grid<Color>& img, Grid<PalIndex>&) {
        for (Color& c : img.d) {
            if (c.argb() == kBlue.argb()) c = kRed;
        }
    });
    EXPECT_EQ(m.tileShapes[0].stroke.argb(), kRed.argb());
    m.setActiveCel(1, 0);
    EXPECT_EQ(m.activeTileset()->tiles[1].at(0, 0).argb(), kRed.argb());
    EXPECT_EQ(m.tile.at(0, 0).argb(), kRed.argb());
}

// --- проект ------------------------------------------------------------------

TEST(Project, GroupsVectorsAndTilemapsRoundTrip) {
    Model m = sprite(8, 8);
    dot(m, 0, 0, kRed);
    m.addVectorLayer();
    m.addVectorShape(line(kGreen, {{0.5, 4.5}, {7.5, 4.5}}));
    m.groupLayer();
    m.setLayerCollapsed(2, true);
    m.addTilemapLayer(4, 4);
    m.addTileToActiveSet(Grid<Color>(4, 4, kBlue));
    m.setTileCell(1, 1, 1);
    const auto bytes = projectEncode(m);
    Model b;
    std::string err;
    ASSERT_TRUE(projectDecode(bytes, b, err)) << err;
    ASSERT_EQ(b.doc.layerCount(), 4);
    EXPECT_EQ(b.doc.layers[1].kind, LK_VECTOR);
    EXPECT_EQ(b.doc.layers[1].depth, 1);
    EXPECT_EQ(b.doc.layers[2].kind, LK_GROUP);
    EXPECT_TRUE(b.doc.layers[2].collapsed);
    EXPECT_EQ(b.doc.layers[3].kind, LK_TILEMAP);
    ASSERT_EQ(b.doc.tilesets.size(), 1U);
    EXPECT_EQ(b.doc.tilesets[0]->tiles.size(), 2U);
    ASSERT_TRUE(b.doc.cel(1, 0));
    ASSERT_EQ(b.doc.cel(1, 0)->shapes.size(), 1U);
    EXPECT_DOUBLE_EQ(b.doc.cel(1, 0)->shapes[0].pts[1].x, 7.5);
    ASSERT_TRUE(b.doc.cel(3, 0));
    EXPECT_EQ(b.doc.cel(3, 0)->tiles.at(1, 1), 1);
    EXPECT_EQ(b.compositeAt(0, 5, 5).argb(), kBlue.argb());
    EXPECT_EQ(b.compositeAt(0, 3, 4).argb(), kGreen.argb());
}

}  // namespace
}  // namespace tjtm
