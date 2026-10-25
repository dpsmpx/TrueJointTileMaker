#include "core/tilemap.h"

#include <algorithm>

namespace tjtm::tmap {

int cols(int w, int tw) {
    return tw > 0 ? (w + tw - 1) / tw : 0;
}

int rows(int h, int th) {
    return th > 0 ? (h + th - 1) / th : 0;
}

void renderCell(const Tileset& ts, const Grid<int>& map, int cx, int cy, Grid<Color>& img) {
    const int v = map.inside(cx, cy) ? map.at(cx, cy) : 0;
    const bool have = v > 0 && v < static_cast<int>(ts.tiles.size());
    for (int y = 0; y < ts.th; ++y) {
        for (int x = 0; x < ts.tw; ++x) {
            const int px = cx * ts.tw + x;
            const int py = cy * ts.th + y;
            if (!img.inside(px, py)) continue;
            Color c = argb(0, 0, 0, 0);
            if (have) {
                const Grid<Color>& t = ts.tiles[static_cast<std::size_t>(v)];
                if (t.inside(x, y)) c = t.at(x, y);
            }
            img.at(px, py) = c;
        }
    }
}

Grid<Color> render(const Tileset& ts, const Grid<int>& map, int w, int h) {
    Grid<Color> img(w, h, argb(0, 0, 0, 0));
    for (int cy = 0; cy < map.h; ++cy) {
        for (int cx = 0; cx < map.w; ++cx)
            renderCell(ts, map, cx, cy, img);
    }
    return img;
}

Grid<Color> cellImage(const Grid<Color>& img, int cx, int cy, int tw, int th) {
    Grid<Color> t(tw, th, argb(0, 0, 0, 0));
    for (int y = 0; y < th; ++y) {
        for (int x = 0; x < tw; ++x) {
            const int px = cx * tw + x;
            const int py = cy * th + y;
            if (img.inside(px, py)) t.at(x, y) = img.at(px, py);
        }
    }
    return t;
}

int findOrAdd(Tileset& ts, const Grid<Color>& tile) {
    if (std::all_of(tile.d.begin(), tile.d.end(), [](Color c) { return c.a == 0; })) return 0;
    if (ts.tiles.empty()) ts.tiles.emplace_back(ts.tw, ts.th, argb(0, 0, 0, 0));
    for (std::size_t i = 1; i < ts.tiles.size(); ++i) {
        if (ts.tiles[i] == tile) return static_cast<int>(i);
    }
    ts.tiles.push_back(tile);
    return static_cast<int>(ts.tiles.size()) - 1;
}

Grid<int> slice(const Grid<Color>& img, Tileset& ts) {
    Grid<int> map(cols(img.w, ts.tw), rows(img.h, ts.th), 0);
    for (int cy = 0; cy < map.h; ++cy) {
        for (int cx = 0; cx < map.w; ++cx)
            map.at(cx, cy) = findOrAdd(ts, cellImage(img, cx, cy, ts.tw, ts.th));
    }
    return map;
}

void removeTile(Tileset& ts, int index) {
    if (index <= 0 || index >= static_cast<int>(ts.tiles.size())) return;
    ts.tiles.erase(ts.tiles.begin() + index);
}

}  // namespace tjtm::tmap
