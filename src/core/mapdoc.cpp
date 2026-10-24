#include "core/mapdoc.h"

#include "core/fsutil.h"
#include "core/text.h"

namespace tjtm {

namespace {

constexpr char kTileChars[] = ".#~T,=:";

bool isBlank(char32_t c) {
    return c == U' ' || c == U'\t';
}

// Первые maxWords слов строки и остаток после них: maxWords + 1 частей.
// Остаток берётся как есть, вместе с хвостовыми пробелами.
std::vector<std::string> splitHead(const std::string& s8, int maxWords) {
    const std::u32string s = utf8ToU32(s8);
    std::vector<std::string> parts(static_cast<std::size_t>(maxWords) + 1);
    std::size_t i = 0;
    int n = 0;
    while (n < maxWords) {
        while (i < s.size() && isBlank(s[i]))
            ++i;
        if (i >= s.size()) break;
        const std::size_t b = i;
        while (i < s.size() && !isBlank(s[i]))
            ++i;
        parts[static_cast<std::size_t>(n)] = u32ToUtf8(std::u32string_view(s).substr(b, i - b));
        ++n;
    }
    while (i < s.size() && isBlank(s[i]))
        ++i;
    if (i < s.size())
        parts[static_cast<std::size_t>(maxWords)] = u32ToUtf8(std::u32string_view(s).substr(i));
    return parts;
}

int toIntDef(const std::string& s, int def) {
    int v = 0;
    return parseNetInt(netTrim(s), v) ? v : def;
}

// Снимает хвостовые CR и ведущие BOM — StripCR оригинала.
std::string stripCR(std::string s) {
    while (!s.empty() && s.back() == '\r')
        s.pop_back();
    return stripBom(s);
}

}  // namespace

char mapTileChar(int code) {
    if (code < 0 || code >= MT_COUNT) return '#';
    return kTileChars[code];
}

int mapCharTile(char32_t c) {
    // Неизвестный символ игра считает стеной; повторяем это поведение.
    for (int i = 0; i < MT_COUNT; ++i) {
        if (static_cast<char32_t>(kTileChars[i]) == c) return i;
    }
    return MT_WALL;
}

std::string mapTileName(int code) {
    switch (code) {
        case MT_FLOOR: return "пол";
        case MT_WALL: return "стена";
        case MT_WATER: return "вода";
        case MT_TREE: return "дерево";
        case MT_GRASS: return "трава";
        case MT_ROAD: return "дорога";
        case MT_DEADWATER: return "стоячая вода";
        default: return "?";
    }
}

bool mapTileWalkable(int code) {
    return code == MT_FLOOR || code == MT_GRASS || code == MT_ROAD || code == MT_DEADWATER;
}

int mapKindCount() {
    return 8;
}

std::string mapKindName(int i) {
    switch (i) {
        case 0: return "npc";
        case 1: return "item";
        case 2: return "exit";
        case 3: return "spawn";
        case 4: return "chest";
        case 5: return "note";
        case 6: return "bed";
        default: return "sign";
    }
}

std::string mapKindTemplate(int i) {
    // Заготовки повторяют порядок полей из разбора в world.cpp игры.
    switch (i) {
        case 0: return "elder";
        case 1: return "bread 1";
        case 2: return "forest 2 8";
        case 3: return "rat 2 4";
        case 4: return "0 - bread:1";
        case 5: return "ink";
        case 6: return "";
        default: return "Текст таблички";
    }
}

bool mapKindKnown(const std::string& kind) {
    for (int i = 0; i < mapKindCount(); ++i) {
        if (mapKindName(i) == kind) return true;
    }
    return false;
}

std::string mapObjectLine(const MapObject& o) {
    std::string r = o.kind + " " + std::to_string(o.x) + " " + std::to_string(o.y);
    if (!o.rest.empty()) r += " " + o.rest;
    return r;
}

void MapDoc::reset(int nw, int nh) {
    nw = nw < kMapMinSide ? kMapMinSide : (nw > kMapMaxSide ? kMapMaxSide : nw);
    nh = nh < kMapMinSide ? kMapMinSide : (nh > kMapMaxSide ? kMapMaxSide : nh);
    w = nw;
    h = nh;
    grid.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), MT_FLOOR);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (x == 0 || y == 0 || x == w - 1 || y == h - 1) at(x, y) = MT_WALL;
        }
    }
    objects.clear();
    name = "Новая локация";
}

void MapDoc::resize(int nw, int nh) {
    nw = nw < kMapMinSide ? kMapMinSide : (nw > kMapMaxSide ? kMapMaxSide : nw);
    nh = nh < kMapMinSide ? kMapMinSide : (nh > kMapMaxSide ? kMapMaxSide : nh);
    if (nw == w && nh == h) return;
    std::vector<std::uint8_t> ng(static_cast<std::size_t>(nw) * static_cast<std::size_t>(nh),
                                 MT_WALL);
    for (int y = 0; y < nh; ++y) {
        for (int x = 0; x < nw; ++x) {
            if (x < w && y < h) {
                ng[static_cast<std::size_t>(y) * static_cast<std::size_t>(nw) +
                   static_cast<std::size_t>(x)] = at(x, y);
            }
        }
    }
    grid = std::move(ng);
    w = nw;
    h = nh;
    std::size_t k = 0;
    while (k < objects.size()) {
        if (objects[k].x >= w || objects[k].y >= h) {
            deleteObject(static_cast<int>(k));
        } else {
            ++k;
        }
    }
}

int MapDoc::objectAt(int x, int y) const {
    int r = -1;
    for (std::size_t k = 0; k < objects.size(); ++k) {
        if (objects[k].x == x && objects[k].y == y) r = static_cast<int>(k);
    }
    return r;
}

void MapDoc::addObject(const std::string& kind, int x, int y, const std::string& rest) {
    objects.push_back(MapObject{kind, x, y, rest});
}

void MapDoc::deleteObject(int idx) {
    if (idx < 0 || idx >= static_cast<int>(objects.size())) return;
    objects.erase(objects.begin() + idx);
}

MapParseResult mapParse(const std::string& text, MapDoc& doc) {
    MapParseResult res;
    const std::vector<std::string> lines = splitNetLines(text);

    MapDoc nd;
    int w = 0;
    int h = 0;
    bool gridDone = false;
    std::size_t i = 0;
    while (i < lines.size()) {
        std::string s = lines[i];
        ++i;
        s = stripCR(s);
        if (s.empty()) continue;
        if (s[0] == ';') continue;

        std::vector<std::string> parts = splitHead(s, 1);
        const std::string key = netToLower(parts[0]);

        if (key == "name") {
            nd.name = parts[1];
        } else if (key == "size") {
            const std::vector<std::string> wh = splitHead(parts[1], 2);
            w = toIntDef(wh[0], 0);
            h = toIntDef(wh[1], 0);
            if (w < 1 || h < 1) {
                res.error =
                    "строка " + std::to_string(i) + ": size ожидает два положительных числа";
                return res;
            }
            if (w > kMapMaxSide || h > kMapMaxSide) {
                res.error = "локация больше " + std::to_string(kMapMaxSide) + " клеток по стороне";
                return res;
            }
        } else if (key == "grid") {
            if (w < 1 || h < 1) {
                res.error = "строка " + std::to_string(i) + ": grid до объявления size";
                return res;
            }
            nd.w = w;
            nd.h = h;
            nd.grid.assign(static_cast<std::size_t>(w) * static_cast<std::size_t>(h), MT_WALL);
            for (int y = 0; y < h; ++y) {
                if (i >= lines.size()) {
                    res.error = "сетка обрывается: не хватает строк";
                    return res;
                }
                // Длина строки — в единицах UTF-16, как Length в .NET: от неё
                // зависит, считается ли строка правильной.
                const std::u32string row = utf8ToU32(stripCR(lines[i]));
                ++i;
                const std::size_t len16 = utf16Length(u32ToUtf8(row));
                // Строку не той длины дополняем стеной или обрезаем, но
                // предупреждаем: файл лучше открыть и дать поправить.
                if (len16 != static_cast<std::size_t>(w)) {
                    res.warning = "строка сетки " + std::to_string(y + 1) + " была длиной " +
                                  std::to_string(len16) + " вместо " + std::to_string(w) +
                                  "; недостающее заполнено стеной";
                }
                // Символ вне BMP занимает в .NET две ячейки строки, и
                // каждая половина — неизвестный символ, то есть стена.
                std::u32string units;
                for (char32_t c : row) {
                    if (c >= 0x10000) {
                        units.push_back(0xD800);
                        units.push_back(0xDC00);
                    } else {
                        units.push_back(c);
                    }
                }
                for (int x = 0; x < w; ++x) {
                    nd.at(x, y) = static_cast<std::uint8_t>(
                        static_cast<std::size_t>(x) < units.size()
                            ? mapCharTile(units[static_cast<std::size_t>(x)])
                            : MT_WALL);
                }
            }
            gridDone = true;
        } else if (key == "objects") {
            continue;
        } else if (key == "end") {
            break;
        } else if (mapKindKnown(key)) {
            const std::vector<std::string> p = splitHead(s, 3);
            nd.addObject(p[0], toIntDef(p[1], 0), toIntDef(p[2], 0), p[3]);
        } else {
            res.error = "строка " + std::to_string(i) + ": неизвестная директива " + key;
            return res;
        }
    }

    if (!gridDone) {
        res.error = "в файле нет сетки";
        return res;
    }
    // Сетка могла быть объявлена раньше последнего size: размер документа
    // берётся из size, как в оригинале, а сетка уже ему соответствует,
    // только если они совпали.
    if (nd.w != w || nd.h != h) {
        res.error = "size после grid не совпадает с размером сетки";
        return res;
    }
    doc = std::move(nd);
    res.ok = true;
    return res;
}

MapParseResult mapLoad(const std::string& fileName, MapDoc& doc) {
    const auto data = readFile(fileName);
    if (!data) {
        MapParseResult r;
        r.error = "не удалось прочитать файл";
        return r;
    }
    return mapParse(*data, doc);
}

std::string MapDoc::serialize() const {
    std::string out;
    auto add = [&out](const std::string& s) {
        out += s;
        out += '\n';
    };
    add("name " + name);
    add("size " + std::to_string(w) + " " + std::to_string(h));
    add("grid");
    for (int y = 0; y < h; ++y) {
        std::string row;
        row.reserve(static_cast<std::size_t>(w));
        for (int x = 0; x < w; ++x)
            row.push_back(mapTileChar(at(x, y)));
        add(row);
    }
    add("objects");
    for (const MapObject& o : objects)
        add(mapObjectLine(o));
    add("end");
    return out;
}

bool mapSave(const std::string& fileName, const MapDoc& doc) {
    if (fileName.empty() || doc.w < 1 || doc.h < 1) return false;
    return writeFile(fileName, doc.serialize());
}

std::vector<std::string> MapDoc::validate() const {
    std::vector<std::string> out;
    if (w < 1 || h < 1) {
        out.emplace_back("Локация пуста.");
        return out;
    }
    // Объекты внутри локации и на проходимой клетке — то же проверяет
    // tools/check_maps.py в самой игре.
    for (const MapObject& o : objects) {
        if (o.x < 0 || o.y < 0 || o.x >= w || o.y >= h) {
            out.push_back(mapObjectLine(o) + " — координаты вне локации");
        } else if (!mapTileWalkable(at(o.x, o.y))) {
            out.push_back(mapObjectLine(o) + " — стоит на непроходимой клетке (" +
                          mapTileName(at(o.x, o.y)) + ")");
        }
    }

    // Связность проходимой части: игрок должен добираться до всего.
    std::vector<char> seen(grid.size(), 0);
    std::vector<int> q(grid.size());
    int walkable = 0;
    for (std::uint8_t c : grid) {
        if (mapTileWalkable(c)) ++walkable;
    }
    int regions = 0;
    auto idx = [this](int x, int y) {
        return static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
               static_cast<std::size_t>(x);
    };
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            if (!mapTileWalkable(at(x, y)) || seen[idx(x, y)] != 0) continue;
            ++regions;
            std::size_t head = 0;
            std::size_t tail = 0;
            auto push = [&](int px, int py) {
                if (px < 0 || py < 0 || px >= w || py >= h) return;
                if (seen[idx(px, py)] != 0 || !mapTileWalkable(at(px, py))) return;
                seen[idx(px, py)] = 1;
                q[tail++] = py * w + px;
            };
            push(x, y);
            while (head < tail) {
                const int cx = q[head] % w;
                const int cy = q[head] / w;
                ++head;
                push(cx + 1, cy);
                push(cx - 1, cy);
                push(cx, cy + 1);
                push(cx, cy - 1);
            }
        }
    }
    if (walkable == 0) {
        out.emplace_back("В локации нет ни одной проходимой клетки.");
    } else if (regions > 1) {
        out.push_back("Проходимая часть разбита на " + std::to_string(regions) +
                      " несвязанных областей: до части локации не дойти.");
    }
    return out;
}

}  // namespace tjtm
