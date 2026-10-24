// Этапы 3 и 4 RoadToProLevel: слои и кадры в модели, проект .tjt.

#include <gtest/gtest.h>

#include <functional>
#include <iterator>

#include "core/imageops.h"
#include "core/model.h"
#include "core/png.h"
#include "core/project.h"
#include "core/zlib.h"

namespace tjtm {
namespace {

constexpr Color kRed = argb(255, 255, 0, 0);
constexpr Color kGreen = argb(255, 0, 255, 0);
constexpr Color kBlue = argb(255, 0, 0, 255);

Model spriteModel() {
    Model m;
    m.transparentBlank = true;
    m.brushReplace = true;
    m.newTileOfSize(8, 8);
    return m;
}

void dot(Model& m, int x, int y, Color c) {
    m.firstColor = c;
    m.firstIdx = IDX_NONE;
    m.beginStroke();
    m.emitPixel(x, y);
    m.endStroke();
}

TEST(Doc, SingleCelIsTheOriginalDocument) {
    Model m = spriteModel();
    EXPECT_EQ(m.doc.layerCount(), 1);
    EXPECT_EQ(m.doc.frameCount(), 1);
    EXPECT_FALSE(m.multiCel());
    dot(m, 1, 1, kRed);
    EXPECT_EQ(m.compositeAt(0, 1, 1), kRed);
}

TEST(Doc, LayersCompositeAndKeepTheirPixels) {
    Model m = spriteModel();
    dot(m, 1, 1, kRed);
    ASSERT_EQ(m.addLayer(), 1);
    EXPECT_EQ(m.tile.at(1, 1).a, 0);  // новый слой пуст
    dot(m, 1, 1, kGreen);
    dot(m, 2, 2, kBlue);
    EXPECT_EQ(m.compositeAt(0, 1, 1), kGreen);  // верхний поверх нижнего
    m.setLayerVisible(1, false);
    EXPECT_EQ(m.compositeAt(0, 1, 1), kRed);
    m.setLayerVisible(1, true);
    m.setLayerOpacity(1, 0);
    EXPECT_EQ(m.compositeAt(0, 1, 1), kRed);
    m.setLayerOpacity(1, 255);
    m.setActiveCel(0, 0);
    EXPECT_EQ(m.tile.at(1, 1), kRed);
    EXPECT_EQ(m.tile.at(2, 2).a, 0);
    // Скрытый и запертый слой не правится.
    m.setLayerLocked(0, true);
    dot(m, 3, 3, kRed);
    EXPECT_EQ(m.tile.at(3, 3).a, 0);
}

TEST(Doc, MergeDownAndMoveLayer) {
    Model m = spriteModel();
    dot(m, 0, 0, kRed);
    m.addLayer();
    dot(m, 1, 0, kGreen);
    ASSERT_TRUE(m.moveLayer(-1));
    EXPECT_EQ(m.activeLayer, 0);
    EXPECT_EQ(m.tile.at(1, 0), kGreen);
    ASSERT_TRUE(m.moveLayer(1));
    ASSERT_TRUE(m.mergeLayerDown());
    EXPECT_EQ(m.doc.layerCount(), 1);
    EXPECT_EQ(m.tile.at(0, 0), kRed);
    EXPECT_EQ(m.tile.at(1, 0), kGreen);
}

TEST(Doc, FramesCopyAndUndoReturnsToTheFrame) {
    Model m = spriteModel();
    dot(m, 0, 0, kRed);
    ASSERT_EQ(m.addFrame(true), 1);
    EXPECT_EQ(m.tile.at(0, 0), kRed);  // копия кадра
    m.pushUndo();
    dot(m, 0, 0, kBlue);
    m.setActiveCel(0, 0);
    EXPECT_EQ(m.tile.at(0, 0), kRed);  // копия не задела исходный кадр
    // Отмена правки кадра 1, сделанная на кадре 0, возвращает на кадр 1.
    m.doUndo();
    EXPECT_EQ(m.activeFrame, 1);
    EXPECT_EQ(m.tile.at(0, 0), kRed);
    ASSERT_EQ(m.addFrame(false), 2);
    EXPECT_EQ(m.tile.at(0, 0).a, 0);
    ASSERT_TRUE(m.moveFrame(-1));
    EXPECT_EQ(m.activeFrame, 1);
    ASSERT_TRUE(m.deleteFrame());
    EXPECT_EQ(m.doc.frameCount(), 2);
}

TEST(Doc, TagsFollowFrameEditsAndPlayback) {
    Model m = spriteModel();
    for (int k = 0; k < 4; ++k)
        m.addFrame(false);  // пять кадров
    ASSERT_EQ(m.addTag("бег", 1, 3, TAG_FORWARD), 0);
    int ping = 0;
    EXPECT_EQ(m.nextPlayFrame(3, 0, ping), 1);
    EXPECT_EQ(m.nextPlayFrame(0, 0, ping), 1);  // вне метки — к её началу
    EXPECT_EQ(m.nextPlayFrame(4, -1, ping), 0);
    m.doc.tags[0].direction = TAG_PINGPONG;
    ping = 0;
    int f = 1;
    std::vector<int> seq;
    for (int k = 0; k < 6; ++k) {
        f = m.nextPlayFrame(f, 0, ping);
        seq.push_back(f);
    }
    EXPECT_EQ(seq, (std::vector<int>{2, 3, 2, 1, 2, 3}));
    m.doc.tags[0].direction = TAG_REVERSE;
    EXPECT_EQ(m.nextPlayFrame(1, 0, ping), 3);
    // Вставка кадра внутри метки растягивает её.
    m.setActiveCel(0, 2);
    m.addFrame(false);
    EXPECT_EQ(m.doc.tags[0].from, 1);
    EXPECT_EQ(m.doc.tags[0].to, 4);
    m.setActiveCel(0, 0);
    m.deleteFrame();
    EXPECT_EQ(m.doc.tags[0].from, 0);
    EXPECT_EQ(m.doc.tags[0].to, 3);
}

TEST(Doc, WholeImageCommandsChangeEveryCel) {
    Model m = spriteModel();
    dot(m, 0, 0, kRed);
    m.addLayer();
    dot(m, 7, 7, kGreen);
    m.addFrame(true);
    ASSERT_TRUE(m.resizeCanvas(16, 8, 0, 0));
    m.transformImage(TF_FLIP_H);
    for (int l = 0; l < 2; ++l) {
        for (int f = 0; f < 2; ++f) {
            m.setActiveCel(l, f);
            EXPECT_EQ(m.tile.w, 16) << l << "," << f;
        }
    }
    m.setActiveCel(0, 0);
    EXPECT_EQ(m.tile.at(15, 0), kRed);
    m.setActiveCel(1, 1);
    EXPECT_EQ(m.tile.at(8, 7), kGreen);
}

TEST(Doc, OnionSkinShowsNeighbours) {
    Model m = spriteModel();
    dot(m, 2, 2, kRed);
    m.addFrame(false);
    EXPECT_EQ(m.viewColorAt(2, 2).a, 0);
    m.onionSkin = true;
    const Color c = m.viewColorAt(2, 2);
    EXPECT_GT(c.a, 0);
    EXPECT_LT(c.a, 255);
    EXPECT_GT(c.r, c.b);  // прошлый кадр — к красному
}

TEST(Doc, PaletteRemapReachesAllCels) {
    Model m = spriteModel();
    m.strictIndexed = true;
    m.setPalette({kRed, kGreen, kBlue});
    m.enableIndexed(false);
    dot(m, 0, 0, kBlue);
    m.addFrame(false);
    dot(m, 1, 0, kBlue);
    m.sortPalette(0, 0, 2);  // по тону: красный, зелёный, синий — порядок тот же
    m.movePaletteEntry(2, 0);  // синий — в начало
    m.setActiveCel(0, 0);
    EXPECT_EQ(m.tileIdx.at(0, 0), 0);
    EXPECT_EQ(m.tile.at(0, 0), kBlue);
    m.setActiveCel(0, 1);
    EXPECT_EQ(m.tileIdx.at(1, 0), 0);
}

// --- проект .tjt ----------------------------------------------------------------

Model richModel() {
    Model m = spriteModel();
    m.setPalette({kRed, kGreen, kBlue, argb(255, 10, 20, 30)});
    m.setTransparentIndex(3);
    dot(m, 1, 1, kRed);
    m.addLayer();
    m.renameLayer(1, "Контур");
    dot(m, 2, 2, kGreen);
    m.setLayerOpacity(1, 128);
    m.addFrame(true);
    dot(m, 3, 3, kBlue);
    m.setFrameDuration(1, 250);
    m.addTag("ходьба", 0, 1, TAG_PINGPONG);
    m.setLayerLocked(0, true);
    m.setSelectionMask(img::maskEllipse(8, 8, 1, 1, 6, 5));
    m.gridOn = true;
    m.symX = true;
    m.symAxisX2 = 5;
    m.ditherPattern = 7;
    m.onionSkin = true;
    m.pixelSize = 16;
    return m;
}

TEST(Project, RoundTrip) {
    Model a = richModel();
    const std::vector<std::uint8_t> file = projectEncode(a);
    EXPECT_TRUE(isProjectFile(file));
    Model b;
    std::string err;
    ASSERT_TRUE(projectDecode(file, b, err)) << err;
    EXPECT_EQ(b.palette, a.palette);
    EXPECT_EQ(b.transparentIdx, 3);
    ASSERT_EQ(b.doc.layerCount(), 2);
    ASSERT_EQ(b.doc.frameCount(), 2);
    EXPECT_EQ(b.doc.layers[1].name, "Контур");
    EXPECT_EQ(b.doc.layers[1].opacity, 128);
    EXPECT_TRUE(b.doc.layers[0].locked);
    EXPECT_EQ(b.doc.frames[1].durationMs, 250);
    ASSERT_EQ(b.doc.tags.size(), 1u);
    EXPECT_EQ(b.doc.tags[0].name, "ходьба");
    EXPECT_EQ(b.doc.tags[0].direction, TAG_PINGPONG);
    EXPECT_EQ(b.activeLayer, a.activeLayer);
    EXPECT_EQ(b.activeFrame, a.activeFrame);
    for (int l = 0; l < 2; ++l) {
        for (int f = 0; f < 2; ++f) {
            a.setActiveCel(l, f);
            b.setActiveCel(l, f);
            EXPECT_EQ(b.tile, a.tile) << l << "," << f;
            EXPECT_EQ(b.tileIdx, a.tileIdx) << l << "," << f;
        }
    }
    EXPECT_EQ(b.selectionMask(), a.selectionMask());
    EXPECT_TRUE(b.gridOn);
    EXPECT_TRUE(b.symX);
    EXPECT_EQ(b.symAxisX2, 5);
    EXPECT_EQ(b.ditherPattern, 7);
    EXPECT_TRUE(b.onionSkin);
    EXPECT_EQ(b.pixelSize, 16);
    EXPECT_FALSE(b.tileModified);
    EXPECT_TRUE(b.undoStack.empty());
}

TEST(Project, FileIsAPngOfTheFirstFrame) {
    Model a = richModel();
    const std::vector<std::uint8_t> file = projectEncode(a);
    Grid<Color> img;
    std::string err;
    ASSERT_TRUE(pngDecode(file, img, err)) << err;
    EXPECT_EQ(img, a.compositeFrame(0));
}

TEST(Project, RejectsPlainPngAndDamage) {
    Grid<Color> g(4, 4, kRed);
    const std::vector<std::uint8_t> plain = pngEncode(g);
    Model m;
    std::string err;
    EXPECT_FALSE(isProjectFile(plain));
    EXPECT_FALSE(projectDecode(plain, m, err));
    EXPECT_FALSE(err.empty());
    // Испорченный байт в куске проекта: контрольная сумма не сходится, и
    // проект не читается, а модель не трогается.
    Model a = richModel();
    std::vector<std::uint8_t> file = projectEncode(a);
    file[file.size() - 40] ^= 0x55;
    Model b = spriteModel();
    const Grid<Color> before = b.tile;
    EXPECT_FALSE(projectDecode(file, b, err));
    EXPECT_EQ(b.tile, before);
}

/// Пересобрать файл проекта с другим телом куска.
std::vector<std::uint8_t> withBody(const std::vector<std::uint8_t>& file,
                                   const std::function<void(std::vector<std::uint8_t>&)>& edit) {
    std::vector<std::uint8_t> packed;
    EXPECT_TRUE(pngFindChunk(file, kProjectChunk, packed));
    std::vector<std::uint8_t> body;
    std::string err;
    EXPECT_TRUE(zlibDecompress(packed.data(), packed.size(), body, err));
    edit(body);
    Grid<Color> img;
    EXPECT_TRUE(pngDecode(file, img, err));
    return pngEncodeWith(img, {PngChunk{kProjectChunk, zlibCompress(body)}});
}

TEST(Project, UnknownSectionsAreSkippedAndNewerVersionRefused) {
    Model a = richModel();
    const std::vector<std::uint8_t> file = projectEncode(a);
    const auto extended = withBody(file, [](std::vector<std::uint8_t>& b) {
        const std::uint8_t sec[] = {'Z', 'Z', 'Z', 'Z', 3, 0, 0, 0, 1, 2, 3};
        b.insert(b.begin() + 8, std::begin(sec), std::end(sec));
    });
    Model b;
    std::string err;
    ASSERT_TRUE(projectDecode(extended, b, err)) << err;
    EXPECT_EQ(b.doc.layerCount(), 2);
    const auto newer = withBody(file, [](std::vector<std::uint8_t>& body) { body[4] = 99; });
    EXPECT_FALSE(projectDecode(newer, b, err));
    EXPECT_NE(err.find("новой"), std::string::npos);
}

}  // namespace
}  // namespace tjtm
