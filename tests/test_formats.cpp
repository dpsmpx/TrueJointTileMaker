// Этап 5: BMP, TIFF и код конца потока LZW у GIF и TIFF.

#include <gtest/gtest.h>

#include <random>

#include "core/gif.h"
#include "core/rasterfmt.h"

namespace {

using namespace tjtm;

Grid<Color> sample(int w, int h, bool alpha) {
    Grid<Color> g(w, h);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            g.at(x, y) = argb(alpha ? static_cast<std::uint8_t>((x * 37 + y * 11) % 256) : 255,
                              static_cast<std::uint8_t>(x * 9), static_cast<std::uint8_t>(y * 13),
                              static_cast<std::uint8_t>((x ^ y) * 5));
        }
    }
    return g;
}

TEST(Lzw, StopsExactlyAtEndCodeForEveryLength) {
    // Код конца должен прийти той ширины, какой его ждёт читающий: иначе
    // строгий разборщик прочтёт мусор. Длины подобраны с переходами ширины.
    std::mt19937 rng(11);
    for (std::size_t len = 1; len < 9000; len += 1 + len / 7) {
        std::vector<std::uint8_t> data(len);
        for (auto& b : data)
            b = static_cast<std::uint8_t>(rng() % 256);
        std::vector<std::uint8_t> back;
        ASSERT_TRUE(
            tiffLzwDecode(tiffLzwEncode(data).data(), tiffLzwEncode(data).size(), back, len + 4096))
            << len;
        EXPECT_EQ(back, data) << "TIFF " << len;
        for (auto& b : data)
            b = static_cast<std::uint8_t>(b % 16);
        ASSERT_TRUE(gifLzwDecode(gifLzwEncode(data, 4), 4, len + 4096, back)) << len;
        EXPECT_EQ(back, data) << "GIF " << len;
    }
}

TEST(Bmp, OpaqueIs24BitAndAlphaIs32Bit) {
    const auto opaque = bmpEncode(sample(5, 3, false));
    // Строка 5 точек по 3 байта — 15, дополнена до 16.
    ASSERT_EQ(opaque.size(), 54U + 16U * 3U);
    EXPECT_EQ(opaque[0], 'B');
    EXPECT_EQ(opaque[28], 24);
    // Нижняя строка картинки идёт первой, синий — первым байтом точки.
    const Grid<Color> g = sample(5, 3, false);
    EXPECT_EQ(opaque[54], g.at(0, 2).b);
    EXPECT_EQ(opaque[56], g.at(0, 2).r);
    const auto alpha = bmpEncode(sample(5, 3, true));
    EXPECT_EQ(alpha[28], 32);
    EXPECT_EQ(alpha[14], 108);  // заголовок V4 с маской альфы
}

TEST(Tiff, RoundTripRgbRgbaAndPages) {
    const std::vector<Grid<Color>> pages = {sample(17, 9, false), sample(6, 30, true)};
    const auto bytes = tiffEncode(pages);
    ASSERT_TRUE(isTiff(bytes));
    std::vector<Grid<Color>> back;
    std::string err;
    ASSERT_TRUE(tiffDecode(bytes, back, err)) << err;
    ASSERT_EQ(back.size(), 2U);
    for (std::size_t p = 0; p < 2; ++p) {
        ASSERT_EQ(back[p].w, pages[p].w);
        ASSERT_EQ(back[p].h, pages[p].h);
        for (std::size_t k = 0; k < back[p].d.size(); ++k) {
            const Color a = pages[p].d[k];
            const Color b = back[p].d[k];
            if (a.a == 0) {
                EXPECT_EQ(b.a, 0);
            } else {
                EXPECT_EQ(a.argb(), b.argb()) << p << " " << k;
            }
        }
    }
}

TEST(Tiff, DamagedAndUnsupportedAreRefused) {
    std::vector<Grid<Color>> back;
    std::string err;
    EXPECT_FALSE(tiffDecode({'I', 'I', 42, 0}, back, err));
    auto bytes = tiffEncode({sample(8, 8, false)});
    bytes.resize(bytes.size() / 2);
    EXPECT_FALSE(tiffDecode(bytes, back, err));
}

}  // namespace
