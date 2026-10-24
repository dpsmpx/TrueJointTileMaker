// Общее для тестов, сверяющих перенос с эталоном.
//
// Эталонные файлы tests/golden/*.txt пишет tools/golden.sh, прогоняя
// исходники оригинала под Mono. Тестам Mono не нужен: файлы лежат в
// репозитории, и сверка идёт где угодно.
#pragma once

#include <cstdint>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace golden {

/// FNV-1a 32 — тот же, что считает харнесс на Паскале.
struct Fnv {
    std::uint32_t h = 2166136261u;
    void byte(int b) { h = (h ^ static_cast<std::uint32_t>(b & 255)) * 16777619u; }
    void i32(int v) {
        const auto u = static_cast<std::uint32_t>(v);
        byte(static_cast<int>(u));
        byte(static_cast<int>(u >> 8));
        byte(static_cast<int>(u >> 16));
        byte(static_cast<int>(u >> 24));
    }
    std::string hex() const {
        std::ostringstream o;
        o.width(8);
        o.fill('0');
        o << std::hex << h;
        return o.str();
    }
};

/// Строки эталонного файла, разбитые на слова.
inline std::vector<std::vector<std::string>> readLines(const std::string& name) {
    std::vector<std::vector<std::string>> out;
    std::ifstream f(std::string(TJTM_SOURCE_DIR) + "/tests/golden/" + name);
    std::string line;
    while (std::getline(f, line)) {
        if (!line.empty() && line.back() == '\r') line.pop_back();
        std::istringstream is(line);
        std::vector<std::string> words;
        std::string w;
        while (is >> w)
            words.push_back(w);
        if (!words.empty()) out.push_back(words);
    }
    return out;
}

/// Хэши из строк «hash имя значение».
inline std::map<std::string, std::string> hashes(
    const std::vector<std::vector<std::string>>& lines) {
    std::map<std::string, std::string> m;
    for (const auto& w : lines) {
        if (w.size() == 3 && w[0] == "hash") m[w[1]] = w[2];
    }
    return m;
}

}  // namespace golden
