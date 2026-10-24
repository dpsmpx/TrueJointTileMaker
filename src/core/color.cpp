#include "core/color.h"

#include "core/pmath.h"

namespace tjtm {

bool sameColor(Color a, Color b) {
    return a.argb() == b.argb();
}

bool sameRGB(Color a, Color b) {
    return a.r == b.r && a.g == b.g && a.b == b.b;
}

// Выражения повторяют оригинал до порядка операций: целое умножение, потом
// вещественное деление. Иначе на границах каналов результат разошёлся бы
// в младшем бите double и, после банковского округления, в целую единицу.
Color hsvToRgb(int h, int s, int v) {
    h = h % 360;
    if (h < 0) h += 360;
    s = clampI(s, 0, 100);
    v = clampI(v, 0, 100);

    const double vm = static_cast<double>((100 - s) * v) / 100;
    const double a = (v - vm) * (static_cast<double>(h % 60) / 60);
    const double vi = vm + a;
    const double vd = v - a;

    double r = 0;
    double g = 0;
    double b = 0;
    switch (h / 60) {
        case 0:
            r = v;
            g = vi;
            b = vm;
            break;
        case 1:
            r = vd;
            g = v;
            b = vm;
            break;
        case 2:
            r = vm;
            g = v;
            b = vi;
            break;
        case 3:
            r = vm;
            g = vd;
            b = v;
            break;
        case 4:
            r = vi;
            g = vm;
            b = v;
            break;
        default:
            r = v;
            g = vm;
            b = vd;
            break;
    }
    return rgb(clampI(pround(r * 2.55), 0, 255), clampI(pround(g * 2.55), 0, 255),
               clampI(pround(b * 2.55), 0, 255));
}

HSVColor rgbToHsv(int r, int g, int b) {
    r = clampI(r, 0, 255);
    g = clampI(g, 0, 255);
    b = clampI(b, 0, 255);

    int mn = r;
    if (g < mn) mn = g;
    if (b < mn) mn = b;
    int mx = r;
    if (g > mx) mx = g;
    if (b > mx) mx = b;
    const int delta = mx - mn;

    int h = 0;
    if (delta == 0) {
        h = 0;
    } else if (mx == r) {
        h = pround(static_cast<double>(60 * (g - b)) / delta);
    } else if (mx == g) {
        h = pround(static_cast<double>(60 * (b - r)) / delta) + 120;
    } else {
        h = pround(static_cast<double>(60 * (r - g)) / delta) + 240;
    }
    h = h % 360;
    if (h < 0) h += 360;

    HSVColor res;
    res.h = h;
    res.s = (mx == 0) ? 0 : clampI(pround(static_cast<double>(delta * 100) / mx), 0, 100);
    res.v = clampI(pround(static_cast<double>(mx * 100) / 255), 0, 100);
    return res;
}

HSVColor colorToHsv(Color c) {
    return rgbToHsv(c.r, c.g, c.b);
}

Color blendOver(Color src, Color dst) {
    if (src.a >= 255) return src;
    if (src.a == 0) return dst;

    const double sa = src.a / 255.0;
    const double da = dst.a / 255.0;
    const double oa = sa + da * (1 - sa);
    if (oa <= 0) return argb(0, 0, 0, 0);
    return argb(clampI(pround(oa * 255), 0, 255),
                clampI(pround((src.r * sa + dst.r * da * (1 - sa)) / oa), 0, 255),
                clampI(pround((src.g * sa + dst.g * da * (1 - sa)) / oa), 0, 255),
                clampI(pround((src.b * sa + dst.b * da * (1 - sa)) / oa), 0, 255));
}

int axisToPercent(int pos, int size) {
    if (size <= 1) return 0;
    return clampI(pround(static_cast<double>(pos * 100) / (size - 1)), 0, 100);
}

int percentToAxis(int percent, int size) {
    if (size <= 1) return 0;
    return clampI(pround(static_cast<double>(clampI(percent, 0, 100) * (size - 1)) / 100), 0,
                  size - 1);
}

int axisToValue(int pos, int size) {
    return 100 - axisToPercent(pos, size);
}

int valueToAxis(int v, int size) {
    return percentToAxis(100 - clampI(v, 0, 100), size);
}

int axisToHue(int pos, int size) {
    if (size <= 1) return 0;
    return clampI(pround(static_cast<double>(pos * 359) / (size - 1)), 0, 359);
}

int hueToAxis(int h, int size) {
    if (size <= 1) return 0;
    return clampI(pround(static_cast<double>(clampI(h, 0, 359) * (size - 1)) / 359), 0, size - 1);
}

int axisToByte(int pos, int size) {
    if (size <= 1) return 0;
    return clampI(pround(static_cast<double>(pos * 255) / (size - 1)), 0, 255);
}

int byteToAxis(int v, int size) {
    if (size <= 1) return 0;
    return clampI(pround(static_cast<double>(clampI(v, 0, 255) * (size - 1)) / 255), 0, size - 1);
}

namespace {

// Общий каркас трёх полос: перебор пикселей буфера с цветом от kind.
void fillPlane(std::vector<std::uint8_t>& bytes, int width, int kind, int tone) {
    if (bytes.empty() || width <= 0) return;
    const int height = static_cast<int>(bytes.size() / 4) / width;
    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            Color c;
            switch (kind) {
                case 0: c = hsvToRgb(tone, axisToPercent(x, width), axisToValue(y, height)); break;
                case 1: c = hsvToRgb(axisToHue(x, width), 100, 100); break;
                default: {
                    const int g = 255 - axisToByte(x, width);
                    c = rgb(g, g, g);
                    break;
                }
            }
            const std::size_t i = (static_cast<std::size_t>(y) * static_cast<std::size_t>(width) +
                                   static_cast<std::size_t>(x)) *
                                  4;
            bytes[i + 0] = c.b;
            bytes[i + 1] = c.g;
            bytes[i + 2] = c.r;
            bytes[i + 3] = 255;
        }
    }
}

}  // namespace

void renderHSVPlane(std::vector<std::uint8_t>& bytes, int width, int tone) {
    fillPlane(bytes, width, 0, tone);
}

void renderHuePlane(std::vector<std::uint8_t>& bytes, int width) {
    fillPlane(bytes, width, 1, 0);
}

void renderAlphaPlane(std::vector<std::uint8_t>& bytes, int width) {
    fillPlane(bytes, width, 2, 0);
}

}  // namespace tjtm
