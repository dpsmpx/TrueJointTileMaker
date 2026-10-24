#include "core/png.h"

#include <algorithm>
#include <array>
#include <cstdlib>
#include <cstring>

#include "core/fsutil.h"
#include "core/pmath.h"
#include "core/zlib.h"

namespace tjtm {

namespace {

constexpr std::array<std::uint8_t, 8> kSig = {137, 80, 78, 71, 13, 10, 26, 10};
/// Верхний предел стороны: защита от файла, обещающего гигантскую картинку.
constexpr std::uint32_t kMaxSide = 16384;

std::uint32_t be32(const std::uint8_t* p) {
    return (std::uint32_t{p[0]} << 24) | (std::uint32_t{p[1]} << 16) | (std::uint32_t{p[2]} << 8) |
           p[3];
}

std::uint16_t be16(const std::uint8_t* p) {
    return static_cast<std::uint16_t>((p[0] << 8) | p[1]);
}

struct Chunk {
    std::string type;
    const std::uint8_t* data = nullptr;
    std::uint32_t len = 0;
};

/// Параметры кодирования, общие для всех кадров файла.
struct Format {
    std::uint32_t w = 0, h = 0;
    int depth = 0;
    int ctype = 0;
    bool interlaced = false;
    std::vector<Color> palette;
    bool hasTrns = false;
    std::array<std::uint16_t, 3> trnsKey{};  // серый или RGB для типов 0 и 2
};

int channels(int ctype) {
    switch (ctype) {
        case 0: return 1;
        case 2: return 3;
        case 3: return 1;
        case 4: return 2;
        case 6: return 4;
        default: return 0;
    }
}

bool validDepth(int ctype, int depth) {
    switch (ctype) {
        case 0: return depth == 1 || depth == 2 || depth == 4 || depth == 8 || depth == 16;
        case 3: return depth == 1 || depth == 2 || depth == 4 || depth == 8;
        case 2:
        case 4:
        case 6: return depth == 8 || depth == 16;
        default: return false;
    }
}

int paeth(int a, int b, int c) {
    const int p = a + b - c;
    const int pa = std::abs(p - a);
    const int pb = std::abs(p - b);
    const int pc = std::abs(p - c);
    if (pa <= pb && pa <= pc) return a;
    if (pb <= pc) return b;
    return c;
}

/// Снимает фильтры строк одного прохода. raw — данные прохода вместе с
/// байтами типа фильтра; на выходе — только пиксели.
bool unfilter(const std::uint8_t* raw, std::size_t rawLen, std::uint32_t w, std::uint32_t h,
              int bpp, std::size_t stride, std::vector<std::uint8_t>& out, std::string& error) {
    out.assign(stride * h, 0);
    if ((stride + 1) * h > rawLen) {
        error = "данных изображения меньше, чем обещает заголовок";
        return false;
    }
    (void)w;
    for (std::uint32_t y = 0; y < h; ++y) {
        const std::uint8_t ft = raw[y * (stride + 1)];
        const std::uint8_t* src = raw + y * (stride + 1) + 1;
        std::uint8_t* dst = out.data() + y * stride;
        const std::uint8_t* up = y > 0 ? out.data() + (y - 1) * stride : nullptr;
        for (std::size_t i = 0; i < stride; ++i) {
            const int a =
                i >= static_cast<std::size_t>(bpp) ? dst[i - static_cast<std::size_t>(bpp)] : 0;
            const int b = up != nullptr ? up[i] : 0;
            const int c = (up != nullptr && i >= static_cast<std::size_t>(bpp))
                              ? up[i - static_cast<std::size_t>(bpp)]
                              : 0;
            int v = src[i];
            switch (ft) {
                case 0: break;
                case 1: v += a; break;
                case 2: v += b; break;
                case 3: v += (a + b) / 2; break;
                case 4: v += paeth(a, b, c); break;
                default: error = "неизвестный фильтр строки"; return false;
            }
            dst[i] = static_cast<std::uint8_t>(v);
        }
    }
    return true;
}

/// Значение отсчёта (x-го по счёту в строке) нужной глубины.
std::uint16_t sampleAt(const std::uint8_t* row, std::size_t idx, int depth) {
    if (depth == 8) return row[idx];
    if (depth == 16) return be16(row + idx * 2);
    const std::size_t bitPos = idx * static_cast<std::size_t>(depth);
    const std::uint8_t byte = row[bitPos / 8];
    const int shift = 8 - depth - static_cast<int>(bitPos % 8);
    return static_cast<std::uint16_t>((byte >> shift) & ((1 << depth) - 1));
}

/// Отсчёт в 8 бит: низкие глубины растягиваются, 16 бит — старший байт.
std::uint8_t to8(std::uint16_t v, int depth) {
    if (depth == 8) return static_cast<std::uint8_t>(v);
    if (depth == 16) return static_cast<std::uint8_t>(v >> 8);
    return static_cast<std::uint8_t>(v * 255 / ((1 << depth) - 1));
}

/// Пиксель x из распакованной строки.
Color pixelAt(const Format& f, const std::uint8_t* row, std::uint32_t x) {
    const int ch = channels(f.ctype);
    const std::size_t base = static_cast<std::size_t>(x) * static_cast<std::size_t>(ch);
    switch (f.ctype) {
        case 0: {
            const std::uint16_t g = sampleAt(row, base, f.depth);
            const std::uint8_t g8 = to8(g, f.depth);
            const bool clear = f.hasTrns && g == f.trnsKey[0];
            return Color(clear ? 0 : 255, g8, g8, g8);
        }
        case 2: {
            const std::uint16_t r = sampleAt(row, base, f.depth);
            const std::uint16_t g = sampleAt(row, base + 1, f.depth);
            const std::uint16_t b = sampleAt(row, base + 2, f.depth);
            const bool clear =
                f.hasTrns && r == f.trnsKey[0] && g == f.trnsKey[1] && b == f.trnsKey[2];
            return Color(clear ? 0 : 255, to8(r, f.depth), to8(g, f.depth), to8(b, f.depth));
        }
        case 3: {
            const std::uint16_t i = sampleAt(row, base, f.depth);
            // Индекс за пределами палитры — чёрный непрозрачный, как у
            // большинства читателей, а не отказ открыть файл.
            return i < f.palette.size() ? f.palette[i] : Color(255, 0, 0, 0);
        }
        case 4: {
            const std::uint8_t g = to8(sampleAt(row, base, f.depth), f.depth);
            return Color(to8(sampleAt(row, base + 1, f.depth), f.depth), g, g, g);
        }
        default:
            return Color(to8(sampleAt(row, base + 3, f.depth), f.depth),
                         to8(sampleAt(row, base, f.depth), f.depth),
                         to8(sampleAt(row, base + 1, f.depth), f.depth),
                         to8(sampleAt(row, base + 2, f.depth), f.depth));
    }
}

/// Распакованный поток изображения w x h -> картинка, с чересстрочностью.
bool decodeImage(const Format& f, std::uint32_t w, std::uint32_t h,
                 const std::vector<std::uint8_t>& raw, Grid<Color>& out, std::string& error) {
    const int ch = channels(f.ctype);
    const int bitsPerPixel = ch * f.depth;
    const int bpp = std::max(1, bitsPerPixel / 8);
    out.resize(static_cast<int>(w), static_cast<int>(h));

    auto passStride = [&](std::uint32_t pw) {
        return (static_cast<std::size_t>(pw) * static_cast<std::size_t>(bitsPerPixel) + 7) / 8;
    };

    if (!f.interlaced) {
        std::vector<std::uint8_t> pix;
        const std::size_t stride = passStride(w);
        if (!unfilter(raw.data(), raw.size(), w, h, bpp, stride, pix, error)) return false;
        for (std::uint32_t y = 0; y < h; ++y) {
            const std::uint8_t* row = pix.data() + y * stride;
            for (std::uint32_t x = 0; x < w; ++x)
                out.at(static_cast<int>(x), static_cast<int>(y)) = pixelAt(f, row, x);
        }
        return true;
    }

    static constexpr int kX0[7] = {0, 4, 0, 2, 0, 1, 0};
    static constexpr int kY0[7] = {0, 0, 4, 0, 2, 0, 1};
    static constexpr int kDX[7] = {8, 8, 4, 4, 2, 2, 1};
    static constexpr int kDY[7] = {8, 8, 8, 4, 4, 2, 2};
    std::size_t off = 0;
    for (int p = 0; p < 7; ++p) {
        const std::uint32_t pw = w > static_cast<std::uint32_t>(kX0[p])
                                     ? (w - static_cast<std::uint32_t>(kX0[p]) +
                                        static_cast<std::uint32_t>(kDX[p]) - 1) /
                                           static_cast<std::uint32_t>(kDX[p])
                                     : 0;
        const std::uint32_t ph = h > static_cast<std::uint32_t>(kY0[p])
                                     ? (h - static_cast<std::uint32_t>(kY0[p]) +
                                        static_cast<std::uint32_t>(kDY[p]) - 1) /
                                           static_cast<std::uint32_t>(kDY[p])
                                     : 0;
        if (pw == 0 || ph == 0) continue;
        const std::size_t stride = passStride(pw);
        if (off > raw.size()) {
            error = "данных изображения меньше, чем обещает заголовок";
            return false;
        }
        std::vector<std::uint8_t> pix;
        if (!unfilter(raw.data() + off, raw.size() - off, pw, ph, bpp, stride, pix, error))
            return false;
        off += (stride + 1) * ph;
        for (std::uint32_t y = 0; y < ph; ++y) {
            const std::uint8_t* row = pix.data() + y * stride;
            for (std::uint32_t x = 0; x < pw; ++x) {
                out.at(kX0[p] + static_cast<int>(x) * kDX[p],
                       kY0[p] + static_cast<int>(y) * kDY[p]) = pixelAt(f, row, x);
            }
        }
    }
    return true;
}

struct FrameCtl {
    std::uint32_t w = 0, h = 0, x = 0, y = 0;
    std::uint16_t delayNum = 0, delayDen = 0;
    std::uint8_t dispose = 0, blend = 0;
};

/// Общий разбор файла: формат, кадры анимации и их данные.
struct Parsed {
    Format f;
    bool animated = false;
    std::uint32_t numPlays = 0;
    std::vector<std::uint8_t> idat;
    /// fcTL перед IDAT: картинка по умолчанию — первый кадр анимации.
    bool idatIsFrame = false;
    FrameCtl idatCtl;
    std::vector<std::pair<FrameCtl, std::vector<std::uint8_t>>> frames;  // после IDAT
};

bool parse(const std::vector<std::uint8_t>& file, Parsed& ps, std::string& error) {
    if (file.size() < 8 || std::memcmp(file.data(), kSig.data(), 8) != 0) {
        error = "это не PNG";
        return false;
    }
    std::size_t pos = 8;
    bool haveHeader = false;
    bool seenIdat = false;
    bool haveEnd = false;
    FrameCtl pending;
    bool havePending = false;
    std::vector<std::uint8_t>* fdatTarget = nullptr;
    Format& f = ps.f;
    while (pos + 12 <= file.size()) {
        Chunk c;
        c.len = be32(file.data() + pos);
        c.type.assign(reinterpret_cast<const char*>(file.data() + pos + 4), 4);
        if (c.len > file.size() - pos - 12) {
            error = "кусок " + c.type + " обрезан";
            return false;
        }
        c.data = file.data() + pos + 8;
        const std::uint32_t crc = be32(file.data() + pos + 8 + c.len);
        if (crc32(file.data() + pos + 4, c.len + 4) != crc) {
            // Испорченный служебный кусок не мешает картинке; испорченный
            // обязательный — значит, испорчена и она.
            const bool critical = (c.type[0] & 0x20) == 0;
            if (critical) {
                error = "контрольная сумма куска " + c.type + " не сходится";
                return false;
            }
            pos += 12 + c.len;
            continue;
        }
        pos += 12 + c.len;

        if (c.type == "IHDR") {
            if (c.len != 13) {
                error = "неверный заголовок";
                return false;
            }
            f.w = be32(c.data);
            f.h = be32(c.data + 4);
            f.depth = c.data[8];
            f.ctype = c.data[9];
            f.interlaced = c.data[12] == 1;
            if (f.w == 0 || f.h == 0 || f.w > kMaxSide || f.h > kMaxSide) {
                error = "недопустимый размер изображения";
                return false;
            }
            if (!validDepth(f.ctype, f.depth) || c.data[10] != 0 || c.data[11] != 0 ||
                c.data[12] > 1) {
                error = "неподдерживаемый вид PNG";
                return false;
            }
            haveHeader = true;
        } else if (!haveHeader) {
            error = "нет заголовка IHDR";
            return false;
        } else if (c.type == "PLTE") {
            f.palette.clear();
            for (std::uint32_t i = 0; i + 2 < c.len; i += 3)
                f.palette.emplace_back(255, c.data[i], c.data[i + 1], c.data[i + 2]);
        } else if (c.type == "tRNS") {
            if (f.ctype == 3) {
                for (std::uint32_t i = 0; i < c.len && i < f.palette.size(); ++i)
                    f.palette[i].a = c.data[i];
            } else if (f.ctype == 0 && c.len >= 2) {
                f.hasTrns = true;
                f.trnsKey[0] = be16(c.data);
            } else if (f.ctype == 2 && c.len >= 6) {
                f.hasTrns = true;
                f.trnsKey = {be16(c.data), be16(c.data + 2), be16(c.data + 4)};
            }
        } else if (c.type == "acTL" && c.len >= 8) {
            ps.animated = true;
            ps.numPlays = be32(c.data + 4);
        } else if (c.type == "fcTL" && c.len >= 26) {
            FrameCtl fc;
            fc.w = be32(c.data + 4);
            fc.h = be32(c.data + 8);
            fc.x = be32(c.data + 12);
            fc.y = be32(c.data + 16);
            fc.delayNum = be16(c.data + 20);
            fc.delayDen = be16(c.data + 22);
            fc.dispose = c.data[24];
            fc.blend = c.data[25];
            if (fc.w == 0 || fc.h == 0 || fc.x + fc.w > f.w || fc.y + fc.h > f.h) {
                error = "кадр анимации выходит за холст";
                return false;
            }
            if (!seenIdat) {
                ps.idatIsFrame = true;
                ps.idatCtl = fc;
            } else {
                ps.frames.emplace_back(fc, std::vector<std::uint8_t>{});
                fdatTarget = &ps.frames.back().second;
            }
            havePending = true;
            pending = fc;
        } else if (c.type == "IDAT") {
            seenIdat = true;
            ps.idat.insert(ps.idat.end(), c.data, c.data + c.len);
        } else if (c.type == "fdAT" && c.len >= 4) {
            if (fdatTarget != nullptr)
                fdatTarget->insert(fdatTarget->end(), c.data + 4, c.data + c.len);
        } else if (c.type == "IEND") {
            haveEnd = true;
            break;
        }
    }
    (void)havePending;
    (void)haveEnd;  // файл без IEND, но с целыми данными открываем
    if (!haveHeader) {
        error = "нет заголовка IHDR";
        return false;
    }
    if (ps.idat.empty()) {
        error = "нет данных изображения";
        return false;
    }
    if (f.ctype == 3 && f.palette.empty()) {
        error = "у картинки с палитрой нет палитры";
        return false;
    }
    return true;
}

bool decodeStream(const Format& f, std::uint32_t w, std::uint32_t h,
                  const std::vector<std::uint8_t>& z, Grid<Color>& out, std::string& error) {
    std::vector<std::uint8_t> raw;
    // Предел распаковки — сколько вообще может занять картинка, с запасом.
    const std::size_t limit = (static_cast<std::size_t>(w) * 8 + 1) * h * 2 + 1024;
    if (!zlibDecompress(z.data(), z.size(), raw, error, limit)) {
        error = "данные изображения испорчены: " + error;
        return false;
    }
    return decodeImage(f, w, h, raw, out, error);
}

// --- запись ------------------------------------------------------------------

void putBe32(std::vector<std::uint8_t>& v, std::uint32_t x) {
    v.push_back(static_cast<std::uint8_t>(x >> 24));
    v.push_back(static_cast<std::uint8_t>(x >> 16));
    v.push_back(static_cast<std::uint8_t>(x >> 8));
    v.push_back(static_cast<std::uint8_t>(x));
}

void putBe16(std::vector<std::uint8_t>& v, std::uint16_t x) {
    v.push_back(static_cast<std::uint8_t>(x >> 8));
    v.push_back(static_cast<std::uint8_t>(x));
}

void writeChunk(std::vector<std::uint8_t>& out, const char* type,
                const std::vector<std::uint8_t>& data) {
    putBe32(out, static_cast<std::uint32_t>(data.size()));
    const std::size_t start = out.size();
    out.insert(out.end(), type, type + 4);
    out.insert(out.end(), data.begin(), data.end());
    putBe32(out, crc32(out.data() + start, data.size() + 4));
}

/// Строки RGBA с фильтрами: для каждой строки — тот, что даёт наименьшую
/// сумму модулей, как эвристика libpng.
std::vector<std::uint8_t> filteredRows(const Grid<Color>& img) {
    const auto w = static_cast<std::size_t>(img.w);
    const std::size_t stride = w * 4;
    std::vector<std::uint8_t> prev(stride, 0);
    std::vector<std::uint8_t> cur(stride);
    std::vector<std::uint8_t> out;
    out.reserve((stride + 1) * static_cast<std::size_t>(img.h));
    std::array<std::vector<std::uint8_t>, 5> cand;
    for (auto& c : cand)
        c.resize(stride);
    for (int y = 0; y < img.h; ++y) {
        for (int x = 0; x < img.w; ++x) {
            const Color c = img.at(x, y);
            const std::size_t k = static_cast<std::size_t>(x) * 4;
            cur[k] = c.r;
            cur[k + 1] = c.g;
            cur[k + 2] = c.b;
            cur[k + 3] = c.a;
        }
        long best = -1;
        int bestType = 0;
        for (int t = 0; t < 5; ++t) {
            long sum = 0;
            for (std::size_t i = 0; i < stride; ++i) {
                const int a = i >= 4 ? cur[i - 4] : 0;
                const int b = prev[i];
                const int c = i >= 4 ? prev[i - 4] : 0;
                int pred = 0;
                switch (t) {
                    case 1: pred = a; break;
                    case 2: pred = b; break;
                    case 3: pred = (a + b) / 2; break;
                    case 4: pred = paeth(a, b, c); break;
                    default: break;
                }
                const auto v = static_cast<std::uint8_t>(cur[i] - pred);
                cand[static_cast<std::size_t>(t)][i] = v;
                sum += v < 128 ? v : 256 - v;
            }
            if (best < 0 || sum < best) {
                best = sum;
                bestType = t;
            }
        }
        out.push_back(static_cast<std::uint8_t>(bestType));
        out.insert(out.end(), cand[static_cast<std::size_t>(bestType)].begin(),
                   cand[static_cast<std::size_t>(bestType)].end());
        prev.swap(cur);
    }
    return out;
}

std::vector<std::uint8_t> ihdr(int w, int h) {
    std::vector<std::uint8_t> d;
    putBe32(d, static_cast<std::uint32_t>(w));
    putBe32(d, static_cast<std::uint32_t>(h));
    d.push_back(8);  // глубина
    d.push_back(6);  // RGBA
    d.push_back(0);
    d.push_back(0);
    d.push_back(0);
    return d;
}

}  // namespace

bool pngDecode(const std::vector<std::uint8_t>& file, Grid<Color>& out, std::string& error,
               PngInfo* info) {
    Parsed ps;
    if (!parse(file, ps, error)) return false;
    if (info != nullptr) {
        info->width = static_cast<int>(ps.f.w);
        info->height = static_cast<int>(ps.f.h);
        info->bitDepth = ps.f.depth;
        info->colorType = ps.f.ctype;
        info->interlaced = ps.f.interlaced;
        info->animated = ps.animated;
    }
    return decodeStream(ps.f, ps.f.w, ps.f.h, ps.idat, out, error);
}

std::vector<std::uint8_t> pngEncode(const Grid<Color>& img, int level) {
    std::vector<std::uint8_t> out(kSig.begin(), kSig.end());
    writeChunk(out, "IHDR", ihdr(img.w, img.h));
    writeChunk(out, "IDAT", zlibCompress(filteredRows(img), level));
    writeChunk(out, "IEND", {});
    return out;
}

bool pngLoad(const std::string& path, Grid<Color>& out, std::string& error) {
    const auto data = readFile(path);
    if (!data) {
        error = "не удалось прочитать файл";
        return false;
    }
    const std::vector<std::uint8_t> bytes(data->begin(), data->end());
    return pngDecode(bytes, out, error);
}

bool pngSave(const std::string& path, const Grid<Color>& img, std::string& error) {
    const auto bytes = pngEncode(img);
    if (!writeFile(path,
                   std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()))) {
        error = "не удалось записать файл";
        return false;
    }
    return true;
}

bool apngDecode(const std::vector<std::uint8_t>& file, std::vector<AnimFrame>& frames,
                std::string& error, int* loops) {
    frames.clear();
    Parsed ps;
    if (!parse(file, ps, error)) return false;
    if (loops != nullptr) *loops = static_cast<int>(ps.numPlays);
    const auto cw = static_cast<int>(ps.f.w);
    const auto ch = static_cast<int>(ps.f.h);

    auto duration = [](const FrameCtl& fc) {
        const int den = fc.delayDen == 0 ? 100 : fc.delayDen;
        return std::max(10, pround(fc.delayNum * 1000.0 / den));
    };

    if (!ps.animated || (!ps.idatIsFrame && ps.frames.empty())) {
        AnimFrame fr;
        if (!decodeStream(ps.f, ps.f.w, ps.f.h, ps.idat, fr.image, error)) return false;
        frames.push_back(std::move(fr));
        return true;
    }

    // Холст с нуля прозрачный; каждый кадр рисуется на нём по своим
    // правилам наложения, а после показа холст очищается по правилам кадра.
    Grid<Color> canvas(cw, ch, Color(0, 0, 0, 0));
    std::vector<std::pair<FrameCtl, const std::vector<std::uint8_t>*>> list;
    if (ps.idatIsFrame) list.emplace_back(ps.idatCtl, &ps.idat);
    for (const auto& fr : ps.frames)
        list.emplace_back(fr.first, &fr.second);

    bool first = true;
    for (const auto& [fc, data] : list) {
        Grid<Color> part;
        if (!decodeStream(ps.f, fc.w, fc.h, *data, part, error)) return false;
        Grid<Color> saved;
        std::uint8_t dispose = fc.dispose;
        if (dispose == 2 && first) dispose = 1;  // «вернуть прежнее» у первого кадра — очистить
        if (dispose == 2) saved = canvas;
        for (int y = 0; y < part.h; ++y) {
            for (int x = 0; x < part.w; ++x) {
                Color& dst = canvas.at(static_cast<int>(fc.x) + x, static_cast<int>(fc.y) + y);
                dst = fc.blend == 1 ? blendOver(part.at(x, y), dst) : part.at(x, y);
            }
        }
        frames.push_back(AnimFrame{canvas, duration(fc)});
        if (dispose == 1) {
            for (int y = 0; y < part.h; ++y) {
                for (int x = 0; x < part.w; ++x)
                    canvas.at(static_cast<int>(fc.x) + x, static_cast<int>(fc.y) + y) =
                        Color(0, 0, 0, 0);
            }
        } else if (dispose == 2) {
            canvas = std::move(saved);
        }
        first = false;
    }
    return true;
}

std::vector<std::uint8_t> apngEncode(const std::vector<AnimFrame>& frames, int loops, int level) {
    if (frames.empty()) return {};
    const Grid<Color>& first = frames.front().image;
    std::vector<std::uint8_t> out(kSig.begin(), kSig.end());
    writeChunk(out, "IHDR", ihdr(first.w, first.h));
    std::vector<std::uint8_t> actl;
    putBe32(actl, static_cast<std::uint32_t>(frames.size()));
    putBe32(actl, static_cast<std::uint32_t>(std::max(0, loops)));
    writeChunk(out, "acTL", actl);
    std::uint32_t seq = 0;
    for (std::size_t i = 0; i < frames.size(); ++i) {
        const AnimFrame& fr = frames[i];
        std::vector<std::uint8_t> fc;
        putBe32(fc, seq++);
        putBe32(fc, static_cast<std::uint32_t>(first.w));
        putBe32(fc, static_cast<std::uint32_t>(first.h));
        putBe32(fc, 0);
        putBe32(fc, 0);
        // Длительность в тысячных долях секунды: точно и без округлений.
        putBe16(fc, static_cast<std::uint16_t>(clampI(fr.durationMs, 1, 65535)));
        putBe16(fc, 1000);
        fc.push_back(0);  // кадр остаётся на холсте
        fc.push_back(0);  // и заменяет его целиком
        writeChunk(out, "fcTL", fc);
        // Кадр всегда во весь холст и того же размера: если чужой кадр
        // оказался другого размера, он подгоняется прозрачной рамкой.
        Grid<Color> img = fr.image;
        if (img.w != first.w || img.h != first.h) {
            Grid<Color> fit(first.w, first.h, Color(0, 0, 0, 0));
            for (int y = 0; y < std::min(img.h, fit.h); ++y) {
                for (int x = 0; x < std::min(img.w, fit.w); ++x)
                    fit.at(x, y) = img.at(x, y);
            }
            img = std::move(fit);
        }
        const auto z = zlibCompress(filteredRows(img), level);
        if (i == 0) {
            writeChunk(out, "IDAT", z);
        } else {
            std::vector<std::uint8_t> fd;
            putBe32(fd, seq++);
            fd.insert(fd.end(), z.begin(), z.end());
            writeChunk(out, "fdAT", fd);
        }
    }
    writeChunk(out, "IEND", {});
    return out;
}

}  // namespace tjtm
