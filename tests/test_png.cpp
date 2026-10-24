// PNG, APNG и deflate переноса.
//
// Чтение сверяется с libpng на PngSuite — наборе, где есть все глубины,
// типы цвета, прозрачность и чересстрочность, — и на картинках самого
// репозитория. Распаковка сверяется с потоками настоящего zlib, запись —
// обратным чтением, а если zlib есть в системе, то и им.

#include <gtest/gtest.h>

#include <random>
#include <sstream>

#include "core/fsutil.h"
#include "core/png.h"
#include "core/zlib.h"
#include "golden_util.h"

#ifdef TJTM_HAVE_ZLIB
#include <zlib.h>
#endif

using namespace tjtm;

namespace {

std::vector<std::uint8_t> fromHex(const std::string& s) {
    std::vector<std::uint8_t> out;
    if (s == "-") return out;
    for (std::size_t i = 0; i + 1 < s.size(); i += 2)
        out.push_back(static_cast<std::uint8_t>(std::stoi(s.substr(i, 2), nullptr, 16)));
    return out;
}

std::vector<std::uint8_t> bytesOf(const std::string& path) {
    const auto d = readFile(path);
    return d ? std::vector<std::uint8_t>(d->begin(), d->end()) : std::vector<std::uint8_t>{};
}

Grid<Color> randomImage(std::mt19937& rng, int w, int h, bool alpha) {
    Grid<Color> g(w, h);
    std::uniform_int_distribution<int> byte(0, 255);
    std::uniform_int_distribution<int> pal(0, 5);
    // Половина картинок — из немногих цветов, как пиксель-арт: так
    // проверяются и длинные совпадения, и литералы.
    const bool art = byte(rng) < 128;
    std::vector<Color> colors;
    for (int i = 0; i < 6; ++i)
        colors.push_back(argb(alpha ? byte(rng) : 255, byte(rng), byte(rng), byte(rng)));
    for (auto& c : g.d)
        c = art ? colors[static_cast<std::size_t>(pal(rng))]
                : argb(alpha ? byte(rng) : 255, byte(rng), byte(rng), byte(rng));
    return g;
}

}  // namespace

TEST(Png, DecodesLikeLibpng) {
    const auto lines = golden::readLines("png.txt");
    ASSERT_GE(lines.size(), 80u);
    int checked = 0;
    for (const auto& w : lines) {
        ASSERT_EQ(w.size(), 5u);
        std::string path;
        for (const char* dir :
             {"tests/data/pngsuite/", "Resources/", "Resources/Icons/", "Картинки/"}) {
            const std::string p = std::string(TJTM_SOURCE_DIR) + "/" + dir + w[1];
            if (fileExists(p)) {
                path = p;
                break;
            }
        }
        ASSERT_FALSE(path.empty()) << w[1];
        Grid<Color> img;
        std::string err;
        ASSERT_TRUE(pngDecode(bytesOf(path), img, err)) << w[1] << ": " << err;
        EXPECT_EQ(img.w, std::stoi(w[2])) << w[1];
        EXPECT_EQ(img.h, std::stoi(w[3])) << w[1];
        golden::Fnv f;
        for (const Color& c : img.d) {
            f.byte(c.r);
            f.byte(c.g);
            f.byte(c.b);
            f.byte(c.a);
        }
        EXPECT_EQ(f.hex(), w[4]) << w[1];
        ++checked;
    }
    EXPECT_GE(checked, 80);
}

TEST(Png, GameTilesDecode) {
    const char* dir = std::getenv("ANDORS_LOVE_DIR");
    if (dir == nullptr || !dirExists(std::string(dir) + "/data/tiles"))
        GTEST_SKIP() << "корпус игры не подключён";
    int n = 0;
    for (const auto& name : listFiles(std::string(dir) + "/data/tiles")) {
        if (pathExtLower(name) != ".png") continue;
        Grid<Color> img;
        std::string err;
        ASSERT_TRUE(pngLoad(std::string(dir) + "/data/tiles/" + name, img, err))
            << name << ": " << err;
        EXPECT_FALSE(img.empty()) << name;
        // Записанное заново читается в точности тем же.
        Grid<Color> again;
        ASSERT_TRUE(pngDecode(pngEncode(img), again, err)) << name;
        EXPECT_EQ(again, img) << name;
        ++n;
    }
    EXPECT_GE(n, 100);
}

TEST(Png, EncodeDecodeRoundTrip) {
    std::mt19937 rng(11);
    for (int k = 0; k < 200; ++k) {
        std::uniform_int_distribution<int> side(1, 70);
        const int w = side(rng);
        const int h = side(rng);
        const Grid<Color> img = randomImage(rng, w, h, k % 2 == 0);
        const auto bytes = pngEncode(img, k % 10);
        Grid<Color> back;
        std::string err;
        PngInfo info;
        ASSERT_TRUE(pngDecode(bytes, back, err, &info)) << k << ": " << err;
        EXPECT_EQ(info.bitDepth, 8);
        EXPECT_EQ(info.colorType, 6);
        ASSERT_EQ(back, img) << "картинка " << k;
    }
}

TEST(Png, RejectsBrokenFiles) {
    Grid<Color> img(4, 4, rgb(1, 2, 3));
    std::vector<std::uint8_t> good = pngEncode(img);
    Grid<Color> out;
    std::string err;

    EXPECT_FALSE(pngDecode({1, 2, 3}, out, err));
    std::vector<std::uint8_t> truncated(good.begin(),
                                        good.begin() + static_cast<long>(good.size() / 2));
    EXPECT_FALSE(pngDecode(truncated, out, err));
    std::vector<std::uint8_t> badCrc = good;
    badCrc[40] ^= 0xFF;  // внутри IDAT
    EXPECT_FALSE(pngDecode(badCrc, out, err));
    EXPECT_FALSE(err.empty());
}

TEST(Png, ApngRoundTrip) {
    std::mt19937 rng(5);
    std::vector<AnimFrame> frames;
    for (int i = 0; i < 5; ++i)
        frames.push_back(AnimFrame{randomImage(rng, 17, 9, true), 40 + i * 30});
    const auto bytes = apngEncode(frames, 3);
    std::vector<AnimFrame> back;
    std::string err;
    int loops = -1;
    ASSERT_TRUE(apngDecode(bytes, back, err, &loops)) << err;
    EXPECT_EQ(loops, 3);
    ASSERT_EQ(back.size(), frames.size());
    for (std::size_t i = 0; i < frames.size(); ++i) {
        EXPECT_EQ(back[i].image, frames[i].image) << i;
        EXPECT_EQ(back[i].durationMs, frames[i].durationMs) << i;
    }
    // Обычный PNG читается как анимация из одного кадра, а первый кадр APNG —
    // как обычная картинка.
    Grid<Color> still;
    ASSERT_TRUE(pngDecode(bytes, still, err));
    EXPECT_EQ(still, frames[0].image);
    std::vector<AnimFrame> one;
    ASSERT_TRUE(apngDecode(pngEncode(frames[1].image), one, err));
    ASSERT_EQ(one.size(), 1u);
    EXPECT_EQ(one[0].image, frames[1].image);
}

TEST(Zlib, InflatesRealZlibStreams) {
    const auto lines = golden::readLines("zlib.txt");
    ASSERT_GE(lines.size(), 80u);
    for (const auto& w : lines) {
        const auto want = fromHex(w[2]);
        const auto z = fromHex(w[3]);
        std::vector<std::uint8_t> out;
        std::string err;
        ASSERT_TRUE(zlibDecompress(z.data(), z.size(), out, err)) << w[1] << ": " << err;
        EXPECT_EQ(out, want) << w[1];
    }
}

TEST(Zlib, RoundTripAllLevels) {
    std::mt19937 rng(3);
    for (int k = 0; k < 60; ++k) {
        std::uniform_int_distribution<int> len(0, 200000);
        std::vector<std::uint8_t> data(static_cast<std::size_t>(len(rng) / (k % 3 == 0 ? 100 : 1)));
        std::uniform_int_distribution<int> alphabet(1, 256);
        const int a = alphabet(rng);
        std::uniform_int_distribution<int> sym(0, a - 1);
        for (auto& b : data)
            b = static_cast<std::uint8_t>(sym(rng));
        const int level = k % 10;
        const auto z = zlibCompress(data, level);
        std::vector<std::uint8_t> out;
        std::string err;
        ASSERT_TRUE(zlibDecompress(z.data(), z.size(), out, err)) << k << ": " << err;
        ASSERT_EQ(out, data) << k;
        if (a <= 8 && data.size() > 1000) {
            EXPECT_LT(z.size(), data.size() / 2) << "плохо сжато, k=" << k;
        }
#ifdef TJTM_HAVE_ZLIB
        // Поток переноса понимает настоящий zlib.
        std::vector<std::uint8_t> ref(data.size() + 1);
        uLongf refLen = static_cast<uLongf>(ref.size());
        ASSERT_EQ(uncompress(ref.data(), &refLen, z.data(), static_cast<uLong>(z.size())), Z_OK)
            << k;
        ref.resize(refLen);
        EXPECT_EQ(ref, data) << k;
#endif
    }
}
