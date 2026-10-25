// Редактор карт (этап 10): кисть смесью местностей, замена местности и
// поиск объектов. Форма объекта — src/core/mapobj.*, окно —
// src/app/map_ui.cpp.

#include <algorithm>
#include <numeric>

#include "core/model.h"
#include "core/text.h"

namespace tjtm {

namespace {

/// Хэш клетки и мазка: одинаковый на всех платформах и в любом порядке.
std::uint32_t cellHash(int x, int y, std::uint32_t seed) {
    std::uint32_t h = static_cast<std::uint32_t>(x) * 0x8DA6B343U ^
                      static_cast<std::uint32_t>(y) * 0xD8163841U ^ seed * 0xCB1AB31FU;
    h ^= h >> 16;
    h *= 0x7FEB352DU;
    h ^= h >> 15;
    h *= 0x846CA68BU;
    h ^= h >> 16;
    return h;
}

/// Нижний регистр латиницы и кириллицы: поиск «Сундук» находит «сундук».
std::u32string lower(const std::string& s) {
    std::u32string u = utf8ToU32(s);
    for (char32_t& c : u) {
        if ((c >= U'A' && c <= U'Z') || (c >= U'А' && c <= U'Я')) {
            c += 32;
        } else if (c == U'Ё') {
            c = U'ё';
        }
    }
    return u;
}

}  // namespace

int Model::mapBrushAt(int x, int y) const {
    if (!mapMix) return mapBrush;
    const int total = std::accumulate(mapMixWeights.begin(), mapMixWeights.end(), 0);
    if (total <= 0) return mapBrush;
    auto r = static_cast<int>(cellHash(x, y, mapMixSeed) % static_cast<std::uint32_t>(total));
    for (int k = 0; k < MT_COUNT; ++k) {
        r -= std::max(0, mapMixWeights[static_cast<std::size_t>(k)]);
        if (r < 0) return k;
    }
    return mapBrush;
}

int Model::replaceMapTerrain(int from, int to, bool inSelection) {
    if (docMode != DOC_MAP || from == to || from < 0 || to < 0 || from >= MT_COUNT ||
        to >= MT_COUNT)
        return 0;
    const bool sel = inSelection && hasSelection;
    int n = 0;
    for (int y = 0; y < map.h; ++y) {
        for (int x = 0; x < map.w; ++x)
            n += map.at(x, y) == from && (!sel || isSelected(x, y)) ? 1 : 0;
    }
    if (n == 0) return 0;
    pushUndo();
    for (int y = 0; y < map.h; ++y) {
        for (int x = 0; x < map.w; ++x) {
            if (map.at(x, y) == from && (!sel || isSelected(x, y)))
                map.at(x, y) = static_cast<std::uint8_t>(to);
        }
    }
    overDirty = true;
    needRepaint = true;
    return n;
}

std::vector<int> Model::findMapObjects(const std::string& query) const {
    std::vector<int> out;
    const std::u32string q = lower(query);
    for (std::size_t k = 0; k < map.objects.size(); ++k) {
        const std::u32string line = lower(mapObjectLine(map.objects[k]));
        if (q.empty() || line.find(q) != std::u32string::npos) out.push_back(static_cast<int>(k));
    }
    return out;
}

}  // namespace tjtm
