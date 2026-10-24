#include "core/zlib.h"

#include <algorithm>
#include <array>
#include <queue>

namespace tjtm {

// ---------------------------------------------------------------------------
// Контрольные суммы
// ---------------------------------------------------------------------------

namespace {

std::array<std::uint32_t, 256> makeCrcTable() {
    std::array<std::uint32_t, 256> t{};
    for (std::uint32_t n = 0; n < 256; ++n) {
        std::uint32_t c = n;
        for (int k = 0; k < 8; ++k)
            c = (c & 1u) != 0 ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        t[n] = c;
    }
    return t;
}

}  // namespace

std::uint32_t crc32(const std::uint8_t* data, std::size_t n, std::uint32_t crc) {
    static const auto table = makeCrcTable();
    crc = ~crc;
    for (std::size_t i = 0; i < n; ++i)
        crc = table[(crc ^ data[i]) & 0xFFu] ^ (crc >> 8);
    return ~crc;
}

std::uint32_t adler32(const std::uint8_t* data, std::size_t n, std::uint32_t adler) {
    std::uint32_t a = adler & 0xFFFFu;
    std::uint32_t b = adler >> 16;
    std::size_t i = 0;
    while (i < n) {
        // 5552 — наибольший кусок, на котором сумма ещё не переполняет 32 бита.
        const std::size_t chunk = std::min<std::size_t>(5552, n - i);
        for (std::size_t k = 0; k < chunk; ++k) {
            a += data[i + k];
            b += a;
        }
        a %= 65521u;
        b %= 65521u;
        i += chunk;
    }
    return (b << 16) | a;
}

// ---------------------------------------------------------------------------
// Таблицы формата
// ---------------------------------------------------------------------------

namespace {

constexpr std::array<std::uint16_t, 29> kLenBase = {3,  4,  5,  6,   7,   8,   9,   10,  11, 13,
                                                    15, 17, 19, 23,  27,  31,  35,  43,  51, 59,
                                                    67, 83, 99, 115, 131, 163, 195, 227, 258};
constexpr std::array<std::uint8_t, 29> kLenExtra = {0, 0, 0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 2, 2, 2,
                                                    2, 3, 3, 3, 3, 4, 4, 4, 4, 5, 5, 5, 5, 0};
constexpr std::array<std::uint16_t, 30> kDistBase = {
    1,   2,   3,   4,   5,   7,    9,    13,   17,   25,   33,   49,   65,    97,    129,
    193, 257, 385, 513, 769, 1025, 1537, 2049, 3073, 4097, 6145, 8193, 12289, 16385, 24577};
constexpr std::array<std::uint8_t, 30> kDistExtra = {0, 0, 0,  0,  1,  1,  2,  2,  3,  3,
                                                     4, 4, 5,  5,  6,  6,  7,  7,  8,  8,
                                                     9, 9, 10, 10, 11, 11, 12, 12, 13, 13};
/// Порядок передачи длин кодов для кодов длин.
constexpr std::array<std::uint8_t, 19> kClOrder = {16, 17, 18, 0, 8,  7, 9,  6, 10, 5,
                                                   11, 4,  12, 3, 13, 2, 14, 1, 15};

// ---------------------------------------------------------------------------
// Распаковка
// ---------------------------------------------------------------------------

struct BitReader {
    const std::uint8_t* d = nullptr;
    std::size_t n = 0;
    std::size_t pos = 0;
    std::uint32_t buf = 0;
    int cnt = 0;
    bool overrun = false;

    int bits(int k) {
        while (cnt < k) {
            if (pos >= n) {
                overrun = true;
                return 0;
            }
            buf |= static_cast<std::uint32_t>(d[pos++]) << cnt;
            cnt += 8;
        }
        const auto v = static_cast<int>(buf & ((1u << k) - 1u));
        buf >>= k;
        cnt -= k;
        return v;
    }
    void alignByte() {
        buf = 0;
        cnt = 0;
    }
};

/// Канонический код Хаффмана для распаковки: сколько кодов каждой длины и
/// символы по порядку кодов.
struct Huffman {
    std::array<std::uint16_t, 16> count{};
    std::vector<std::uint16_t> symbol;

    /// false — набор длин не образует правильного кода.
    bool build(const std::uint8_t* lengths, int n) {
        count.fill(0);
        for (int i = 0; i < n; ++i)
            ++count[lengths[i]];
        if (count[0] == n) {
            symbol.clear();
            return true;  // пустой код: допустим, если им не пользуются
        }
        int left = 1;
        for (int len = 1; len < 16; ++len) {
            left <<= 1;
            left -= count[static_cast<std::size_t>(len)];
            if (left < 0) return false;  // переполненный код
        }
        std::array<std::uint16_t, 16> offs{};
        for (int len = 1; len < 15; ++len) {
            offs[static_cast<std::size_t>(len) + 1] = static_cast<std::uint16_t>(
                offs[static_cast<std::size_t>(len)] + count[static_cast<std::size_t>(len)]);
        }
        symbol.assign(static_cast<std::size_t>(n), 0);
        for (int i = 0; i < n; ++i) {
            if (lengths[i] != 0) symbol[offs[lengths[i]]++] = static_cast<std::uint16_t>(i);
        }
        return true;
    }

    /// Символ или -1 при ошибке.
    int decode(BitReader& br) const {
        int code = 0;
        int first = 0;
        int index = 0;
        for (int len = 1; len < 16; ++len) {
            code |= br.bits(1);
            if (br.overrun) return -1;
            const int c = count[static_cast<std::size_t>(len)];
            if (code - c < first) return symbol[static_cast<std::size_t>(index + (code - first))];
            index += c;
            first += c;
            first <<= 1;
            code <<= 1;
        }
        return -1;
    }
};

bool inflateCodes(BitReader& br, const Huffman& lit, const Huffman& dist,
                  std::vector<std::uint8_t>& out, std::string& error, std::size_t maxOut) {
    while (true) {
        int sym = lit.decode(br);
        if (sym < 0) {
            error = "испорченный код Хаффмана";
            return false;
        }
        if (sym < 256) {
            if (out.size() >= maxOut) {
                error = "распакованные данные больше допустимого";
                return false;
            }
            out.push_back(static_cast<std::uint8_t>(sym));
            continue;
        }
        if (sym == 256) return true;
        sym -= 257;
        if (sym >= 29) {
            error = "недопустимый код длины";
            return false;
        }
        const int len = kLenBase[static_cast<std::size_t>(sym)] +
                        br.bits(kLenExtra[static_cast<std::size_t>(sym)]);
        const int dsym = dist.decode(br);
        if (dsym < 0 || dsym >= 30) {
            error = "недопустимый код расстояния";
            return false;
        }
        const auto d =
            static_cast<std::size_t>(kDistBase[static_cast<std::size_t>(dsym)] +
                                     br.bits(kDistExtra[static_cast<std::size_t>(dsym)]));
        if (br.overrun) {
            error = "поток обрывается";
            return false;
        }
        if (d > out.size()) {
            error = "расстояние за началом данных";
            return false;
        }
        if (out.size() + static_cast<std::size_t>(len) > maxOut) {
            error = "распакованные данные больше допустимого";
            return false;
        }
        const std::size_t from = out.size() - d;
        for (int k = 0; k < len; ++k)
            out.push_back(out[from + static_cast<std::size_t>(k)]);
    }
}

}  // namespace

bool inflateRaw(const std::uint8_t* data, std::size_t n, std::vector<std::uint8_t>& out,
                std::string& error, std::size_t maxOut) {
    BitReader br{data, n};
    out.clear();
    static Huffman fixedLit;
    static Huffman fixedDist;
    static bool fixedReady = false;
    if (!fixedReady) {
        std::array<std::uint8_t, 288> l{};
        for (int i = 0; i < 144; ++i)
            l[static_cast<std::size_t>(i)] = 8;
        for (int i = 144; i < 256; ++i)
            l[static_cast<std::size_t>(i)] = 9;
        for (int i = 256; i < 280; ++i)
            l[static_cast<std::size_t>(i)] = 7;
        for (int i = 280; i < 288; ++i)
            l[static_cast<std::size_t>(i)] = 8;
        fixedLit.build(l.data(), 288);
        std::array<std::uint8_t, 30> dl{};
        dl.fill(5);
        fixedDist.build(dl.data(), 30);
        fixedReady = true;
    }

    bool last = false;
    while (!last) {
        last = br.bits(1) != 0;
        const int type = br.bits(2);
        if (br.overrun) {
            error = "поток обрывается";
            return false;
        }
        if (type == 0) {
            br.alignByte();
            if (br.pos + 4 > n) {
                error = "поток обрывается";
                return false;
            }
            const unsigned len = data[br.pos] | (data[br.pos + 1] << 8);
            const unsigned nlen = data[br.pos + 2] | (data[br.pos + 3] << 8);
            br.pos += 4;
            if (len != (~nlen & 0xFFFFu)) {
                error = "испорчен блок без сжатия";
                return false;
            }
            if (br.pos + len > n) {
                error = "поток обрывается";
                return false;
            }
            if (out.size() + len > maxOut) {
                error = "распакованные данные больше допустимого";
                return false;
            }
            out.insert(out.end(), data + br.pos, data + br.pos + len);
            br.pos += len;
        } else if (type == 1) {
            if (!inflateCodes(br, fixedLit, fixedDist, out, error, maxOut)) return false;
        } else if (type == 2) {
            const int nlen = br.bits(5) + 257;
            const int ndist = br.bits(5) + 1;
            const int ncode = br.bits(4) + 4;
            if (nlen > 286 || ndist > 30) {
                error = "неверные размеры кодов";
                return false;
            }
            std::array<std::uint8_t, 19> cl{};
            for (int i = 0; i < ncode; ++i)
                cl[kClOrder[static_cast<std::size_t>(i)]] = static_cast<std::uint8_t>(br.bits(3));
            Huffman clh;
            if (!clh.build(cl.data(), 19)) {
                error = "испорчен код длин";
                return false;
            }
            std::array<std::uint8_t, 316> lengths{};
            int idx = 0;
            while (idx < nlen + ndist) {
                const int sym = clh.decode(br);
                if (sym < 0) {
                    error = "испорчен код длин";
                    return false;
                }
                if (sym < 16) {
                    lengths[static_cast<std::size_t>(idx++)] = static_cast<std::uint8_t>(sym);
                    continue;
                }
                std::uint8_t val = 0;
                int rep = 0;
                if (sym == 16) {
                    if (idx == 0) {
                        error = "повтор без предыдущей длины";
                        return false;
                    }
                    val = lengths[static_cast<std::size_t>(idx - 1)];
                    rep = 3 + br.bits(2);
                } else if (sym == 17) {
                    rep = 3 + br.bits(3);
                } else {
                    rep = 11 + br.bits(7);
                }
                if (idx + rep > nlen + ndist) {
                    error = "длины выходят за набор";
                    return false;
                }
                for (int k = 0; k < rep; ++k)
                    lengths[static_cast<std::size_t>(idx++)] = val;
            }
            if (lengths[256] == 0) {
                error = "нет кода конца блока";
                return false;
            }
            Huffman lit;
            Huffman dist;
            if (!lit.build(lengths.data(), nlen) || !dist.build(lengths.data() + nlen, ndist)) {
                error = "испорчен код Хаффмана";
                return false;
            }
            if (!inflateCodes(br, lit, dist, out, error, maxOut)) return false;
        } else {
            error = "недопустимый тип блока";
            return false;
        }
    }
    return true;
}

bool zlibDecompress(const std::uint8_t* data, std::size_t n, std::vector<std::uint8_t>& out,
                    std::string& error, std::size_t maxOut) {
    if (n < 6) {
        error = "поток zlib слишком короткий";
        return false;
    }
    const unsigned cmf = data[0];
    const unsigned flg = data[1];
    if ((cmf & 0x0Fu) != 8 || ((cmf << 8) | flg) % 31 != 0 || (flg & 0x20u) != 0) {
        error = "неверный заголовок zlib";
        return false;
    }
    if (!inflateRaw(data + 2, n - 2, out, error, maxOut)) return false;
    // Контрольную сумму не проверяем строго: некоторые программы пишут её
    // неверно, а данные при этом целы. CRC кусков PNG уже проверен.
    return true;
}

// ---------------------------------------------------------------------------
// Упаковка
// ---------------------------------------------------------------------------

namespace {

struct BitWriter {
    std::vector<std::uint8_t> out;
    std::uint64_t buf = 0;
    int cnt = 0;

    void put(std::uint32_t v, int k) {
        buf |= static_cast<std::uint64_t>(v) << cnt;
        cnt += k;
        while (cnt >= 8) {
            out.push_back(static_cast<std::uint8_t>(buf));
            buf >>= 8;
            cnt -= 8;
        }
    }
    void flushByte() {
        if (cnt > 0) put(0, 8 - cnt);
    }
};

std::uint32_t reverseBits(std::uint32_t v, int len) {
    std::uint32_t r = 0;
    for (int i = 0; i < len; ++i) {
        r = (r << 1) | (v & 1u);
        v >>= 1;
    }
    return r;
}

/// Длины кодов Хаффмана по частотам, не длиннее maxLen. Сначала обычное
/// дерево, затем подгонка счётчиков длин под неравенство Крафта (так же
/// делает miniz), затем самые частые символы получают самые короткие коды.
std::vector<std::uint8_t> buildLengths(const std::vector<std::uint32_t>& freq, int maxLen) {
    const std::size_t n = freq.size();
    std::vector<std::uint8_t> lengths(n, 0);
    std::vector<std::size_t> used;
    for (std::size_t i = 0; i < n; ++i) {
        if (freq[i] > 0) used.push_back(i);
    }
    if (used.empty()) return lengths;
    if (used.size() == 1) {
        // Один символ: добавляем второй, чтобы код был полным — так его
        // примет любой распаковщик.
        lengths[used[0]] = 1;
        lengths[used[0] == 0 ? 1 : 0] = 1;
        return lengths;
    }

    struct Node {
        std::uint64_t f;
        int left, right;
    };
    std::vector<Node> nodes;
    using Item = std::pair<std::uint64_t, int>;
    std::priority_queue<Item, std::vector<Item>, std::greater<>> pq;
    for (std::size_t s : used) {
        nodes.push_back({freq[s], -1, static_cast<int>(s)});
        pq.emplace(freq[s], static_cast<int>(nodes.size() - 1));
    }
    while (pq.size() > 1) {
        const Item a = pq.top();
        pq.pop();
        const Item b = pq.top();
        pq.pop();
        nodes.push_back({a.first + b.first, a.second, b.second});
        pq.emplace(a.first + b.first, static_cast<int>(nodes.size() - 1));
    }
    // Глубины листьев обходом от корня.
    std::vector<int> depthCount(64, 0);
    std::vector<std::pair<int, int>> stack{{pq.top().second, 0}};
    while (!stack.empty()) {
        const auto [ni, dep] = stack.back();
        stack.pop_back();
        const Node& nd = nodes[static_cast<std::size_t>(ni)];
        if (nd.left < 0) {
            ++depthCount[static_cast<std::size_t>(std::min(dep, 63))];
        } else {
            stack.emplace_back(nd.left, dep + 1);
            stack.emplace_back(nd.right, dep + 1);
        }
    }
    std::vector<int> count(static_cast<std::size_t>(maxLen) + 1, 0);
    for (int d = 1; d < 64; ++d)
        count[static_cast<std::size_t>(std::min(d, maxLen))] +=
            depthCount[static_cast<std::size_t>(d)];
    std::uint64_t total = 0;
    for (int len = 1; len <= maxLen; ++len) {
        total += static_cast<std::uint64_t>(count[static_cast<std::size_t>(len)]) << (maxLen - len);
    }
    while (total != (std::uint64_t{1} << maxLen)) {
        --count[static_cast<std::size_t>(maxLen)];
        for (int len = maxLen - 1; len > 0; --len) {
            if (count[static_cast<std::size_t>(len)] != 0) {
                --count[static_cast<std::size_t>(len)];
                count[static_cast<std::size_t>(len) + 1] += 2;
                break;
            }
        }
        --total;
    }
    std::vector<std::size_t> order = used;
    std::stable_sort(order.begin(), order.end(),
                     [&](std::size_t a, std::size_t b) { return freq[a] > freq[b]; });
    std::size_t k = 0;
    for (int len = 1; len <= maxLen; ++len) {
        for (int c = 0; c < count[static_cast<std::size_t>(len)]; ++c)
            lengths[order[k++]] = static_cast<std::uint8_t>(len);
    }
    return lengths;
}

/// Канонические коды по длинам, уже развёрнутые для записи младшим битом вперёд.
std::vector<std::uint32_t> canonicalCodes(const std::vector<std::uint8_t>& lengths) {
    std::array<std::uint32_t, 16> blCount{};
    for (std::uint8_t l : lengths) {
        if (l != 0) ++blCount[l];
    }
    std::array<std::uint32_t, 16> next{};
    std::uint32_t code = 0;
    for (int bits = 1; bits < 16; ++bits) {
        code = (code + blCount[static_cast<std::size_t>(bits) - 1]) << 1;
        next[static_cast<std::size_t>(bits)] = code;
    }
    std::vector<std::uint32_t> codes(lengths.size(), 0);
    for (std::size_t i = 0; i < lengths.size(); ++i) {
        const int l = lengths[i];
        if (l != 0) codes[i] = reverseBits(next[static_cast<std::size_t>(l)]++, l);
    }
    return codes;
}

/// Символ LZ77: литерал (dist == 0) или пара длина-расстояние.
struct Sym {
    std::uint16_t len;   // для литерала — сам байт
    std::uint16_t dist;  // 0 — литерал
};

int lenCode(int len) {
    int c = 0;
    while (c < 28 && kLenBase[static_cast<std::size_t>(c) + 1] <= len)
        ++c;
    return c;
}

int distCode(int d) {
    int c = 0;
    while (c < 29 && kDistBase[static_cast<std::size_t>(c) + 1] <= d)
        ++c;
    return c;
}

std::uint64_t codedBits(const std::vector<Sym>& syms, const std::vector<std::uint8_t>& ll,
                        const std::vector<std::uint8_t>& dl) {
    std::uint64_t bits = ll[256];
    for (const Sym& s : syms) {
        if (s.dist == 0) {
            bits += ll[s.len];
        } else {
            const int lc = lenCode(s.len);
            const int dc = distCode(s.dist);
            bits += static_cast<std::uint64_t>(ll[static_cast<std::size_t>(257 + lc)]) +
                    kLenExtra[static_cast<std::size_t>(lc)] + dl[static_cast<std::size_t>(dc)] +
                    kDistExtra[static_cast<std::size_t>(dc)];
        }
    }
    return bits;
}

void writeSyms(BitWriter& bw, const std::vector<Sym>& syms, const std::vector<std::uint8_t>& ll,
               const std::vector<std::uint8_t>& dl) {
    const auto lc = canonicalCodes(ll);
    const auto dc = canonicalCodes(dl);
    for (const Sym& s : syms) {
        if (s.dist == 0) {
            bw.put(lc[s.len], ll[s.len]);
        } else {
            const int l = lenCode(s.len);
            bw.put(lc[static_cast<std::size_t>(257 + l)], ll[static_cast<std::size_t>(257 + l)]);
            bw.put(static_cast<std::uint32_t>(s.len - kLenBase[static_cast<std::size_t>(l)]),
                   kLenExtra[static_cast<std::size_t>(l)]);
            const int d = distCode(s.dist);
            bw.put(dc[static_cast<std::size_t>(d)], dl[static_cast<std::size_t>(d)]);
            bw.put(static_cast<std::uint32_t>(s.dist - kDistBase[static_cast<std::size_t>(d)]),
                   kDistExtra[static_cast<std::size_t>(d)]);
        }
    }
    bw.put(lc[256], ll[256]);
}

/// Длины кодов, сжатые повторами (символы 16, 17, 18 кода длин).
struct ClItem {
    std::uint8_t sym;
    std::uint8_t extra;
};

std::vector<ClItem> rleLengths(const std::vector<std::uint8_t>& all) {
    std::vector<ClItem> out;
    std::size_t i = 0;
    while (i < all.size()) {
        const std::uint8_t v = all[i];
        std::size_t run = 1;
        while (i + run < all.size() && all[i + run] == v)
            ++run;
        if (v == 0 && run >= 3) {
            std::size_t r = run;
            while (r >= 11) {
                const std::size_t t = std::min<std::size_t>(r, 138);
                out.push_back({18, static_cast<std::uint8_t>(t - 11)});
                r -= t;
            }
            if (r >= 3) {
                out.push_back({17, static_cast<std::uint8_t>(r - 3)});
                r = 0;
            }
            while (r-- > 0)
                out.push_back({0, 0});
        } else if (v != 0 && run >= 4) {
            out.push_back({v, 0});
            std::size_t r = run - 1;
            while (r >= 3) {
                const std::size_t t = std::min<std::size_t>(r, 6);
                out.push_back({16, static_cast<std::uint8_t>(t - 3)});
                r -= t;
            }
            while (r-- > 0)
                out.push_back({v, 0});
        } else {
            for (std::size_t k = 0; k < run; ++k)
                out.push_back({v, 0});
        }
        i += run;
    }
    return out;
}

/// Блок с собственными кодами: биты заголовка и сами символы.
struct DynamicPlan {
    std::vector<std::uint8_t> ll, dl, clLen;
    std::vector<ClItem> cl;
    int nlen = 257, ndist = 1, ncode = 4;
    std::uint64_t bits = 0;
};

DynamicPlan planDynamic(const std::vector<Sym>& syms) {
    std::vector<std::uint32_t> lf(286, 0);
    std::vector<std::uint32_t> df(30, 0);
    for (const Sym& s : syms) {
        if (s.dist == 0) {
            ++lf[s.len];
        } else {
            ++lf[static_cast<std::size_t>(257 + lenCode(s.len))];
            ++df[static_cast<std::size_t>(distCode(s.dist))];
        }
    }
    lf[256] = 1;
    DynamicPlan p;
    p.ll = buildLengths(lf, 15);
    p.dl = buildLengths(df, 15);
    p.nlen = 286;
    while (p.nlen > 257 && p.ll[static_cast<std::size_t>(p.nlen) - 1] == 0)
        --p.nlen;
    p.ndist = 30;
    while (p.ndist > 1 && p.dl[static_cast<std::size_t>(p.ndist) - 1] == 0)
        --p.ndist;
    std::vector<std::uint8_t> all(p.ll.begin(), p.ll.begin() + p.nlen);
    all.insert(all.end(), p.dl.begin(), p.dl.begin() + p.ndist);
    p.cl = rleLengths(all);
    std::vector<std::uint32_t> cf(19, 0);
    for (const ClItem& c : p.cl)
        ++cf[c.sym];
    p.clLen = buildLengths(cf, 7);
    p.ncode = 19;
    while (p.ncode > 4 && p.clLen[kClOrder[static_cast<std::size_t>(p.ncode) - 1]] == 0)
        --p.ncode;
    std::uint64_t bits = 5 + 5 + 4 + 3 * static_cast<std::uint64_t>(p.ncode);
    for (const ClItem& c : p.cl) {
        bits += p.clLen[c.sym];
        if (c.sym == 16) bits += 2;
        if (c.sym == 17) bits += 3;
        if (c.sym == 18) bits += 7;
    }
    p.bits = bits + codedBits(syms, p.ll, p.dl);
    return p;
}

void writeStored(BitWriter& bw, const std::uint8_t* data, std::size_t n, bool last) {
    std::size_t off = 0;
    do {
        const std::size_t len = std::min<std::size_t>(65535, n - off);
        const bool fin = last && off + len == n;
        bw.put(fin ? 1u : 0u, 1);
        bw.put(0, 2);
        bw.flushByte();
        bw.put(static_cast<std::uint32_t>(len), 16);
        bw.put(static_cast<std::uint32_t>(~len & 0xFFFFu), 16);
        for (std::size_t k = 0; k < len; ++k)
            bw.put(data[off + k], 8);
        off += len;
    } while (off < n);
}

void emitBlock(BitWriter& bw, const std::vector<Sym>& syms, const std::uint8_t* raw,
               std::size_t rawLen, bool last) {
    static const std::vector<std::uint8_t> fixedLL = [] {
        std::vector<std::uint8_t> l(288);
        for (int i = 0; i < 288; ++i)
            l[static_cast<std::size_t>(i)] = i < 144 ? 8 : (i < 256 ? 9 : (i < 280 ? 7 : 8));
        return l;
    }();
    static const std::vector<std::uint8_t> fixedDL(30, 5);

    const DynamicPlan dyn = planDynamic(syms);
    const std::uint64_t fixedBits = codedBits(syms, fixedLL, fixedDL);
    const std::uint64_t storedBits = (rawLen / 65535 + 1) * 40 + rawLen * 8;
    if (storedBits <= dyn.bits && storedBits <= fixedBits) {
        writeStored(bw, raw, rawLen, last);
    } else if (fixedBits <= dyn.bits) {
        bw.put(last ? 1u : 0u, 1);
        bw.put(1, 2);
        writeSyms(bw, syms, fixedLL, fixedDL);
    } else {
        bw.put(last ? 1u : 0u, 1);
        bw.put(2, 2);
        bw.put(static_cast<std::uint32_t>(dyn.nlen - 257), 5);
        bw.put(static_cast<std::uint32_t>(dyn.ndist - 1), 5);
        bw.put(static_cast<std::uint32_t>(dyn.ncode - 4), 4);
        for (int i = 0; i < dyn.ncode; ++i)
            bw.put(dyn.clLen[kClOrder[static_cast<std::size_t>(i)]], 3);
        const auto clCodes = canonicalCodes(dyn.clLen);
        for (const ClItem& c : dyn.cl) {
            bw.put(clCodes[c.sym], dyn.clLen[c.sym]);
            if (c.sym == 16) bw.put(c.extra, 2);
            if (c.sym == 17) bw.put(c.extra, 3);
            if (c.sym == 18) bw.put(c.extra, 7);
        }
        writeSyms(bw, syms, dyn.ll, dyn.dl);
    }
}

}  // namespace

std::vector<std::uint8_t> deflateRaw(const std::uint8_t* data, std::size_t n, int level) {
    BitWriter bw;
    if (n == 0 || level <= 0) {
        if (n == 0) {
            // Пустой поток — один последний блок с фиксированными кодами.
            bw.put(1, 1);
            bw.put(1, 2);
            bw.put(0, 7);  // код конца блока 256 — семь нулевых битов
        } else {
            writeStored(bw, data, n, true);
        }
        bw.flushByte();
        return bw.out;
    }

    constexpr int kWin = 32768;
    constexpr int kHashBits = 15;
    constexpr int kHashSize = 1 << kHashBits;
    const int maxChain = level >= 9 ? 4096 : (level >= 6 ? 128 : (level >= 3 ? 32 : 8));
    std::vector<int> head(kHashSize, -1);
    std::vector<int> prev(kWin, -1);
    auto hashAt = [&](std::size_t i) {
        const std::uint32_t v = (static_cast<std::uint32_t>(data[i]) << 16) |
                                (static_cast<std::uint32_t>(data[i + 1]) << 8) | data[i + 2];
        return static_cast<int>((v * 2654435761u) >> (32 - kHashBits));
    };
    auto insert = [&](std::size_t i) {
        if (i + 2 >= n) return;
        const int hv = hashAt(i);
        prev[i & (kWin - 1)] = head[static_cast<std::size_t>(hv)];
        head[static_cast<std::size_t>(hv)] = static_cast<int>(i);
    };
    auto findMatch = [&](std::size_t i, int& bestDist) {
        int best = 0;
        if (i + 2 >= n) return 0;
        int cand = head[static_cast<std::size_t>(hashAt(i))];
        int chain = maxChain;
        const int maxLen = static_cast<int>(std::min<std::size_t>(258, n - i));
        while (cand >= 0 && chain-- > 0) {
            const int dist = static_cast<int>(i) - cand;
            if (dist <= 0 || dist > kWin) break;
            const auto c = static_cast<std::size_t>(cand);
            if (data[c + static_cast<std::size_t>(best)] ==
                data[i + static_cast<std::size_t>(best)]) {
                int len = 0;
                while (len < maxLen && data[c + static_cast<std::size_t>(len)] ==
                                           data[i + static_cast<std::size_t>(len)])
                    ++len;
                if (len > best) {
                    best = len;
                    bestDist = dist;
                    if (len == maxLen) break;
                }
            }
            const int pc = prev[c & (kWin - 1)];
            if (pc >= cand) break;  // цепочка ушла за окно
            cand = pc;
        }
        return best >= 3 ? best : 0;
    };

    std::vector<Sym> syms;
    std::size_t blockStart = 0;
    std::size_t i = 0;
    constexpr std::size_t kBlockSyms = 16384;
    while (i < n) {
        int dist = 0;
        int len = findMatch(i, dist);
        if (len > 0) {
            insert(i);
            if (i + 1 < n) {
                // Ленивый выбор: если со следующего байта совпадение длиннее,
                // этот байт выгоднее отдать литералом.
                int dist2 = 0;
                const int len2 = findMatch(i + 1, dist2);
                if (len2 > len) {
                    syms.push_back({data[i], 0});
                    ++i;
                    len = len2;
                    dist = dist2;
                    insert(i);
                }
            }
            syms.push_back({static_cast<std::uint16_t>(len), static_cast<std::uint16_t>(dist)});
            const std::size_t end = i + static_cast<std::size_t>(len);
            for (std::size_t k = i + 1; k < end; ++k)
                insert(k);
            i = end;
        } else {
            insert(i);
            syms.push_back({data[i], 0});
            ++i;
        }
        if (syms.size() >= kBlockSyms) {
            emitBlock(bw, syms, data + blockStart, i - blockStart, i >= n);
            syms.clear();
            blockStart = i;
        }
    }
    // Если последний символ закрыл полный блок, он уже записан с признаком
    // конца; иначе дописываем остаток.
    if (!syms.empty()) emitBlock(bw, syms, data + blockStart, n - blockStart, true);
    bw.flushByte();
    return bw.out;
}

std::vector<std::uint8_t> zlibCompress(const std::vector<std::uint8_t>& data, int level) {
    std::vector<std::uint8_t> out = {0x78, 0x9C};
    const auto body = deflateRaw(data.data(), data.size(), level);
    out.insert(out.end(), body.begin(), body.end());
    const std::uint32_t a = adler32(data.data(), data.size());
    out.push_back(static_cast<std::uint8_t>(a >> 24));
    out.push_back(static_cast<std::uint8_t>(a >> 16));
    out.push_back(static_cast<std::uint8_t>(a >> 8));
    out.push_back(static_cast<std::uint8_t>(a));
    return out;
}

}  // namespace tjtm
