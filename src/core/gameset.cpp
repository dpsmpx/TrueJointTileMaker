#include "core/gameset.h"

#include <algorithm>
#include <numeric>

#include "core/anim.h"
#include "core/model.h"

namespace tjtm::game {

namespace {

constexpr Color kMarker = argb(255, 1, 2, 3);

}  // namespace

bool isMarker(Color c) {
    return c.argb() == kMarker.argb();
}

bool composeSheet(const std::vector<Grid<Color>>& slots, Grid<Color>& out, std::string& error) {
    if (slots.size() > static_cast<std::size_t>(kSheetSlots)) {
        error = "в лист не влезает больше 64 тайлов";
        return false;
    }
    int cw = 1;
    int ch = 1;
    for (const Grid<Color>& t : slots) {
        cw = std::max(cw, t.w);
        ch = std::max(ch, t.h);
    }
    if (cw > kSheetMaxTile || ch > kSheetMaxTile) {
        error = "тайл больше 63 точек по стороне в лист игры не влезает";
        return false;
    }
    // Запас в точку — разделитель даже у тайла во всю клетку.
    ++cw;
    ++ch;
    out = Grid<Color>(kSheetCols * cw, kSheetRows * ch, kMarker);
    for (std::size_t k = 0; k < slots.size(); ++k) {
        const Grid<Color>& t = slots[k];
        const int ox = static_cast<int>(k % kSheetCols) * cw;
        const int oy = static_cast<int>(k / kSheetCols) * ch;
        for (int y = 0; y < t.h; ++y) {
            for (int x = 0; x < t.w; ++x)
                out.at(ox + x, oy + y) = t.at(x, y);
        }
    }
    return true;
}

bool sliceSheet(const Grid<Color>& sheet, std::vector<Grid<Color>>& slots, std::string& error) {
    const int cw = sheet.w / kSheetCols;
    const int ch = sheet.h / kSheetRows;
    if (sheet.empty() || cw < 1 || ch < 1) {
        error = "не похоже на лист тайлов: картинка должна делиться на 8 клеток по каждой стороне";
        return false;
    }
    slots.assign(static_cast<std::size_t>(kSheetSlots), Grid<Color>{});
    for (int i = 0; i < kSheetRows; ++i) {
        for (int j = 0; j < kSheetCols; ++j) {
            const int ox = j * cw;
            const int oy = i * ch;
            // Край — по маркеру, как у игры: вправо по верхней строке клетки
            // и вниз по левому столбцу.
            int tw = 0;
            while (tw < cw && !isMarker(sheet.at(ox + tw, oy)))
                ++tw;
            int th = 0;
            while (th < ch && !isMarker(sheet.at(ox, oy + th)))
                ++th;
            if (tw < 1 || th < 1) continue;
            Grid<Color> t(tw, th);
            for (int y = 0; y < th; ++y) {
                for (int x = 0; x < tw; ++x)
                    t.at(x, y) = sheet.at(ox + x, oy + y);
            }
            slots[static_cast<std::size_t>(i * kSheetCols + j)] = std::move(t);
        }
    }
    return true;
}

Grid<Color> packAtlas(const std::vector<AtlasEntry>& entries, int maxWidth, int padding,
                      std::vector<AtlasPlace>& places) {
    places.clear();
    padding = std::max(0, padding);
    std::vector<std::size_t> order(entries.size());
    std::iota(order.begin(), order.end(), std::size_t(0));
    // Выше — раньше, при равной высоте — по имени: раскладка не зависит от
    // порядка файлов в папке.
    std::stable_sort(order.begin(), order.end(), [&](std::size_t a, std::size_t b) {
        if (entries[a].image.h != entries[b].image.h)
            return entries[a].image.h > entries[b].image.h;
        return entries[a].name < entries[b].name;
    });
    int widest = 0;
    for (const AtlasEntry& e : entries)
        widest = std::max(widest, e.image.w);
    maxWidth = std::max(maxWidth, widest + 2 * padding);
    int x = padding;
    int y = padding;
    int rowH = 0;
    int usedW = 0;
    for (const std::size_t i : order) {
        const Grid<Color>& g = entries[i].image;
        if (g.empty()) continue;
        if (x + g.w + padding > maxWidth && x > padding) {
            x = padding;
            y += rowH + padding;
            rowH = 0;
        }
        places.push_back({entries[i].name, x, y, g.w, g.h});
        x += g.w + padding;
        rowH = std::max(rowH, g.h);
        usedW = std::max(usedW, x);
    }
    Grid<Color> out(std::max(1, usedW), std::max(1, y + rowH + padding), argb(0, 0, 0, 0));
    for (std::size_t k = 0, p = 0; k < order.size(); ++k) {
        const Grid<Color>& g = entries[order[k]].image;
        if (g.empty()) continue;
        const AtlasPlace& pl = places[p++];
        for (int yy = 0; yy < g.h; ++yy) {
            for (int xx = 0; xx < g.w; ++xx)
                out.at(pl.x + xx, pl.y + yy) = g.at(xx, yy);
        }
    }
    return out;
}

std::string atlasJson(const std::vector<AtlasPlace>& places, const std::string& imageFile, int w,
                      int h) {
    auto kv = [](const char* key, const std::string& value) {
        return jsonString(key) + ": " + value;
    };
    auto num = [](int v) { return std::to_string(v); };
    auto rect = [&](int x, int y, int rw, int rh) {
        return "{ " + kv("x", num(x)) + ", " + kv("y", num(y)) + ", " + kv("w", num(rw)) + ", " +
               kv("h", num(rh)) + " }";
    };
    std::string o = "{ " + kv("frames", "{") + "\n";
    for (std::size_t i = 0; i < places.size(); ++i) {
        const AtlasPlace& p = places[i];
        o += "  " + jsonString(p.name + ".png") + ": {\n";
        o += "   " + kv("frame", rect(p.x, p.y, p.w, p.h)) + ",\n";
        o += "   " + kv("rotated", "false") + ",\n";
        o += "   " + kv("trimmed", "false") + ",\n";
        o += "   " + kv("spriteSourceSize", rect(0, 0, p.w, p.h)) + ",\n";
        o += "   " + kv("sourceSize", "{ " + kv("w", num(p.w)) + ", " + kv("h", num(p.h)) + " }") +
             "\n";
        o += i + 1 < places.size() ? "  },\n" : "  }\n";
    }
    o += " },\n";
    o += " " + kv("meta", "{") + "\n";
    o += "  " + kv("app", jsonString(kAppTitle)) + ",\n";
    o += "  " + kv("version", jsonString(kAppVersion)) + ",\n";
    o += "  " + kv("image", jsonString(imageFile)) + ",\n";
    o += "  " + kv("format", jsonString("RGBA8888")) + ",\n";
    o += "  " + kv("size", "{ " + kv("w", num(w)) + ", " + kv("h", num(h)) + " }") + ",\n";
    o += "  " + kv("scale", jsonString("1")) + "\n";
    o += " }\n}\n";
    return o;
}

int autotileMask(const MapDoc& map, int x, int y) {
    if (!map.inside(x, y)) return 0;
    const int t = map.at(x, y);
    // За краем карты — та же местность: стена у края не обрывается.
    auto same = [&](int nx, int ny) { return !map.inside(nx, ny) || map.at(nx, ny) == t; };
    return (same(x, y - 1) ? 1 : 0) | (same(x + 1, y) ? 2 : 0) | (same(x, y + 1) ? 4 : 0) |
           (same(x - 1, y) ? 8 : 0);
}

std::string autotileName(const std::string& slot, int mask) {
    return slot + "_" + std::to_string(mask);
}

Grid<Color> autotileSheet(const std::vector<Grid<Color>>& variants, int tw, int th) {
    Grid<Color> out(4 * tw, 4 * th, argb(0, 0, 0, 0));
    for (std::size_t k = 0; k < variants.size() && k < 16; ++k) {
        const int ox = static_cast<int>(k % 4) * tw;
        const int oy = static_cast<int>(k / 4) * th;
        const Grid<Color>& v = variants[k];
        for (int y = 0; y < std::min(th, v.h); ++y) {
            for (int x = 0; x < std::min(tw, v.w); ++x)
                out.at(ox + x, oy + y) = v.at(x, y);
        }
    }
    return out;
}

std::vector<Grid<Color>> sliceAutotileSheet(const Grid<Color>& sheet) {
    std::vector<Grid<Color>> out;
    const int tw = sheet.w / 4;
    const int th = sheet.h / 4;
    if (tw < 1 || th < 1) return out;
    for (int k = 0; k < 16; ++k) {
        Grid<Color> v(tw, th);
        for (int y = 0; y < th; ++y) {
            for (int x = 0; x < tw; ++x)
                v.at(x, y) = sheet.at((k % 4) * tw + x, (k / 4) * th + y);
        }
        out.push_back(std::move(v));
    }
    return out;
}

}  // namespace tjtm::game
