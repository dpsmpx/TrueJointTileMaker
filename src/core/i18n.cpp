#include "core/i18n.h"

#include <algorithm>
#include <fstream>
#include <sstream>
#include <unordered_map>

namespace tjtm::i18n {

namespace {

struct Pattern {
    std::vector<std::string> parts;  // куски между {}
    std::string to;                  // перевод с {} по порядку
};

struct State {
    std::string lang = "ru";
    std::unordered_map<std::string, std::string> exact;
    std::vector<Pattern> patterns;
    std::vector<std::pair<std::string, std::string>> pieces;
    // Шаблоны и куски дают новые строки: они хранятся здесь, чтобы
    // text() мог вернуть ссылку.
    std::unordered_map<std::string, std::string> made;
    int raw = 0;
};

State& st() {
    static State s;
    return s;
}

std::string unescape(const std::string& s) {
    std::string out;
    for (std::size_t i = 0; i < s.size(); ++i) {
        if (s[i] == '\\' && i + 1 < s.size() && s[i + 1] == 'n') {
            out += '\n';
            ++i;
        } else if (s[i] == '\\' && i + 1 < s.size() && s[i + 1] == 't') {
            out += '\t';
            ++i;
        } else {
            out += s[i];
        }
    }
    return out;
}

std::vector<std::string> splitHoles(const std::string& s) {
    std::vector<std::string> parts;
    std::size_t from = 0;
    while (true) {
        const std::size_t p = s.find("{}", from);
        if (p == std::string::npos) {
            parts.push_back(s.substr(from));
            break;
        }
        parts.push_back(s.substr(from, p - from));
        from = p + 2;
    }
    return parts;
}

int holes(const std::string& s) {
    return static_cast<int>(splitHoles(s).size()) - 1;
}

/// Строка по шаблону: куски на своих местах, между ними — непустое.
bool matchPattern(const Pattern& p, const std::string& s, std::vector<std::string>& got) {
    got.clear();
    const std::vector<std::string>& parts = p.parts;
    if (!s.starts_with(parts.front())) return false;
    std::size_t pos = parts.front().size();
    for (std::size_t i = 1; i < parts.size(); ++i) {
        const std::string& lit = parts[i];
        std::size_t at = 0;
        if (i + 1 == parts.size()) {
            // Последний кусок — в конце строки.
            if (s.size() < pos + lit.size() + 1 || !s.ends_with(lit)) return false;
            at = s.size() - lit.size();
        } else {
            if (lit.empty()) return false;  // два {} подряд не разделить
            at = s.find(lit, pos + 1);
            if (at == std::string::npos) return false;
        }
        if (at <= pos) return false;
        got.push_back(s.substr(pos, at - pos));
        pos = at + lit.size();
    }
    return true;
}

const std::string* lookup(const std::string& s) {
    State& t = st();
    if (const auto it = t.exact.find(s); it != t.exact.end()) return &it->second;
    if (const auto it = t.made.find(s); it != t.made.end()) return &it->second;
    std::string out;
    bool found = false;
    std::vector<std::string> got;
    for (const Pattern& p : t.patterns) {
        if (!matchPattern(p, s, got)) continue;
        // Подставленное — само строка интерфейса («Слой 2», «тайл»).
        std::size_t g = 0;
        std::size_t from = 0;
        while (true) {
            const std::size_t h = p.to.find("{}", from);
            if (h == std::string::npos) {
                out += p.to.substr(from);
                break;
            }
            out += p.to.substr(from, h - from);
            if (g < got.size()) {
                const auto it = t.exact.find(got[g]);
                out += it != t.exact.end() ? it->second : got[g];
            }
            ++g;
            from = h + 2;
        }
        found = true;
        break;
    }
    if (!found) {
        out = s;
        for (const auto& [from, to] : t.pieces) {
            for (std::size_t p = out.find(from); p != std::string::npos;
                 p = out.find(from, p + to.size())) {
                out.replace(p, from.size(), to);
                found = true;
            }
        }
    }
    if (!found) return nullptr;
    // Строки с числами не повторяются бесконечно, но копятся: держим
    // разумный запас.
    if (t.made.size() > 4096) t.made.clear();
    return &t.made.emplace(s, std::move(out)).first->second;
}

bool hasCyrillic(const std::string& s) {
    for (std::size_t i = 0; i + 1 < s.size(); ++i) {
        const auto c = static_cast<unsigned char>(s[i]);
        if (c == 0xD0 || c == 0xD1) return true;
    }
    return false;
}

}  // namespace

void setLanguage(const std::string& lang) {
    st().lang = lang == "en" ? "en" : "ru";
}

const std::string& language() {
    return st().lang;
}

int loadCatalogText(const std::string& text) {
    State& t = st();
    t.exact.clear();
    t.patterns.clear();
    t.pieces.clear();
    t.made.clear();
    std::istringstream in(text);
    std::string line;
    int n = 0;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        const std::size_t tab = line.find('\t');
        if (tab == std::string::npos || tab == 0) continue;
        std::string from = unescape(line.substr(0, tab));
        const std::string to = unescape(line.substr(tab + 1));
        if (from[0] == '~') {
            t.pieces.emplace_back(from.substr(1), to);
        } else if (from.find("{}") != std::string::npos) {
            if (holes(from) != holes(to)) continue;
            t.patterns.push_back({splitHoles(from), to});
        } else {
            t.exact[from] = to;
        }
        ++n;
    }
    // Шаблон точнее — первым: «страница {} из {},  тайлов {}» раньше
    // «страница {} из {}», иначе общий заберёт строку себе.
    auto fixed = [](const Pattern& p) {
        std::size_t len = 0;
        for (const std::string& part : p.parts)
            len += part.size();
        return len;
    };
    std::stable_sort(t.patterns.begin(), t.patterns.end(),
                     [&](const Pattern& a, const Pattern& b) { return fixed(a) > fixed(b); });
    // Длинные куски — первыми: «Палитра — » раньше «Палитра».
    std::stable_sort(t.pieces.begin(), t.pieces.end(),
                     [](const auto& a, const auto& b) { return a.first.size() > b.first.size(); });
    return n;
}

int loadCatalog(const std::string& path) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return loadCatalogText({});
    std::ostringstream ss;
    ss << f.rdbuf();
    return loadCatalogText(ss.str());
}

std::vector<std::string> catalogProblems(const std::string& text) {
    std::vector<std::string> bad;
    std::unordered_map<std::string, int> seen;
    std::istringstream in(text);
    std::string line;
    while (std::getline(in, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        if (line.empty() || line[0] == '#') continue;
        const std::size_t tab = line.find('\t');
        if (tab == std::string::npos || tab == 0 || tab + 1 >= line.size()) {
            bad.push_back("нет перевода: " + line);
            continue;
        }
        const std::string from = line.substr(0, tab);
        const std::string to = line.substr(tab + 1);
        if (to.find('\t') != std::string::npos) bad.push_back("две табуляции: " + line);
        if (holes(from) != holes(to)) bad.push_back("разное число {}: " + line);
        if (hasCyrillic(to)) bad.push_back("кириллица в переводе: " + line);
        if (++seen[from] == 2) bad.push_back("дважды: " + from);
    }
    return bad;
}

const std::string& text(const std::string& s) {
    const State& t = st();
    if (t.lang == "ru" || t.raw > 0 || s.empty() || !hasCyrillic(s)) return s;
    const std::string* r = lookup(s);
    return r != nullptr ? *r : s;
}

std::string tr(const std::string& s) {
    return text(s);
}

bool has(const std::string& s) {
    const std::string lang = st().lang;
    st().lang = "en";
    const bool yes = lookup(s) != nullptr;
    st().lang = lang;
    return yes;
}

Raw::Raw() {
    ++st().raw;
}

Raw::~Raw() {
    --st().raw;
}

}  // namespace tjtm::i18n
