#include "core/mapobj.h"

#include <cstdlib>

namespace tjtm::mapobj {

namespace {

struct Spec {
    const char* label;
    bool number;
    bool optional;
    bool tail;
    const char* hint;
};

/// Поля вида — в порядке разбора world.cpp игры.
std::vector<Spec> specOf(const std::string& kind) {
    if (kind == "npc") return {{"Житель", false, false, false, "id из build_npcs() игры"}};
    if (kind == "item")
        return {{"Предмет", false, false, false, "id предмета"},
                {"Количество", true, true, false, "пусто — один"}};
    if (kind == "exit")
        return {
            {"Локация", false, false, false, "имя файла карты без .map"},
            {"X цели", true, false, false, ""},
            {"Y цели", true, false, false, ""},
            {"Условия", false, true, true, "key=ключ quest=код:этап counter=код:число deny=текст"}};
    if (kind == "spawn")
        return {{"Враг", false, false, false, "id из build_enemies() игры"},
                {"Сколько", true, false, false, "сколько врагов держит зона"},
                {"Радиус", true, false, false, "в клетках"}};
    if (kind == "chest")
        return {{"Золото", true, false, false, ""},
                {"Ключ", false, false, false, "id ключа или - без ключа"},
                {"Содержимое", false, true, true, "предмет:количество через пробел"}};
    if (kind == "note") return {{"Записка", false, false, false, "код записки"}};
    if (kind == "bed") return {};
    if (kind == "sign") return {{"Текст", false, false, true, ""}};
    return {{"Строка", false, true, true, "вид не из игры: хвост строки как есть"}};
}

bool isSpace(char c) {
    return c == ' ' || c == '\t';
}

std::string trim(const std::string& s) {
    std::size_t a = 0;
    std::size_t b = s.size();
    while (a < b && isSpace(s[a]))
        ++a;
    while (b > a && isSpace(s[b - 1]))
        --b;
    return s.substr(a, b - a);
}

bool parseInt(const std::string& s, long& v) {
    if (s.empty()) return false;
    char* end = nullptr;
    v = std::strtol(s.c_str(), &end, 10);
    return end != nullptr && *end == '\0';
}

std::vector<std::string> words(const std::string& s) {
    std::vector<std::string> out;
    std::size_t i = 0;
    while (i < s.size()) {
        while (i < s.size() && isSpace(s[i]))
            ++i;
        const std::size_t a = i;
        while (i < s.size() && !isSpace(s[i]))
            ++i;
        if (i > a) out.push_back(s.substr(a, i - a));
    }
    return out;
}

/// Условие перехода — как его разбирает игра.
std::string checkGate(const std::string& cond) {
    const std::vector<std::string> w = words(cond);
    for (const std::string& t : w) {
        auto pair = [&](std::size_t prefix, const char* what) -> std::string {
            const std::string v = t.substr(prefix);
            const std::size_t colon = v.find(':');
            if (colon == std::string::npos || colon == 0 || colon + 1 >= v.size())
                return std::string("Условия: ") + what;
            return {};
        };
        if (t.starts_with("key=")) continue;
        if (t.starts_with("quest=")) {
            if (std::string e = pair(6, "quest=код:этап"); !e.empty()) return e;
            continue;
        }
        if (t.starts_with("counter=")) {
            if (std::string e = pair(8, "counter=код:число"); !e.empty()) return e;
            continue;
        }
        // deny= забирает весь остаток строки.
        if (t.starts_with("deny=")) return {};
        return "Условия: игра не знает «" + t + "» — бывают key=, quest=, counter=, deny=";
    }
    return {};
}

}  // namespace

std::vector<Field> fieldsOf(const std::string& kind, const std::string& rest) {
    std::vector<Field> out;
    std::size_t pos = 0;
    auto next = [&]() {
        while (pos < rest.size() && isSpace(rest[pos]))
            ++pos;
        const std::size_t a = pos;
        while (pos < rest.size() && !isSpace(rest[pos]))
            ++pos;
        return rest.substr(a, pos - a);
    };
    for (const Spec& s : specOf(kind)) {
        Field f;
        f.label = s.label;
        f.number = s.number;
        f.optional = s.optional;
        f.tail = s.tail;
        f.hint = s.hint;
        if (s.tail) {
            f.value = trim(rest.substr(std::min(pos, rest.size())));
            pos = rest.size();
        } else {
            f.value = next();
        }
        out.push_back(std::move(f));
    }
    // Лишние слова игра у этого вида пропускает; форма их бережёт.
    const std::string extra = trim(rest.substr(std::min(pos, rest.size())));
    if (!extra.empty()) {
        Field f;
        f.label = "Прочее";
        f.optional = true;
        f.tail = true;
        f.hint = "слова сверх того, что читает игра";
        f.value = extra;
        out.push_back(std::move(f));
    }
    return out;
}

std::string restOf(const std::vector<Field>& fields) {
    std::string out;
    for (const Field& f : fields) {
        const std::string v = trim(f.value);
        if (v.empty()) continue;
        if (!out.empty()) out += ' ';
        out += v;
    }
    return out;
}

std::string checkFields(const std::string& kind, const std::vector<Field>& fields) {
    for (const Field& f : fields) {
        const std::string v = trim(f.value);
        if (v.empty()) {
            if (!f.optional) return "Не заполнено: " + f.label;
            continue;
        }
        if (!f.tail && words(v).size() != 1) return f.label + ": одно слово, без пробелов";
        long n = 0;
        if (f.number && !parseInt(v, n)) return f.label + ": нужно целое число";
        if (f.number && n < 0) return f.label + ": не меньше нуля";
        if (kind == "item" && f.label == "Количество" && n < 1) return "Количество: хотя бы один";
        if (kind == "spawn" && f.label == "Сколько" && n < 1) return "Сколько: хотя бы один";
        if (kind == "exit" && f.label == "Условия") {
            if (std::string e = checkGate(v); !e.empty()) return e;
        }
        if (kind == "chest" && f.label == "Содержимое") {
            for (const std::string& w : words(v)) {
                const std::size_t colon = w.find(':');
                long k = 0;
                if (colon == std::string::npos || colon == 0 || !parseInt(w.substr(colon + 1), k) ||
                    k <= 0)
                    return "Содержимое: «" + w + "» — нужно предмет:количество больше нуля";
            }
        }
    }
    return {};
}

}  // namespace tjtm::mapobj
