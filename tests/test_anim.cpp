// Этап 4: GIF, лист спрайтов, кадры анимации в модели.

#include <gtest/gtest.h>

#include <random>

#include "core/anim.h"
#include "core/gif.h"
#include "core/model.h"

namespace {

using namespace tjtm;

AnimFrame frameOf(int w, int h, int ms, Color fill) {
    return AnimFrame{Grid<Color>(w, h, fill), ms};
}

TEST(Gif, LzwRoundTripRandomAndRuns) {
    std::mt19937 rng(7);
    for (const int minCode : {2, 3, 5, 8}) {
        const int top = 1 << minCode;
        // Случайные данные переполняют словарь и проверяют сброс; длинные
        // повторы — случай KwKwK, когда код приходит раньше своей записи.
        std::vector<std::uint8_t> data(70000);
        for (std::size_t i = 0; i < data.size(); ++i)
            data[i] = static_cast<std::uint8_t>(i < 30000 ? rng() % static_cast<unsigned>(top)
                                                          : (i / 997) % static_cast<unsigned>(top));
        const auto z = gifLzwEncode(data, minCode);
        std::vector<std::uint8_t> back;
        ASSERT_TRUE(gifLzwDecode(z, minCode, data.size(), back)) << minCode;
        EXPECT_EQ(back, data) << minCode;
    }
    std::vector<std::uint8_t> one = {1};
    std::vector<std::uint8_t> back;
    ASSERT_TRUE(gifLzwDecode(gifLzwEncode(one, 2), 2, 1, back));
    EXPECT_EQ(back, one);
}

TEST(Gif, AnimationRoundTripWithTransparency) {
    std::vector<AnimFrame> frames;
    for (int k = 0; k < 3; ++k) {
        // GIF хранит сотые доли секунды: длительности кратны 10 мс.
        AnimFrame f = frameOf(12, 7, 40 + k * 110, argb(0, 0, 0, 0));
        for (int x = 0; x < 12; ++x)
            f.image.at(x, k) =
                rgb(static_cast<std::uint8_t>(20 * x), 200, static_cast<std::uint8_t>(80 * k));
        f.image.at(5, 5) = argb(100, 255, 0, 0);  // меньше половины — прозрачная
        f.image.at(6, 5) = argb(200, 0, 0, 255);  // больше — видна целиком
        frames.push_back(f);
    }
    const auto bytes = gifEncode(frames, 3);
    ASSERT_TRUE(isGif(bytes));
    std::vector<AnimFrame> back;
    std::string err;
    int plays = -1;
    ASSERT_TRUE(gifDecode(bytes, back, err, &plays)) << err;
    EXPECT_EQ(plays, 3);
    ASSERT_TRUE(gifDecode(gifEncode(frames, 0), back, err, &plays));
    EXPECT_EQ(plays, 0);
    ASSERT_TRUE(gifDecode(gifEncode(frames, 1), back, err, &plays));
    EXPECT_EQ(plays, 1);
    ASSERT_EQ(back.size(), frames.size());
    for (std::size_t k = 0; k < frames.size(); ++k) {
        EXPECT_EQ(back[k].durationMs, frames[k].durationMs);
        ASSERT_EQ(back[k].image.w, 12);
        ASSERT_EQ(back[k].image.h, 7);
        for (int y = 0; y < 7; ++y) {
            for (int x = 0; x < 12; ++x) {
                const Color a = frames[k].image.at(x, y);
                const Color b = back[k].image.at(x, y);
                if (a.a < 128) {
                    EXPECT_EQ(b.a, 0) << k << " " << x << " " << y;
                } else {
                    EXPECT_EQ(b.argb(), rgb(a.r, a.g, a.b).argb()) << k << " " << x << " " << y;
                }
            }
        }
    }
}

TEST(Gif, PaletteOrderIsKeptAndManyColorsAreReduced) {
    const std::vector<Color> pal = {rgb(9, 9, 9), rgb(200, 0, 0), rgb(0, 200, 0)};
    AnimFrame f = frameOf(4, 1, 100, rgb(0, 200, 0));
    f.image.at(1, 0) = rgb(9, 9, 9);
    f.image.at(2, 0) = rgb(1, 2, 3);  // не из палитры — встаёт после неё
    const auto bytes = gifEncode({f}, 0, pal);
    // Глобальная таблица идёт сразу за 13 байтами заголовка: цвета палитры,
    // которые есть в картинке, — первыми и в её порядке.
    ASSERT_GT(bytes.size(), 13U + 9U);
    EXPECT_EQ(rgb(bytes[13], bytes[14], bytes[15]).argb(), rgb(9, 9, 9).argb());
    EXPECT_EQ(rgb(bytes[16], bytes[17], bytes[18]).argb(), rgb(0, 200, 0).argb());
    EXPECT_EQ(rgb(bytes[19], bytes[20], bytes[21]).argb(), rgb(1, 2, 3).argb());

    AnimFrame many = frameOf(64, 64, 100, rgb(0, 0, 0));
    for (int y = 0; y < 64; ++y) {
        for (int x = 0; x < 64; ++x)
            many.image.at(x, y) =
                rgb(static_cast<std::uint8_t>(x * 4), static_cast<std::uint8_t>(y * 4), 128);
    }
    std::vector<AnimFrame> back;
    std::string err;
    ASSERT_TRUE(gifDecode(gifEncode({many}), back, err)) << err;
    ASSERT_EQ(back.size(), 1U);
    int far = 0;
    for (std::size_t k = 0; k < many.image.d.size(); ++k) {
        const Color a = many.image.d[k];
        const Color b = back[0].image.d[k];
        if (std::abs(a.r - b.r) > 32 || std::abs(a.g - b.g) > 32 || b.a != 255) ++far;
    }
    EXPECT_EQ(far, 0);
}

TEST(Gif, DamagedFilesAreRefused) {
    std::vector<AnimFrame> back;
    std::string err;
    EXPECT_FALSE(gifDecode({}, back, err));
    EXPECT_FALSE(gifDecode({'G', 'I', 'F', '8', '9', 'a', 1, 0}, back, err));
    auto bytes = gifEncode({frameOf(5, 5, 100, rgb(1, 2, 3))});
    bytes.resize(bytes.size() - 8);
    // Обрезанный поток — ошибка или кадр с тем, что успело прочитаться,
    // но не падение.
    gifDecode(bytes, back, err);
}

TEST(Sheet, LayoutsMarginSpacingScale) {
    std::vector<AnimFrame> frames;
    for (int k = 0; k < 5; ++k) {
        AnimFrame f = frameOf(3, 2, 50 * (k + 1), rgb(static_cast<std::uint8_t>(k * 40), 0, 0));
        f.image.at(0, 0) = rgb(255, 255, 255);
        frames.push_back(f);
    }
    SheetOptions row;
    SpriteSheet s = buildSheet(frames, row);
    EXPECT_EQ(s.image.w, 15);
    EXPECT_EQ(s.image.h, 2);
    EXPECT_EQ(s.cells[4].x, 12);
    EXPECT_EQ(s.cells[4].durationMs, 250);

    SheetOptions col;
    col.layout = SHEET_COLUMN;
    s = buildSheet(frames, col);
    EXPECT_EQ(s.image.w, 3);
    EXPECT_EQ(s.image.h, 10);

    SheetOptions grid;
    grid.layout = SHEET_GRID;  // 5 кадров — сетка 3 на 2
    grid.margin = 2;
    grid.spacing = 1;
    grid.scale = 2;
    s = buildSheet(frames, grid);
    EXPECT_EQ(s.image.w, 2 * 2 + 3 * 6 + 2);
    EXPECT_EQ(s.image.h, 2 * 2 + 2 * 4 + 1);
    EXPECT_EQ(s.cells[3].x, 2);
    EXPECT_EQ(s.cells[3].y, 2 + 4 + 1);
    EXPECT_EQ(s.cells[3].w, 6);
    // Увеличение ближайшим соседом: белая точка стала квадратом 2x2.
    EXPECT_EQ(s.image.at(2, 2).argb(), rgb(255, 255, 255).argb());
    EXPECT_EQ(s.image.at(3, 3).argb(), rgb(255, 255, 255).argb());
    EXPECT_EQ(s.image.at(4, 2).argb(), rgb(0, 0, 0).argb());
    EXPECT_EQ(s.image.at(0, 0).a, 0);  // поле прозрачно
}

TEST(Sheet, JsonDescribesFramesAndTags) {
    std::vector<AnimFrame> frames = {frameOf(4, 4, 100, rgb(1, 1, 1)),
                                     frameOf(4, 4, 150, rgb(2, 2, 2))};
    const SpriteSheet s = buildSheet(frames, SheetOptions{});
    const std::string j =
        sheetJson(s, "hero.png", "hero", {AnimTag{"бег \"быстрый\"", 0, 1, TAG_PINGPONG}});
    EXPECT_NE(j.find("\"filename\": \"hero 1\""), std::string::npos);
    EXPECT_NE(j.find("\"frame\": { \"x\": 4, \"y\": 0, \"w\": 4, \"h\": 4 }"), std::string::npos);
    EXPECT_NE(j.find("\"duration\": 150"), std::string::npos);
    EXPECT_NE(j.find("\"image\": \"hero.png\""), std::string::npos);
    EXPECT_NE(j.find("\"name\": \"бег \\\"быстрый\\\"\""), std::string::npos);
    EXPECT_NE(j.find("\"direction\": \"pingpong\""), std::string::npos);
    EXPECT_EQ(jsonString("a\nb\x01"), "\"a\\nb\\u0001\"");
}

TEST(AnimModel, FramesOutAndIn) {
    Model m;
    m.newTileOfSize(8, 8);
    m.tile.at(1, 1) = rgb(255, 0, 0);
    m.addFrame(false);
    m.tile.at(2, 2) = rgb(0, 255, 0);
    m.setFrameDuration(1, 250);
    m.addLayer();
    m.tile.at(3, 3) = rgb(0, 0, 255);
    const auto frames = m.animationFrames(0, 5);
    ASSERT_EQ(frames.size(), 2U);
    EXPECT_EQ(frames[0].image.at(1, 1).argb(), rgb(255, 0, 0).argb());
    EXPECT_EQ(frames[1].image.at(2, 2).argb(), rgb(0, 255, 0).argb());
    EXPECT_EQ(frames[1].image.at(3, 3).argb(), rgb(0, 0, 255).argb());
    EXPECT_EQ(frames[1].durationMs, 250);

    Model n;
    n.setAnimation(frames, "x.gif");
    EXPECT_EQ(n.doc.frameCount(), 2);
    EXPECT_EQ(n.doc.layerCount(), 1);
    EXPECT_EQ(n.doc.frames[1].durationMs, 250);
    EXPECT_EQ(n.tile.at(1, 1).argb(), rgb(255, 0, 0).argb());
    n.setActiveCel(0, 1);
    EXPECT_EQ(n.tile.at(3, 3).argb(), rgb(0, 0, 255).argb());
    EXPECT_FALSE(n.isModified());
}

TEST(AnimModel, TagsEditAndDurationsUndo) {
    Model m;
    m.newTileOfSize(8, 8);
    for (int k = 0; k < 4; ++k)
        m.addFrame(false);
    const int t = m.addTag("бег", 1, 2, TAG_FORWARD);
    EXPECT_EQ(m.tagOfFrame(0), -1);
    EXPECT_EQ(m.tagOfFrame(2), t);
    ASSERT_TRUE(m.setTag(t, AnimTag{"прыжок", 4, 3, TAG_REVERSE}));
    EXPECT_EQ(m.doc.tags[0].name, "прыжок");
    EXPECT_EQ(m.doc.tags[0].from, 3);
    EXPECT_EQ(m.doc.tags[0].to, 4);
    m.setFramesDuration(0, 4, 80);
    for (const Frame& f : m.doc.frames)
        EXPECT_EQ(f.durationMs, 80);
    m.doUndo();
    EXPECT_EQ(m.doc.frames[2].durationMs, 100);
}

}  // namespace
