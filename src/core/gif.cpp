#include "core/gif.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <unordered_map>

#include "core/palette.h"
#include "core/pmath.h"

namespace tjtm {

namespace {

constexpr int kMaxCodes = 4096;
constexpr int kMaxSide = 1 << 14;
constexpr std::size_t kMaxFrames = 4096;

void putLe16(std::vector<std::uint8_t>& o, int v) {
    o.push_back(static_cast<std::uint8_t>(v & 0xFF));
    o.push_back(static_cast<std::uint8_t>((v >> 8) & 0xFF));
}

/// Точка видна, если её альфа не меньше половины: у GIF прозрачность
/// бывает только полной.
bool visible(Color c) {
    return c.a >= 128;
}

std::uint32_t rgbKey(Color c) {
    return (static_cast<std::uint32_t>(c.r) << 16) | (static_cast<std::uint32_t>(c.g) << 8) | c.b;
}

/// Таблица цветов на все кадры. Возвращает цвета без прозрачного места.
std::vector<Color> buildTable(const std::vector<AnimFrame>& frames,
                              const std::vector<Color>& preferred, bool& hasClear) {
    hasClear = false;
    std::unordered_map<std::uint32_t, std::size_t> seen;
    std::vector<Color> used;
    for (const AnimFrame& f : frames) {
        for (const Color c : f.image.d) {
            if (!visible(c)) {
                hasClear = true;
                continue;
            }
            if (seen.emplace(rgbKey(c), used.size()).second) used.push_back(rgb(c.r, c.g, c.b));
        }
    }
    const int slots = hasClear ? 255 : 256;
    if (static_cast<int>(used.size()) > slots) {
        // Все кадры одной картинкой: медианный разрез видит цвета всей
        // анимации, а не одного кадра.
        int total = 0;
        for (const AnimFrame& f : frames)
            total += f.image.w * f.image.h;
        Grid<Color> all(std::max(1, total), 1, argb(0, 0, 0, 0));
        std::size_t k = 0;
        for (const AnimFrame& f : frames) {
            for (const Color c : f.image.d)
                all.d[k++] = visible(c) ? rgb(c.r, c.g, c.b) : argb(0, 0, 0, 0);
        }
        return pal::extract(all, slots);
    }
    // Сперва цвета палитры, которые есть в картинке, — в порядке палитры;
    // за ними прочие — в порядке появления.
    std::vector<Color> table;
    std::unordered_map<std::uint32_t, bool> placed;
    for (const Color p : preferred) {
        const std::uint32_t k = rgbKey(p);
        if (seen.count(k) != 0 && placed.emplace(k, true).second)
            table.push_back(rgb(p.r, p.g, p.b));
    }
    for (const Color c : used) {
        if (placed.emplace(rgbKey(c), true).second) table.push_back(c);
    }
    return table;
}

class BitWriter {
public:
    explicit BitWriter(std::vector<std::uint8_t>& out) : out_(out) {}
    void put(int code, int size) {
        acc_ |= static_cast<std::uint32_t>(code) << bits_;
        bits_ += size;
        while (bits_ >= 8) {
            out_.push_back(static_cast<std::uint8_t>(acc_ & 0xFF));
            acc_ >>= 8;
            bits_ -= 8;
        }
    }
    void flush() {
        if (bits_ > 0) out_.push_back(static_cast<std::uint8_t>(acc_ & 0xFF));
        acc_ = 0;
        bits_ = 0;
    }

private:
    std::vector<std::uint8_t>& out_;
    std::uint32_t acc_ = 0;
    int bits_ = 0;
};

/// Поток байтов из подблоков GIF (длина, данные…, 0).
bool readSubBlocks(const std::vector<std::uint8_t>& f, std::size_t& pos,
                   std::vector<std::uint8_t>* out) {
    while (true) {
        if (pos >= f.size()) return false;
        const std::size_t n = f[pos++];
        if (n == 0) return true;
        if (pos + n > f.size()) return false;
        if (out != nullptr)
            out->insert(out->end(), f.begin() + static_cast<std::ptrdiff_t>(pos),
                        f.begin() + static_cast<std::ptrdiff_t>(pos + n));
        pos += n;
    }
}

int le16(const std::vector<std::uint8_t>& f, std::size_t p) {
    return f[p] | (f[p + 1] << 8);
}

}  // namespace

bool isGif(const std::vector<std::uint8_t>& file) {
    return file.size() >= 6 && std::memcmp(file.data(), "GIF8", 4) == 0 &&
           (file[4] == '7' || file[4] == '9') && file[5] == 'a';
}

std::vector<std::uint8_t> gifLzwEncode(const std::vector<std::uint8_t>& indices, int minCodeSize) {
    std::vector<std::uint8_t> out;
    BitWriter bw(out);
    const int clear = 1 << minCodeSize;
    const int eoi = clear + 1;
    int next = eoi + 1;
    int size = minCodeSize + 1;
    // Словарь «код префикса + байт → код»: строки растут по одному байту.
    std::unordered_map<std::uint32_t, int> dict;
    dict.reserve(static_cast<std::size_t>(kMaxCodes) * 2);
    bw.put(clear, size);
    if (indices.empty()) {
        bw.put(eoi, size);
        bw.flush();
        return out;
    }
    int prefix = indices[0];
    for (std::size_t i = 1; i < indices.size(); ++i) {
        const int k = indices[i];
        const std::uint32_t key =
            (static_cast<std::uint32_t>(prefix) << 8) | static_cast<std::uint32_t>(k);
        const auto it = dict.find(key);
        if (it != dict.end()) {
            prefix = it->second;
            continue;
        }
        bw.put(prefix, size);
        if (next < kMaxCodes) {
            dict.emplace(key, next++);
            // Читающий узнаёт о новом коде на шаг позже пишущего, поэтому
            // ширина растёт, когда код next - 1 перестал помещаться.
            if (next > (1 << size) && size < 12) ++size;
        } else {
            // Словарь полон: сброс, и сжатие начинается заново — так поток
            // подстраивается под новые участки картинки.
            bw.put(clear, size);
            dict.clear();
            next = eoi + 1;
            size = minCodeSize + 1;
        }
        prefix = k;
    }
    bw.put(prefix, size);
    bw.put(eoi, size);
    bw.flush();
    return out;
}

bool gifLzwDecode(const std::vector<std::uint8_t>& data, int minCodeSize, std::size_t expected,
                  std::vector<std::uint8_t>& out) {
    out.clear();
    if (minCodeSize < 1 || minCodeSize > 11) return false;
    out.reserve(expected);
    const int clear = 1 << minCodeSize;
    const int eoi = clear + 1;
    int next = eoi + 1;
    int size = minCodeSize + 1;
    std::array<std::uint16_t, kMaxCodes> prefix{};
    std::array<std::uint8_t, kMaxCodes> suffix{};
    std::array<std::uint8_t, kMaxCodes> first{};
    std::array<std::uint8_t, kMaxCodes + 1> stack{};
    for (int c = 0; c < clear; ++c) {
        suffix[static_cast<std::size_t>(c)] = static_cast<std::uint8_t>(c);
        first[static_cast<std::size_t>(c)] = static_cast<std::uint8_t>(c);
    }
    int prev = -1;
    std::uint32_t acc = 0;
    int bits = 0;
    std::size_t pos = 0;
    while (out.size() < expected) {
        while (bits < size && pos < data.size()) {
            acc |= static_cast<std::uint32_t>(data[pos++]) << bits;
            bits += 8;
        }
        if (bits < size) break;  // поток кончился раньше картинки
        const int code = static_cast<int>(acc & ((1U << size) - 1));
        acc >>= size;
        bits -= size;
        if (code == clear) {
            next = eoi + 1;
            size = minCodeSize + 1;
            prev = -1;
            continue;
        }
        if (code == eoi) break;
        if (prev < 0) {
            if (code >= clear) return false;
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
            // Строка, которой ещё нет в словаре: прежняя плюс её же первый
            // байт (случай KwKwK).
            lead = first[static_cast<std::size_t>(prev)];
            stack[sp++] = lead;
            cur = prev;
        } else {
            return false;
        }
        while (cur >= clear) {
            stack[sp++] = suffix[static_cast<std::size_t>(cur)];
            cur = prefix[static_cast<std::size_t>(cur)];
        }
        stack[sp++] = static_cast<std::uint8_t>(cur);
        while (sp > 0 && out.size() < expected)
            out.push_back(stack[--sp]);
        if (next < kMaxCodes) {
            prefix[static_cast<std::size_t>(next)] = static_cast<std::uint16_t>(prev);
            suffix[static_cast<std::size_t>(next)] = lead;
            first[static_cast<std::size_t>(next)] = first[static_cast<std::size_t>(prev)];
            ++next;
            if (next == (1 << size) && size < 12) ++size;
        }
        prev = code;
    }
    return true;
}

std::vector<std::uint8_t> gifEncode(const std::vector<AnimFrame>& frames, int plays,
                                    const std::vector<Color>& preferred) {
    if (frames.empty() || frames.front().image.empty()) return {};
    const int w = frames.front().image.w;
    const int h = frames.front().image.h;
    bool hasClear = false;
    std::vector<Color> table = buildTable(frames, preferred, hasClear);
    if (table.empty()) table.push_back(rgb(0, 0, 0));
    const int clearIdx = hasClear ? static_cast<int>(table.size()) : -1;
    const int entries = static_cast<int>(table.size()) + (hasClear ? 1 : 0);
    int bitsPer = 1;
    while ((1 << bitsPer) < entries)
        ++bitsPer;

    std::vector<std::uint8_t> out = {'G', 'I', 'F', '8', '9', 'a'};
    putLe16(out, w);
    putLe16(out, h);
    out.push_back(static_cast<std::uint8_t>(0x80 | ((bitsPer - 1) << 4) | (bitsPer - 1)));
    out.push_back(0);  // фон
    out.push_back(0);  // соотношение сторон точки
    for (int k = 0; k < (1 << bitsPer); ++k) {
        const Color c =
            k < static_cast<int>(table.size()) ? table[static_cast<std::size_t>(k)] : rgb(0, 0, 0);
        out.push_back(c.r);
        out.push_back(c.g);
        out.push_back(c.b);
    }
    // Расширение NETSCAPE хранит число повторов после первого показа, а
    // без него анимация проигрывается один раз.
    if (frames.size() > 1 && plays != 1) {
        const std::uint8_t app[] = {0x21, 0xFF, 11,  'N', 'E', 'T', 'S', 'C',
                                    'A',  'P',  'E', '2', '.', '0', 3,   1};
        out.insert(out.end(), std::begin(app), std::end(app));
        putLe16(out, plays <= 0 ? 0 : std::min(plays - 1, 65535));
        out.push_back(0);
    }

    std::unordered_map<std::uint32_t, std::uint8_t> lookup;
    auto indexOf = [&](Color c) -> std::uint8_t {
        if (!visible(c)) return static_cast<std::uint8_t>(clearIdx);
        const std::uint32_t k = rgbKey(c);
        const auto it = lookup.find(k);
        if (it != lookup.end()) return it->second;
        const auto v = static_cast<std::uint8_t>(std::max(0, pal::nearest(table, c)));
        lookup.emplace(k, v);
        return v;
    };

    const int minCode = std::max(2, bitsPer);
    for (const AnimFrame& f : frames) {
        // Графическое расширение: задержка в сотых секунды, прозрачный номер.
        // Задержки короче 20 мс браузеры растягивают до 100 мс, поэтому
        // меньше двух сотых не пишется.
        const int cs = std::max(2, pround(f.durationMs / 10.0));
        out.push_back(0x21);
        out.push_back(0xF9);
        out.push_back(4);
        out.push_back(
            static_cast<std::uint8_t>((hasClear ? (2 << 2) : (1 << 2)) | (hasClear ? 1 : 0)));
        putLe16(out, std::min(cs, 65535));
        out.push_back(static_cast<std::uint8_t>(hasClear ? clearIdx : 0));
        out.push_back(0);

        out.push_back(0x2C);
        putLe16(out, 0);
        putLe16(out, 0);
        putLe16(out, w);
        putLe16(out, h);
        out.push_back(0);  // без своей таблицы, без чересстрочности

        std::vector<std::uint8_t> idx(static_cast<std::size_t>(w) * static_cast<std::size_t>(h),
                                      static_cast<std::uint8_t>(hasClear ? clearIdx : 0));
        for (int y = 0; y < std::min(h, f.image.h); ++y) {
            for (int x = 0; x < std::min(w, f.image.w); ++x)
                idx[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                    static_cast<std::size_t>(x)] = indexOf(f.image.at(x, y));
        }
        out.push_back(static_cast<std::uint8_t>(minCode));
        const std::vector<std::uint8_t> z = gifLzwEncode(idx, minCode);
        for (std::size_t p = 0; p < z.size(); p += 255) {
            const std::size_t n = std::min<std::size_t>(255, z.size() - p);
            out.push_back(static_cast<std::uint8_t>(n));
            out.insert(out.end(), z.begin() + static_cast<std::ptrdiff_t>(p),
                       z.begin() + static_cast<std::ptrdiff_t>(p + n));
        }
        out.push_back(0);
    }
    out.push_back(0x3B);
    return out;
}

bool gifDecode(const std::vector<std::uint8_t>& f, std::vector<AnimFrame>& frames,
               std::string& error, int* plays) {
    frames.clear();
    if (plays != nullptr) *plays = 1;
    if (!isGif(f) || f.size() < 13) {
        error = "это не GIF";
        return false;
    }
    const int cw = le16(f, 6);
    const int ch = le16(f, 8);
    if (cw < 1 || ch < 1 || cw > kMaxSide || ch > kMaxSide) {
        error = "недопустимый размер холста";
        return false;
    }
    std::size_t pos = 13;
    std::vector<Color> global;
    if ((f[10] & 0x80) != 0) {
        const std::size_t n = std::size_t(2) << (f[10] & 7);
        if (pos + n * 3 > f.size()) {
            error = "таблица цветов обрезана";
            return false;
        }
        for (std::size_t k = 0; k < n; ++k)
            global.push_back(rgb(f[pos + k * 3], f[pos + k * 3 + 1], f[pos + k * 3 + 2]));
        pos += n * 3;
    }

    Grid<Color> canvas(cw, ch, argb(0, 0, 0, 0));
    int delay = 0;
    int dispose = 0;
    int clearIdx = -1;
    while (pos < f.size()) {
        const std::uint8_t tag = f[pos++];
        if (tag == 0x3B) break;
        if (tag == 0x21) {
            if (pos >= f.size()) break;
            const std::uint8_t label = f[pos++];
            std::vector<std::uint8_t> body;
            if (!readSubBlocks(f, pos, &body)) break;
            if (label == 0xF9 && body.size() >= 4) {
                dispose = (body[0] >> 2) & 7;
                delay = body[1] | (body[2] << 8);
                clearIdx = (body[0] & 1) != 0 ? body[3] : -1;
            } else if (label == 0xFF && body.size() >= 14 &&
                       (std::memcmp(body.data(), "NETSCAPE2.0", 11) == 0 ||
                        std::memcmp(body.data(), "ANIMEXTS1.0", 11) == 0) &&
                       body[11] == 1) {
                const int repeats = body[12] | (body[13] << 8);
                if (plays != nullptr) *plays = repeats == 0 ? 0 : repeats + 1;
            }
            continue;
        }
        if (tag != 0x2C) break;  // мусор после картинок не мешает уже прочитанным
        if (pos + 9 > f.size()) break;
        const int ix = le16(f, pos);
        const int iy = le16(f, pos + 2);
        const int iw = le16(f, pos + 4);
        const int ih = le16(f, pos + 6);
        const std::uint8_t packed = f[pos + 8];
        pos += 9;
        std::vector<Color> local;
        if ((packed & 0x80) != 0) {
            const std::size_t n = std::size_t(2) << (packed & 7);
            if (pos + n * 3 > f.size()) break;
            for (std::size_t k = 0; k < n; ++k)
                local.push_back(rgb(f[pos + k * 3], f[pos + k * 3 + 1], f[pos + k * 3 + 2]));
            pos += n * 3;
        }
        const std::vector<Color>& table = local.empty() ? global : local;
        if (pos >= f.size()) break;
        const int minCode = f[pos++];
        std::vector<std::uint8_t> data;
        if (!readSubBlocks(f, pos, &data)) {
            if (frames.empty()) {
                error = "картинка обрезана";
                return false;
            }
            break;
        }
        const std::size_t count = static_cast<std::size_t>(iw) * static_cast<std::size_t>(ih);
        std::vector<std::uint8_t> idx;
        if (!gifLzwDecode(data, minCode, count, idx)) {
            error = "поток LZW испорчен";
            return false;
        }
        idx.resize(count, static_cast<std::uint8_t>(clearIdx < 0 ? 0 : clearIdx));

        // Чересстрочная картинка хранит строки четырьмя проходами.
        std::vector<int> rows;
        rows.reserve(static_cast<std::size_t>(ih));
        if ((packed & 0x40) != 0) {
            for (const auto& [start, step] : {std::pair{0, 8}, {4, 8}, {2, 4}, {1, 2}}) {
                for (int y = start; y < ih; y += step)
                    rows.push_back(y);
            }
        } else {
            for (int y = 0; y < ih; ++y)
                rows.push_back(y);
        }
        const Grid<Color> saved = dispose == 3 ? canvas : Grid<Color>();
        for (int r = 0; r < ih; ++r) {
            const int y = iy + rows[static_cast<std::size_t>(r)];
            for (int x = 0; x < iw; ++x) {
                const int v = idx[static_cast<std::size_t>(r) * static_cast<std::size_t>(iw) +
                                  static_cast<std::size_t>(x)];
                if (v == clearIdx || static_cast<std::size_t>(v) >= table.size()) continue;
                if (canvas.inside(ix + x, y))
                    canvas.at(ix + x, y) = table[static_cast<std::size_t>(v)];
            }
        }
        // Задержку 0 и 1 браузеры показывают как 100 мс — и здесь так же.
        frames.push_back(AnimFrame{canvas, delay < 2 ? 100 : delay * 10});
        if (frames.size() >= kMaxFrames) break;
        if (dispose == 2) {
            for (int y = iy; y < iy + ih; ++y) {
                for (int x = ix; x < ix + iw; ++x) {
                    if (canvas.inside(x, y)) canvas.at(x, y) = argb(0, 0, 0, 0);
                }
            }
        } else if (dispose == 3) {
            canvas = saved;
        }
        delay = 0;
        dispose = 0;
        clearIdx = -1;
    }
    if (frames.empty()) {
        error = "в файле нет картинок";
        return false;
    }
    return true;
}

}  // namespace tjtm
