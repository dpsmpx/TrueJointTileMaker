// Цвет переноса против эталона TJTMColor: полные переборы сверяются по
// хэшу, выборка — построчно, чтобы при расхождении было видно, где.

#include <gtest/gtest.h>

#include "core/color.h"
#include "golden_util.h"

using namespace tjtm;

namespace {

const auto& goldenLines() {
    static const auto lines = golden::readLines("color.txt");
    return lines;
}

std::string hashOf(const std::string& name) {
    const auto m = golden::hashes(goldenLines());
    const auto it = m.find(name);
    return it == m.end() ? std::string("нет в эталоне") : it->second;
}

}  // namespace

TEST(Color, GoldenFilePresent) {
    ASSERT_GT(goldenLines().size(), 1000u) << "нет tests/golden/color.txt";
}

TEST(Color, HsvToRgbSamples) {
    int n = 0;
    for (const auto& w : goldenLines()) {
        if (w[0] != "hsv") continue;
        const Color c = hsvToRgb(std::stoi(w[1]), std::stoi(w[2]), std::stoi(w[3]));
        ASSERT_EQ(c.r, std::stoi(w[4])) << w[1] << " " << w[2] << " " << w[3];
        ASSERT_EQ(c.g, std::stoi(w[5])) << w[1] << " " << w[2] << " " << w[3];
        ASSERT_EQ(c.b, std::stoi(w[6])) << w[1] << " " << w[2] << " " << w[3];
        ++n;
    }
    EXPECT_GT(n, 100);
}

TEST(Color, HsvToRgbFullSweep) {
    golden::Fnv f;
    for (int h = -360; h <= 719; ++h) {
        for (int s = -5; s <= 105; ++s) {
            for (int v = -5; v <= 105; ++v) {
                const Color c = hsvToRgb(h, s, v);
                f.byte(c.r);
                f.byte(c.g);
                f.byte(c.b);
                f.byte(c.a);
            }
        }
    }
    EXPECT_EQ(f.hex(), hashOf("hsvToRgb"));
}

TEST(Color, RgbToHsvSamplesAndSweep) {
    for (const auto& w : goldenLines()) {
        if (w[0] != "rgb") continue;
        const HSVColor x = rgbToHsv(std::stoi(w[1]), std::stoi(w[2]), std::stoi(w[3]));
        ASSERT_EQ(x.h, std::stoi(w[4])) << w[1] << " " << w[2] << " " << w[3];
        ASSERT_EQ(x.s, std::stoi(w[5])) << w[1] << " " << w[2] << " " << w[3];
        ASSERT_EQ(x.v, std::stoi(w[6])) << w[1] << " " << w[2] << " " << w[3];
    }
    golden::Fnv f;
    for (int r = 0; r <= 255; ++r) {
        for (int g = 0; g <= 255; ++g) {
            for (int b = 0; b <= 255; ++b) {
                const HSVColor x = rgbToHsv(r, g, b);
                f.i32(x.h);
                f.byte(x.s);
                f.byte(x.v);
            }
        }
    }
    EXPECT_EQ(f.hex(), hashOf("rgbToHsv"));
}

TEST(Color, BlendOverSamplesAndSweep) {
    for (const auto& w : goldenLines()) {
        if (w[0] != "blend") continue;
        const auto src = Color::fromArgb(static_cast<std::uint32_t>(std::stoll(w[1])));
        const auto dst = Color::fromArgb(static_cast<std::uint32_t>(std::stoll(w[2])));
        const auto want = static_cast<std::uint32_t>(std::stoll(w[3]));
        ASSERT_EQ(blendOver(src, dst).argb(), want) << w[1] << " over " << w[2];
    }
    golden::Fnv f;
    long long seed = 12345;
    for (int sa = 0; sa <= 255; ++sa) {
        for (int da = 0; da <= 255; ++da) {
            seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
            const Color src =
                argb(sa, static_cast<int>(seed & 255), static_cast<int>((seed >> 8) & 255),
                     static_cast<int>((seed >> 16) & 255));
            seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
            const Color dst =
                argb(da, static_cast<int>(seed & 255), static_cast<int>((seed >> 8) & 255),
                     static_cast<int>((seed >> 16) & 255));
            const Color c = blendOver(src, dst);
            f.byte(c.a);
            f.byte(c.r);
            f.byte(c.g);
            f.byte(c.b);
        }
    }
    EXPECT_EQ(f.hex(), hashOf("blendOver"));
}

TEST(Color, AxisFunctions) {
    golden::Fnv f;
    for (int size = -1; size <= 300; ++size) {
        for (int pos = -10; pos <= 310; ++pos) {
            f.i32(axisToPercent(pos, size));
            f.i32(percentToAxis(pos, size));
            f.i32(axisToValue(pos, size));
            f.i32(valueToAxis(pos, size));
            f.i32(axisToHue(pos, size));
            f.i32(hueToAxis(pos, size));
            f.i32(axisToByte(pos, size));
            f.i32(byteToAxis(pos, size));
        }
    }
    EXPECT_EQ(f.hex(), hashOf("axis"));
}

TEST(Color, Planes) {
    golden::Fnv f;
    std::vector<std::uint8_t> raw(100 * 100 * 4);
    for (int tone = 0; tone <= 359; ++tone) {
        renderHSVPlane(raw, 100, tone);
        for (std::uint8_t b : raw)
            f.byte(b);
    }
    raw.assign(100 * 12 * 4, 0);
    renderHuePlane(raw, 100);
    for (std::uint8_t b : raw)
        f.byte(b);
    renderAlphaPlane(raw, 100);
    for (std::uint8_t b : raw)
        f.byte(b);
    EXPECT_EQ(f.hex(), hashOf("planes"));
}
