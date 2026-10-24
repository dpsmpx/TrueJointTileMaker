// Этап 7: лист 8x8 для игры, атлас набора, стыки для автотайлинга.

#include <gtest/gtest.h>

#include <random>

#include "core/gameset.h"

namespace {

using namespace tjtm;

Grid<Color> noise(int w, int h, unsigned seed) {
    std::mt19937 rng(seed);
    Grid<Color> g(w, h);
    for (Color& c : g.d)
        c = argb(static_cast<int>(rng() % 256), static_cast<int>(rng() % 256),
                 static_cast<int>(rng() % 256), static_cast<int>(rng() % 256));
    return g;
}

TEST(GameSheet, RoundTripKeepsSizesAndEmptySlots) {
    // Тайлы разного размера и пропуски: игра режет по маркеру, поэтому
    // каждый должен вернуться своим размером, а пустой слот — пустым.
    std::vector<Grid<Color>> slots(20);
    slots[0] = noise(32, 32, 1);
    slots[1] = noise(16, 24, 2);
    slots[7] = noise(1, 1, 3);
    slots[8] = noise(63, 63, 4);
    slots[19] = noise(40, 5, 5);
    Grid<Color> sheet;
    std::string err;
    ASSERT_TRUE(game::composeSheet(slots, sheet, err)) << err;
    EXPECT_EQ(sheet.w, 8 * 64);
    EXPECT_EQ(sheet.h, 8 * 64);
    std::vector<Grid<Color>> back;
    ASSERT_TRUE(game::sliceSheet(sheet, back, err)) << err;
    ASSERT_EQ(back.size(), 64U);
    for (std::size_t i = 0; i < back.size(); ++i) {
        const Grid<Color> want = i < slots.size() ? slots[i] : Grid<Color>{};
        EXPECT_EQ(back[i].w, want.w) << i;
        EXPECT_EQ(back[i].h, want.h) << i;
        EXPECT_EQ(back[i].d, want.d) << i;
    }
}

TEST(GameSheet, TransparentPixelsAreNotMarker) {
    // Маркер сравнивается по всем каналам: прозрачная точка того же цвета,
    // что маркер, и прозрачный чёрный не обрезают тайл.
    Grid<Color> t(4, 4, argb(0, 0, 0, 0));
    t.at(1, 0) = argb(0, 1, 2, 3);
    t.at(0, 2) = argb(254, 1, 2, 3);
    Grid<Color> sheet;
    std::string err;
    ASSERT_TRUE(game::composeSheet({t}, sheet, err)) << err;
    EXPECT_EQ(sheet.w, 8 * 5);
    std::vector<Grid<Color>> back;
    ASSERT_TRUE(game::sliceSheet(sheet, back, err));
    EXPECT_EQ(back[0].w, 4);
    EXPECT_EQ(back[0].h, 4);
    EXPECT_EQ(back[0].d, t.d);
    EXPECT_TRUE(back[1].empty());
}

TEST(GameSheet, RejectsTooBigOrTooMany) {
    Grid<Color> sheet;
    std::string err;
    EXPECT_FALSE(game::composeSheet({noise(64, 8, 1)}, sheet, err));
    EXPECT_FALSE(err.empty());
    err.clear();
    EXPECT_FALSE(game::composeSheet(std::vector<Grid<Color>>(65, noise(2, 2, 1)), sheet, err));
    EXPECT_FALSE(err.empty());
    std::vector<Grid<Color>> back;
    EXPECT_FALSE(game::sliceSheet(Grid<Color>(7, 7), back, err));
}

TEST(GameAtlas, PlacesDoNotOverlapAndKeepPixels) {
    std::vector<game::AtlasEntry> entries;
    for (int i = 0; i < 30; ++i)
        entries.push_back({"t" + std::to_string(i),
                           noise(8 + (i * 7) % 25, 5 + (i * 11) % 30, static_cast<unsigned>(i))});
    entries.push_back({"empty", {}});
    std::vector<game::AtlasPlace> places;
    const Grid<Color> atlas = game::packAtlas(entries, 128, 1, places);
    ASSERT_EQ(places.size(), 30U);
    EXPECT_LE(atlas.w, 128);
    for (std::size_t a = 0; a < places.size(); ++a) {
        const game::AtlasPlace& p = places[a];
        ASSERT_GE(p.x, 1);
        ASSERT_GE(p.y, 1);
        ASSERT_LE(p.x + p.w + 1, atlas.w);
        ASSERT_LE(p.y + p.h + 1, atlas.h);
        for (std::size_t b = a + 1; b < places.size(); ++b) {
            const game::AtlasPlace& q = places[b];
            // С отступом: соседи не касаются, фильтрация в игре не смешает.
            const bool apart = p.x + p.w + 1 <= q.x || q.x + q.w + 1 <= p.x ||
                               p.y + p.h + 1 <= q.y || q.y + q.h + 1 <= p.y;
            EXPECT_TRUE(apart) << p.name << " " << q.name;
        }
        const int idx = std::stoi(p.name.substr(1));
        const Grid<Color>& src = entries[static_cast<std::size_t>(idx)].image;
        ASSERT_EQ(src.w, p.w);
        ASSERT_EQ(src.h, p.h);
        for (int y = 0; y < p.h; ++y) {
            for (int x = 0; x < p.w; ++x)
                ASSERT_EQ(atlas.at(p.x + x, p.y + y).argb(), src.at(x, y).argb()) << p.name;
        }
    }
    // Порядок файлов в папке не меняет раскладку.
    std::vector<game::AtlasEntry> reversed(entries.rbegin(), entries.rend());
    std::vector<game::AtlasPlace> places2;
    const Grid<Color> atlas2 = game::packAtlas(reversed, 128, 1, places2);
    EXPECT_EQ(atlas2.d, atlas.d);
}

TEST(GameAtlas, JsonDescribesEveryFrame) {
    const std::vector<game::AtlasPlace> places = {{"wall", 1, 1, 32, 32}, {"floor", 34, 1, 16, 8}};
    const std::string j = game::atlasJson(places, "tiles.png", 64, 40);
    EXPECT_NE(j.find("\"wall.png\": {"), std::string::npos);
    EXPECT_NE(j.find("\"frame\": { \"x\": 34, \"y\": 1, \"w\": 16, \"h\": 8 }"), std::string::npos);
    EXPECT_NE(j.find("\"image\": \"tiles.png\""), std::string::npos);
    EXPECT_NE(j.find("\"size\": { \"w\": 64, \"h\": 40 }"), std::string::npos);
    EXPECT_EQ(j.find("},\n },"), std::string::npos);
}

TEST(Autotile, MaskCountsSameTerrainAndEdges) {
    MapDoc m;
    m.reset(5, 4);
    // Стена по краю, пол внутри; одна стена внутри — отдельно стоящая.
    const int wall = m.at(0, 0);
    const int floor = m.at(1, 1);
    ASSERT_NE(wall, floor);
    m.at(2, 2) = static_cast<std::uint8_t>(wall);
    // Угол: за краем — та же местность, внутри — пол.
    EXPECT_EQ(game::autotileMask(m, 0, 0), 1 | 2 | 4 | 8);
    // Верхняя стена посреди края: юг — пол.
    EXPECT_EQ(game::autotileMask(m, 2, 0), 1 | 2 | 8);
    // Внутренняя стена: снизу стена края (y=3), остальное — пол.
    EXPECT_EQ(game::autotileMask(m, 2, 2), 4);
    // Пол в углу комнаты: пол справа и снизу, сверху и слева — стены края.
    EXPECT_EQ(game::autotileMask(m, 1, 1), 2 | 4);
    EXPECT_EQ(game::autotileMask(m, -1, 0), 0);
    EXPECT_EQ(game::autotileName("wall", 5), "wall_5");
}

TEST(Autotile, SheetRoundTrip) {
    std::vector<Grid<Color>> variants;
    for (int k = 0; k < 16; ++k)
        variants.push_back(noise(12, 10, static_cast<unsigned>(100 + k)));
    const Grid<Color> sheet = game::autotileSheet(variants, 12, 10);
    EXPECT_EQ(sheet.w, 48);
    EXPECT_EQ(sheet.h, 40);
    const auto back = game::sliceAutotileSheet(sheet);
    ASSERT_EQ(back.size(), 16U);
    for (int k = 0; k < 16; ++k)
        EXPECT_EQ(back[static_cast<std::size_t>(k)].d, variants[static_cast<std::size_t>(k)].d)
            << k;
    EXPECT_TRUE(game::sliceAutotileSheet(Grid<Color>(3, 3)).empty());
}

}  // namespace
