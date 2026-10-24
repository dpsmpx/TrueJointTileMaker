#include "core/text.h"

#include <array>
#include <cstdint>

namespace tjtm {

std::u32string utf8ToU32(std::string_view s) {
    std::u32string out;
    out.reserve(s.size());
    std::size_t i = 0;
    const std::size_t n = s.size();
    while (i < n) {
        const auto c0 = static_cast<unsigned char>(s[i]);
        if (c0 < 0x80) {
            out.push_back(c0);
            ++i;
            continue;
        }
        int len = 0;
        char32_t cp = 0;
        char32_t minCp = 0;
        if ((c0 & 0xE0) == 0xC0) {
            len = 2;
            cp = c0 & 0x1Fu;
            minCp = 0x80;
        } else if ((c0 & 0xF0) == 0xE0) {
            len = 3;
            cp = c0 & 0x0Fu;
            minCp = 0x800;
        } else if ((c0 & 0xF8) == 0xF0) {
            len = 4;
            cp = c0 & 0x07u;
            minCp = 0x10000;
        } else {
            out.push_back(0xFFFD);
            ++i;
            continue;
        }
        if (i + static_cast<std::size_t>(len) > n) {
            out.push_back(0xFFFD);
            ++i;
            continue;
        }
        bool ok = true;
        for (int k = 1; k < len; ++k) {
            const auto ck = static_cast<unsigned char>(s[i + static_cast<std::size_t>(k)]);
            if ((ck & 0xC0) != 0x80) {
                ok = false;
                break;
            }
            cp = (cp << 6) | (ck & 0x3Fu);
        }
        if (!ok || cp < minCp || cp > 0x10FFFF || (cp >= 0xD800 && cp <= 0xDFFF)) {
            out.push_back(0xFFFD);
            ++i;
            continue;
        }
        out.push_back(cp);
        i += static_cast<std::size_t>(len);
    }
    return out;
}

void appendUtf8(std::string& out, char32_t cp) {
    if (cp < 0x80) {
        out.push_back(static_cast<char>(cp));
    } else if (cp < 0x800) {
        out.push_back(static_cast<char>(0xC0 | (cp >> 6)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        out.push_back(static_cast<char>(0xE0 | (cp >> 12)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    } else {
        out.push_back(static_cast<char>(0xF0 | (cp >> 18)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 12) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | ((cp >> 6) & 0x3F)));
        out.push_back(static_cast<char>(0x80 | (cp & 0x3F)));
    }
}

std::string u32ToUtf8(std::u32string_view s) {
    std::string out;
    out.reserve(s.size());
    for (char32_t c : s)
        appendUtf8(out, c);
    return out;
}

std::size_t utf16Length(std::string_view s) {
    std::size_t n = 0;
    for (char32_t c : utf8ToU32(s))
        n += (c >= 0x10000) ? 2 : 1;
    return n;
}

std::string cp1251ToUtf8(std::string_view s) {
    // Верхняя половина CP1251; нули там, где в кодировке дыра (0x98).
    static constexpr std::array<char16_t, 64> kHigh = {
        0x0402, 0x0403, 0x201A, 0x0453, 0x201E, 0x2026, 0x2020, 0x2021, 0x20AC, 0x2030, 0x0409,
        0x2039, 0x040A, 0x040C, 0x040B, 0x040F, 0x0452, 0x2018, 0x2019, 0x201C, 0x201D, 0x2022,
        0x2013, 0x2014, 0xFFFD, 0x2122, 0x0459, 0x203A, 0x045A, 0x045C, 0x045B, 0x045F, 0x00A0,
        0x040E, 0x045E, 0x0408, 0x00A4, 0x0490, 0x00A6, 0x00A7, 0x0401, 0x00A9, 0x0404, 0x00AB,
        0x00AC, 0x00AD, 0x00AE, 0x0407, 0x00B0, 0x00B1, 0x0406, 0x0456, 0x0491, 0x00B5, 0x00B6,
        0x00B7, 0x0451, 0x2116, 0x0454, 0x00BB, 0x0458, 0x0405, 0x0455, 0x0457,
    };
    std::string out;
    out.reserve(s.size() * 2);
    for (char ch : s) {
        const auto c = static_cast<unsigned char>(ch);
        if (c < 0x80) {
            out.push_back(static_cast<char>(c));
        } else if (c < 0xC0) {
            appendUtf8(out, kHigh[c - 0x80]);
        } else {
            appendUtf8(out, static_cast<char32_t>(0x0410 + (c - 0xC0)));
        }
    }
    return out;
}

bool isNetWhiteSpace(char32_t c) {
    if (c == 0x20 || (c >= 0x09 && c <= 0x0D)) return true;
    if (c == 0x85 || c == 0xA0 || c == 0x1680) return true;
    if (c >= 0x2000 && c <= 0x200A) return true;
    return c == 0x2028 || c == 0x2029 || c == 0x202F || c == 0x205F || c == 0x3000;
}

std::string netTrim(std::string_view s) {
    const std::u32string u = utf8ToU32(s);
    std::size_t b = 0;
    std::size_t e = u.size();
    while (b < e && isNetWhiteSpace(u[b]))
        ++b;
    while (e > b && isNetWhiteSpace(u[e - 1]))
        --e;
    return u32ToUtf8(std::u32string_view(u).substr(b, e - b));
}

std::string netToLower(std::string_view s) {
    std::u32string u = utf8ToU32(s);
    for (char32_t& c : u) {
        if ((c >= U'A' && c <= U'Z') || (c >= 0x0410 && c <= 0x042F)) {
            c += 32;  // латиница и А..Я
        } else if (c >= 0x0400 && c <= 0x040F) {
            c += 80;  // Ѐ..Џ, в том числе Ё -> ё
        }
    }
    return u32ToUtf8(u);
}

std::vector<std::string> splitNetLines(std::string_view text) {
    std::vector<std::string> lines;
    std::size_t start = 0;
    // Метка BOM в начале — признак кодировки, а не часть первой строки:
    // StreamReader снимает её сам.
    if (text.size() >= 3 && static_cast<unsigned char>(text[0]) == 0xEF &&
        static_cast<unsigned char>(text[1]) == 0xBB &&
        static_cast<unsigned char>(text[2]) == 0xBF) {
        start = 3;
    }
    std::size_t i = start;
    while (i < text.size()) {
        const char c = text[i];
        if (c == '\r' || c == '\n') {
            lines.emplace_back(text.substr(start, i - start));
            if (c == '\r' && i + 1 < text.size() && text[i + 1] == '\n') ++i;
            ++i;
            start = i;
        } else {
            ++i;
        }
    }
    if (start < text.size()) lines.emplace_back(text.substr(start));
    return lines;
}

std::string stripBom(std::string_view s) {
    while (s.size() >= 3 && static_cast<unsigned char>(s[0]) == 0xEF &&
           static_cast<unsigned char>(s[1]) == 0xBB && static_cast<unsigned char>(s[2]) == 0xBF) {
        s.remove_prefix(3);
    }
    return std::string(s);
}

bool parseNetInt(std::string_view sv, int& value) {
    const std::u32string s = utf8ToU32(sv);
    std::size_t j = 0;
    const std::size_t n = s.size();
    while (j < n && isNetWhiteSpace(s[j]))
        ++j;
    if (j >= n) return false;
    int sign = 1;
    if (s[j] == U'-') {
        sign = -1;
        ++j;
    } else if (s[j] == U'+') {
        ++j;
    }
    if (j >= n) return false;
    if (s[j] < U'0' || s[j] > U'9') return false;
    std::int64_t res = s[j] - U'0';
    ++j;
    while (j < n && s[j] >= U'0' && s[j] <= U'9') {
        res = res * 10 + (s[j] - U'0');
        if (res > 2147483648LL) return false;
        ++j;
    }
    while (j < n && isNetWhiteSpace(s[j]))
        ++j;
    if (j < n) return false;
    if (sign > 0 && res > 2147483647LL) return false;
    value = static_cast<int>(sign * res);
    return true;
}

}  // namespace tjtm
