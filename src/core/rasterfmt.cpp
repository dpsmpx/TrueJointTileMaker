#include "core/rasterfmt.h"

#include <algorithm>
#include <cstring>
#include <set>

#include "core/zlib.h"

namespace tjtm {

namespace {

void putLe16(std::vector<std::uint8_t>& o, std::uint32_t v) {
    o.push_back(static_cast<std::uint8_t>(v & 0xFF));
    o.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
}

void putLe32(std::vector<std::uint8_t>& o, std::uint32_t v) {
    putLe16(o, v & 0xFFFF);
    putLe16(o, v >> 16);
}

void setLe32(std::vector<std::uint8_t>& o, std::size_t at, std::uint32_t v) {
    for (int k = 0; k < 4; ++k)
        o[at + static_cast<std::size_t>(k)] = static_cast<std::uint8_t>((v >> (8 * k)) & 0xFF);
}

bool opaque(const Grid<Color>& g) {
    return std::all_of(g.d.begin(), g.d.end(), [](Color c) { return c.a == 255; });
}

constexpr int kMaxSide = 1 << 14;
constexpr std::size_t kMaxPages = 4096;

}  // namespace

// ---------------------------------------------------------------------------
// BMP
// ---------------------------------------------------------------------------

std::vector<std::uint8_t> bmpEncode(const Grid<Color>& img) {
    const bool alpha = !opaque(img);
    const std::uint32_t infoSize = alpha ? 108 : 40;
    const std::uint32_t bpp = alpha ? 32 : 24;
    const std::uint32_t rowBytes = ((static_cast<std::uint32_t>(img.w) * bpp + 31) / 32) * 4;
    const std::uint32_t dataSize = rowBytes * static_cast<std::uint32_t>(img.h);
    const std::uint32_t offset = 14 + infoSize;
    std::vector<std::uint8_t> o;
    o.reserve(offset + dataSize);
    o.push_back('B');
    o.push_back('M');
    putLe32(o, offset + dataSize);
    putLe32(o, 0);
    putLe32(o, offset);
    putLe32(o, infoSize);
    putLe32(o, static_cast<std::uint32_t>(img.w));
    putLe32(o, static_cast<std::uint32_t>(img.h));  // снизу вверх, как принято
    putLe16(o, 1);
    putLe16(o, bpp);
    putLe32(o, alpha ? 3 : 0);  // BI_BITFIELDS — маски цветов и альфы
    putLe32(o, dataSize);
    putLe32(o, 2835);  // 72 точки на дюйм
    putLe32(o, 2835);
    putLe32(o, 0);
    putLe32(o, 0);
    if (alpha) {
        putLe32(o, 0x00FF0000);
        putLe32(o, 0x0000FF00);
        putLe32(o, 0x000000FF);
        putLe32(o, 0xFF000000);
        putLe32(o, 0x73524742);  // 'sRGB'
        for (int k = 0; k < 12; ++k)
            putLe32(o, 0);  // конечные точки и гамма не нужны при sRGB
    }
    for (int y = img.h - 1; y >= 0; --y) {
        std::size_t written = 0;
        for (int x = 0; x < img.w; ++x) {
            const Color c = img.at(x, y);
            o.push_back(c.b);
            o.push_back(c.g);
            o.push_back(c.r);
            written += 3;
            if (alpha) {
                o.push_back(c.a);
                ++written;
            }
        }
        for (; written < rowBytes; ++written)
            o.push_back(0);
    }
    return o;
}

// ---------------------------------------------------------------------------
// TIFF: LZW
// ---------------------------------------------------------------------------

std::vector<std::uint8_t> tiffLzwEncode(const std::vector<std::uint8_t>& data) {
    std::vector<std::uint8_t> out;
    std::uint32_t acc = 0;
    int bits = 0;
    int width = 9;
    auto put = [&](int code) {
        acc = (acc << width) | static_cast<std::uint32_t>(code);
        bits += width;
        while (bits >= 8) {
            out.push_back(static_cast<std::uint8_t>((acc >> (bits - 8)) & 0xFF));
            bits -= 8;
        }
        acc &= (1U << bits) - 1;
    };
    constexpr int kClear = 256;
    constexpr int kEoi = 257;
    int next = 258;
    // Словарь «(префикс << 8 | байт) → код»: открытая адресация.
    struct Entry {
        std::uint32_t key;
        int code;
    };
    std::vector<Entry> table(1 << 14);
    auto clearTable = [&]() {
        for (Entry& e : table)
            e.code = -1;
    };
    auto find = [&](std::uint32_t key) -> Entry& {
        std::size_t h = (key * 2654435761U) >> 18;
        while (table[h].code >= 0 && table[h].key != key)
            h = (h + 1) & (table.size() - 1);
        return table[h];
    };
    clearTable();
    put(kClear);
    if (data.empty()) {
        put(kEoi);
        if (bits > 0) out.push_back(static_cast<std::uint8_t>((acc << (8 - bits)) & 0xFF));
        return out;
    }
    int prefix = data[0];
    for (std::size_t i = 1; i < data.size(); ++i) {
        const int k = data[i];
        const std::uint32_t key =
            (static_cast<std::uint32_t>(prefix) << 8) | static_cast<std::uint32_t>(k);
        Entry& e = find(key);
        if (e.code >= 0) {
            prefix = e.code;
            continue;
        }
        put(prefix);
        e.key = key;
        e.code = next++;
        // Ширина кода растёт на код раньше, чем у GIF: так читает libtiff.
        if (next >= (1 << width) && width < 12) ++width;
        if (next >= 4094) {
            put(kClear);
            clearTable();
            next = 258;
            width = 9;
        }
        prefix = k;
    }
    put(prefix);
    // Код конца — уже той ширины, до какой дорос словарь после последнего
    // кода: читающий добавит запись и тоже перейдёт.
    if (next + 1 >= (1 << width) && width < 12) ++width;
    put(kEoi);
    if (bits > 0) out.push_back(static_cast<std::uint8_t>((acc << (8 - bits)) & 0xFF));
    return out;
}

bool tiffLzwDecode(const std::uint8_t* data, std::size_t n, std::vector<std::uint8_t>& out,
                   std::size_t expected) {
    out.clear();
    out.reserve(expected);
    constexpr int kClear = 256;
    constexpr int kEoi = 257;
    std::vector<std::uint16_t> prefix(4096);
    std::vector<std::uint8_t> suffix(4096);
    std::vector<std::uint8_t> first(4096);
    std::vector<std::uint8_t> stack(4097);
    for (int c = 0; c < 256; ++c) {
        suffix[static_cast<std::size_t>(c)] = static_cast<std::uint8_t>(c);
        first[static_cast<std::size_t>(c)] = static_cast<std::uint8_t>(c);
    }
    int width = 9;
    int next = 258;
    int prev = -1;
    std::uint32_t acc = 0;
    int bits = 0;
    std::size_t pos = 0;
    while (out.size() < expected) {
        while (bits < width && pos < n) {
            acc = (acc << 8) | data[pos++];
            bits += 8;
        }
        if (bits < width) break;
        const int code = static_cast<int>((acc >> (bits - width)) & ((1U << width) - 1));
        bits -= width;
        acc &= (1U << bits) - 1;
        if (code == kClear) {
            width = 9;
            next = 258;
            prev = -1;
            continue;
        }
        if (code == kEoi) break;
        if (prev < 0) {
            if (code > 255) return false;
            out.push_back(static_cast<std::uint8_t>(code));
            prev = code;
            continue;
        }
        int cur = code;
        std::size_t sp = 0;
        std::uint8_t lead = 0;
        if (code < next) {
            lead = first[static_cast<std::size_t>(code)];
        } else if (code == next) {
            lead = first[static_cast<std::size_t>(prev)];
            stack[sp++] = lead;
            cur = prev;
        } else {
            return false;
        }
        while (cur > 255) {
            stack[sp++] = suffix[static_cast<std::size_t>(cur)];
            cur = prefix[static_cast<std::size_t>(cur)];
        }
        stack[sp++] = static_cast<std::uint8_t>(cur);
        while (sp > 0 && out.size() < expected)
            out.push_back(stack[--sp]);
        if (next < 4096) {
            prefix[static_cast<std::size_t>(next)] = static_cast<std::uint16_t>(prev);
            suffix[static_cast<std::size_t>(next)] = lead;
            first[static_cast<std::size_t>(next)] = first[static_cast<std::size_t>(prev)];
            ++next;
            if (next >= (1 << width) - 1 && width < 12) ++width;
        }
        prev = code;
    }
    return true;
}

// ---------------------------------------------------------------------------
// TIFF: чтение
// ---------------------------------------------------------------------------

bool isTiff(const std::vector<std::uint8_t>& f) {
    return f.size() >= 8 && ((f[0] == 'I' && f[1] == 'I' && f[2] == 42 && f[3] == 0) ||
                             (f[0] == 'M' && f[1] == 'M' && f[2] == 0 && f[3] == 42));
}

namespace {

struct TiffReader {
    const std::vector<std::uint8_t>& f;
    bool be = false;
    std::uint32_t u16(std::size_t p) const {
        if (p + 2 > f.size()) return 0;
        return be ? (static_cast<std::uint32_t>(f[p]) << 8) | f[p + 1]
                  : (static_cast<std::uint32_t>(f[p + 1]) << 8) | f[p];
    }
    std::uint32_t u32(std::size_t p) const {
        if (p + 4 > f.size()) return 0;
        return be ? (u16(p) << 16) | u16(p + 2) : (u16(p + 2) << 16) | u16(p);
    }
};

struct Ifd {
    int w = 0, h = 0;
    std::vector<std::uint32_t> bps{1};
    int compression = 1;
    int photometric = -1;
    std::vector<std::uint32_t> offsets, counts;
    int spp = 1;
    int rowsPerStrip = 0;
    int planar = 1;
    int predictor = 1;
    std::vector<std::uint32_t> colormap;
    std::vector<std::uint32_t> extra;
    int tileW = 0, tileH = 0;
    int sampleFormat = 1;
    std::uint32_t next = 0;
};

std::vector<std::uint32_t> values(const TiffReader& r, std::size_t entry, std::size_t maxCount) {
    const std::uint32_t type = r.u16(entry + 2);
    const std::uint32_t count =
        std::min<std::uint32_t>(r.u32(entry + 4), static_cast<std::uint32_t>(maxCount));
    std::size_t size = 0;
    switch (type) {
        case 1:
        case 2:
        case 6:
        case 7: size = 1; break;
        case 3:
        case 8: size = 2; break;
        case 4:
        case 9: size = 4; break;
        default: return {};
    }
    const std::size_t at = size * count <= 4 ? entry + 8 : r.u32(entry + 8);
    if (at + size * count > r.f.size()) return {};
    std::vector<std::uint32_t> v(count);
    for (std::uint32_t k = 0; k < count; ++k) {
        const std::size_t p = at + k * size;
        v[k] = size == 1 ? r.f[p] : (size == 2 ? r.u16(p) : r.u32(p));
    }
    return v;
}

bool readIfd(const TiffReader& r, std::uint32_t at, Ifd& d, std::string& error) {
    const std::uint32_t n = r.u16(at);
    if (at + 2 + n * 12 + 4 > r.f.size()) {
        error = "оглавление TIFF обрезано";
        return false;
    }
    for (std::uint32_t i = 0; i < n; ++i) {
        const std::size_t e = at + 2 + i * 12;
        const std::uint32_t tag = r.u16(e);
        auto one = [&]() {
            const auto v = values(r, e, 1);
            return v.empty() ? 0 : static_cast<int>(v[0]);
        };
        switch (tag) {
            case 256: d.w = one(); break;
            case 257: d.h = one(); break;
            case 258: d.bps = values(r, e, 8); break;
            case 259: d.compression = one(); break;
            case 262: d.photometric = one(); break;
            case 273:
            case 324: d.offsets = values(r, e, 1 << 20); break;
            case 277: d.spp = one(); break;
            case 278: d.rowsPerStrip = one(); break;
            case 279:
            case 325: d.counts = values(r, e, 1 << 20); break;
            case 284: d.planar = one(); break;
            case 317: d.predictor = one(); break;
            case 320: d.colormap = values(r, e, std::size_t(3) * 65536); break;
            case 322: d.tileW = one(); break;
            case 323: d.tileH = one(); break;
            case 338: d.extra = values(r, e, 8); break;
            case 339: d.sampleFormat = one(); break;
            default: break;
        }
    }
    d.next = r.u32(at + 2 + n * 12);
    return true;
}

bool unpack(const std::vector<std::uint8_t>& f, std::uint32_t off, std::uint32_t cnt,
            int compression, std::size_t expected, std::vector<std::uint8_t>& out,
            std::string& error) {
    if (off > f.size() || cnt > f.size() - off) {
        error = "данные TIFF за концом файла";
        return false;
    }
    const std::uint8_t* p = f.data() + off;
    out.clear();
    switch (compression) {
        case 1: out.assign(p, p + std::min<std::size_t>(cnt, expected)); break;
        case 5:
            if (!tiffLzwDecode(p, cnt, out, expected)) {
                error = "поток LZW испорчен";
                return false;
            }
            break;
        case 8:
        case 32946:
            if (!zlibDecompress(p, cnt, out, error, expected + 64)) return false;
            break;
        case 32773: {
            // PackBits: длина со знаком — повтор или подряд идущие байты.
            std::size_t i = 0;
            while (i < cnt && out.size() < expected) {
                const auto n = static_cast<std::int8_t>(p[i++]);
                if (n >= 0) {
                    for (int k = 0; k <= n && i < cnt; ++k)
                        out.push_back(p[i++]);
                } else if (n != -128 && i < cnt) {
                    out.insert(out.end(), static_cast<std::size_t>(1 - n), p[i++]);
                }
            }
            break;
        }
        default:
            error = "сжатие TIFF " + std::to_string(compression) +
                    " не поддержано: сохраните без сжатия, LZW или Deflate";
            return false;
    }
    out.resize(expected, 0);
    return true;
}

}  // namespace

bool tiffDecode(const std::vector<std::uint8_t>& f, std::vector<Grid<Color>>& pages,
                std::string& error) {
    pages.clear();
    if (!isTiff(f)) {
        error = "это не TIFF";
        return false;
    }
    TiffReader r{f, f[0] == 'M'};
    std::uint32_t at = r.u32(4);
    std::set<std::uint32_t> seen;
    while (at != 0 && pages.size() < kMaxPages) {
        if (!seen.insert(at).second) break;  // петля в цепочке оглавлений
        Ifd d;
        if (!readIfd(r, at, d, error)) return !pages.empty();
        at = d.next;
        if (d.w < 1 || d.h < 1 || d.w > kMaxSide || d.h > kMaxSide) {
            error = "недопустимый размер страницы TIFF";
            return false;
        }
        const int bps = static_cast<int>(d.bps.empty() ? 1 : d.bps[0]);
        if (bps != 1 && bps != 2 && bps != 4 && bps != 8 && bps != 16) {
            error = "TIFF с " + std::to_string(bps) + " битами на канал не поддержан";
            return false;
        }
        if (d.sampleFormat != 1) {
            error = "TIFF с дробными или знаковыми каналами не поддержан";
            return false;
        }
        if (d.photometric < 0) d.photometric = d.spp >= 3 ? 2 : 1;
        if (d.photometric > 3) {
            error = "TIFF в CMYK, YCbCr и Lab не поддержан: сохраните в RGB";
            return false;
        }
        const int base = d.photometric == 2 ? 3 : 1;
        if (d.spp < base || d.spp > 8) {
            error = "число каналов TIFF не сходится с его цветом";
            return false;
        }
        const bool hasAlpha = d.spp > base;
        const bool premul = hasAlpha && !d.extra.empty() && d.extra[0] == 1;
        const bool tiled = d.tileW > 0 && d.tileH > 0;
        const int cw = tiled ? d.tileW : d.w;
        const int chunkH =
            tiled ? d.tileH : (d.rowsPerStrip > 0 ? std::min(d.rowsPerStrip, d.h) : d.h);
        const int across = tiled ? (d.w + cw - 1) / cw : 1;
        const int down = (d.h + chunkH - 1) / chunkH;
        const int planes = d.planar == 2 ? d.spp : 1;
        const int sppChunk = d.planar == 2 ? 1 : d.spp;
        // Размеры — без знака: на них считаются смещения в буферах.
        const auto uw = static_cast<std::size_t>(d.w);
        const auto uspp = static_cast<std::size_t>(d.spp);
        const auto ubps = static_cast<std::size_t>(bps);
        const auto uchunk = static_cast<std::size_t>(sppChunk);
        const std::size_t rowBytes = (static_cast<std::size_t>(cw) * uchunk * ubps + 7) / 8;
        if (d.offsets.size() < static_cast<std::size_t>(across * down * planes) ||
            d.counts.size() < d.offsets.size()) {
            error = "у TIFF не хватает полос или плиток";
            return false;
        }
        // Значения каналов страницы, 16 бит на значение: так проще и для 1,
        // и для 16 бит.
        std::vector<std::uint16_t> px(uw * static_cast<std::size_t>(d.h) * uspp, 0);
        std::vector<std::uint8_t> buf;
        for (int pl = 0; pl < planes; ++pl) {
            for (int ty = 0; ty < down; ++ty) {
                for (int tx = 0; tx < across; ++tx) {
                    const auto idx =
                        static_cast<std::size_t>(pl * across * down + ty * across + tx);
                    const int rows = tiled ? chunkH : std::min(chunkH, d.h - ty * chunkH);
                    if (!unpack(f, d.offsets[idx], d.counts[idx], d.compression,
                                rowBytes * static_cast<std::size_t>(rows), buf, error))
                        return false;
                    // Горизонтальный предсказатель: точка хранит разницу с
                    // соседкой слева.
                    if (d.predictor == 2 && (bps == 8 || bps == 16)) {
                        for (int y = 0; y < rows; ++y) {
                            std::uint8_t* row = buf.data() + static_cast<std::size_t>(y) * rowBytes;
                            if (bps == 8) {
                                for (std::size_t k = uchunk; k < rowBytes; ++k)
                                    row[k] = static_cast<std::uint8_t>(
                                        row[k] + row[k - static_cast<std::size_t>(sppChunk)]);
                            } else {
                                const std::size_t step = static_cast<std::size_t>(sppChunk) * 2;
                                for (std::size_t k = step; k + 1 < rowBytes; k += 2) {
                                    auto word = [&](std::size_t i) {
                                        const std::uint32_t hi = row[r.be ? i : i + 1];
                                        const std::uint32_t lo = row[r.be ? i + 1 : i];
                                        return hi << 8 | lo;
                                    };
                                    const std::uint32_t a = word(k);
                                    const std::uint32_t b = word(k - step);
                                    const std::uint32_t v = (a + b) & 0xFFFF;
                                    row[k] = static_cast<std::uint8_t>(r.be ? v >> 8 : v & 0xFF);
                                    row[k + 1] =
                                        static_cast<std::uint8_t>(r.be ? v & 0xFF : v >> 8);
                                }
                            }
                        }
                    }
                    for (int y = 0; y < rows; ++y) {
                        const int gy = ty * chunkH + y;
                        if (gy >= d.h) break;
                        const std::uint8_t* row =
                            buf.data() + static_cast<std::size_t>(y) * rowBytes;
                        for (int x = 0; x < cw; ++x) {
                            const int gx = tx * cw + x;
                            if (gx >= d.w) break;
                            for (int s = 0; s < sppChunk; ++s) {
                                const std::size_t bit = (static_cast<std::size_t>(x) * uchunk +
                                                         static_cast<std::size_t>(s)) *
                                                        ubps;
                                std::uint32_t v = 0;
                                if (bps == 16) {
                                    const std::size_t b = bit / 8;
                                    v = r.be ? (row[b] << 8 | row[b + 1])
                                             : (row[b + 1] << 8 | row[b]);
                                } else if (bps == 8) {
                                    v = row[bit / 8];
                                } else {
                                    v = (row[bit / 8] >> (8 - bps - static_cast<int>(bit % 8))) &
                                        ((1U << bps) - 1);
                                }
                                const int ch = d.planar == 2 ? pl : s;
                                px[(static_cast<std::size_t>(gy) * uw +
                                    static_cast<std::size_t>(gx)) *
                                       uspp +
                                   static_cast<std::size_t>(ch)] = static_cast<std::uint16_t>(v);
                            }
                        }
                    }
                }
            }
        }
        auto to8 = [bps](std::uint32_t v) {
            // 16 бит — с округлением, а не отбрасыванием младшего байта:
            // у палитр и картинок не из 8 бит значения не кратны 257.
            if (bps == 16) return static_cast<int>((v * 255 + 32767) / 65535);
            if (bps == 8) return static_cast<int>(v);
            return static_cast<int>(v * 255 / ((1U << bps) - 1));
        };
        const std::size_t ncolors = std::size_t(1) << bps;
        if (d.photometric == 3 && d.colormap.size() < 3 * ncolors) {
            error = "у TIFF с палитрой нет таблицы цветов";
            return false;
        }
        Grid<Color> g(d.w, d.h);
        for (std::size_t k = 0; k < g.d.size(); ++k) {
            const std::uint16_t* s = &px[k * uspp];
            int R = 0;
            int G = 0;
            int B = 0;
            if (d.photometric == 2) {
                R = to8(s[0]);
                G = to8(s[1]);
                B = to8(s[2]);
            } else if (d.photometric == 3) {
                const std::size_t i = std::min<std::size_t>(s[0], ncolors - 1);
                auto c8 = [](std::uint32_t v) {
                    return static_cast<int>((v * 255 + 32767) / 65535);
                };
                R = c8(d.colormap[i]);
                G = c8(d.colormap[i + ncolors]);
                B = c8(d.colormap[i + 2 * ncolors]);
            } else {
                const int v = to8(s[0]);
                R = G = B = d.photometric == 0 ? 255 - v : v;
            }
            int A = hasAlpha ? to8(s[base]) : 255;
            if (premul && A > 0 && A < 255) {
                R = std::min(255, (R * 255 + A / 2) / A);
                G = std::min(255, (G * 255 + A / 2) / A);
                B = std::min(255, (B * 255 + A / 2) / A);
            }
            if (A == 0) R = G = B = 0;
            g.d[k] = argb(static_cast<std::uint8_t>(A), static_cast<std::uint8_t>(R),
                          static_cast<std::uint8_t>(G), static_cast<std::uint8_t>(B));
        }
        pages.push_back(std::move(g));
    }
    if (pages.empty()) {
        error = "в TIFF нет страниц";
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// TIFF: запись
// ---------------------------------------------------------------------------

std::vector<std::uint8_t> tiffEncode(const std::vector<Grid<Color>>& pages) {
    std::vector<std::uint8_t> o = {'I', 'I', 42, 0, 0, 0, 0, 0};
    std::size_t prevNext = 4;  // куда вписать смещение следующего оглавления
    for (const Grid<Color>& g : pages) {
        if (g.empty()) continue;
        const bool alpha = !opaque(g);
        const int spp = alpha ? 4 : 3;
        const auto uspp = static_cast<std::size_t>(spp);
        const std::size_t rowBytes = static_cast<std::size_t>(g.w) * uspp;
        std::vector<std::uint8_t> raw(rowBytes * static_cast<std::size_t>(g.h));
        for (int y = 0; y < g.h; ++y) {
            std::uint8_t* row = raw.data() + static_cast<std::size_t>(y) * rowBytes;
            for (int x = 0; x < g.w; ++x) {
                const Color c = g.at(x, y);
                std::uint8_t* p = row + static_cast<std::size_t>(x) * uspp;
                p[0] = c.r;
                p[1] = c.g;
                p[2] = c.b;
                if (alpha) p[3] = c.a;
            }
            // Предсказатель — с конца строки, чтобы брать ещё не тронутых соседей.
            for (std::size_t k = rowBytes; k-- > static_cast<std::size_t>(spp);)
                row[k] = static_cast<std::uint8_t>(row[k] - row[k - static_cast<std::size_t>(spp)]);
        }
        const std::vector<std::uint8_t> z = zlibCompress(raw, 9);
        const auto dataAt = static_cast<std::uint32_t>(o.size());
        o.insert(o.end(), z.begin(), z.end());
        if (o.size() % 2 != 0) o.push_back(0);
        const auto bpsAt = static_cast<std::uint32_t>(o.size());
        for (int k = 0; k < spp; ++k)
            putLe16(o, 8);
        const auto resAt = static_cast<std::uint32_t>(o.size());
        putLe32(o, 72);
        putLe32(o, 1);
        const auto ifdAt = static_cast<std::uint32_t>(o.size());
        setLe32(o, prevNext, ifdAt);
        struct Tag {
            std::uint16_t tag, type;
            std::uint32_t count, value;
        };
        std::vector<Tag> tags = {
            {256, 4, 1, static_cast<std::uint32_t>(g.w)},
            {257, 4, 1, static_cast<std::uint32_t>(g.h)},
            {258, 3, static_cast<std::uint32_t>(spp), bpsAt},
            {259, 3, 1, 8},  // Deflate
            {262, 3, 1, 2},  // RGB
            {273, 4, 1, dataAt},
            {277, 3, 1, static_cast<std::uint32_t>(spp)},
            {278, 4, 1, static_cast<std::uint32_t>(g.h)},
            {279, 4, 1, static_cast<std::uint32_t>(z.size())},
            {282, 5, 1, resAt},
            {283, 5, 1, resAt},
            {284, 3, 1, 1},
            {296, 3, 1, 2},  // дюймы
            {317, 3, 1, 2},  // горизонтальный предсказатель
        };
        if (alpha) tags.push_back({338, 3, 1, 2});  // прямая (неумноженная) альфа
        putLe16(o, static_cast<std::uint32_t>(tags.size()));
        for (const Tag& t : tags) {
            putLe16(o, t.tag);
            putLe16(o, t.type);
            putLe32(o, t.count);
            // Короткое значение лежит в самом поле — прижатым к его началу.
            if (t.type == 3 && t.count == 1) {
                putLe16(o, t.value);
                putLe16(o, 0);
            } else {
                putLe32(o, t.value);
            }
        }
        prevNext = o.size();
        putLe32(o, 0);
    }
    return o;
}

}  // namespace tjtm
