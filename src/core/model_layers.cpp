// Этап 6 RoadToProLevel: группы слоёв, векторные слои и слои-тайлкарты.
//
// Дерево слоёв хранится плоским списком с глубиной (Layer::depth): дети
// группы лежат сплошным рядом прямо под ней. Поэтому всё, что ходит по
// слоям — сведение, ячейки, проект, — работает как прежде, а дерево
// читается из глубины там, где оно нужно.

#include <algorithm>

#include "core/model.h"
#include "core/pmath.h"
#include "core/tilemap.h"
#include "core/vector.h"

namespace tjtm {

namespace {

constexpr Color kClear = argb(0, 0, 0, 0);

/// Слой документа по номеру; один шаблон и для чтения, и для правки.
template <class Doc>
auto& layerAt(Doc& d, int l) {
    return d.layers[static_cast<std::size_t>(l)];
}

}  // namespace

// --- дерево ------------------------------------------------------------------

int Model::activeKind() const {
    if (activeLayer < 0 || activeLayer >= doc.layerCount()) return LK_RASTER;
    return layerAt(doc, activeLayer).kind;
}

int Model::layerParent(int l) const {
    if (l < 0 || l >= doc.layerCount()) return -1;
    const int d = layerAt(doc, l).depth;
    if (d <= 0) return -1;
    for (int i = l + 1; i < doc.layerCount(); ++i) {
        if (layerAt(doc, i).depth < d) return i;
    }
    return -1;
}

int Model::subtreeStart(int l) const {
    if (l < 0 || l >= doc.layerCount() || layerAt(doc, l).kind != LK_GROUP) return l;
    const int d = layerAt(doc, l).depth;
    int i = l;
    while (i > 0 && layerAt(doc, i - 1).depth > d)
        --i;
    return i;
}

bool Model::layerShownDeep(int l) const {
    for (int i = l; i >= 0; i = layerParent(i)) {
        if (!layerAt(doc, i).visible) return false;
    }
    return true;
}

bool Model::layerLockedDeep(int l) const {
    for (int i = l; i >= 0; i = layerParent(i)) {
        if (layerAt(doc, i).locked) return true;
    }
    return false;
}

bool Model::layerRowVisible(int l) const {
    for (int i = layerParent(l); i >= 0; i = layerParent(i)) {
        if (layerAt(doc, i).collapsed) return false;
    }
    return true;
}

// --- группы ------------------------------------------------------------------

int Model::groupLayer() {
    if (docMode != DOC_TILE) return -1;
    pushUndo();
    commitActiveCel();
    // Слой с поддеревом уходит на уровень глубже, а над ним встаёт группа
    // на его прежней глубине.
    const int s = subtreeStart(activeLayer);
    const int d = layerAt(doc, activeLayer).depth;
    for (int l = s; l <= activeLayer; ++l)
        ++layerAt(doc, l).depth;
    Layer g{"Группа " + std::to_string(doc.layerCount() + 1)};
    g.kind = LK_GROUP;
    g.depth = d;
    const int at = activeLayer + 1;
    doc.layers.insert(doc.layers.begin() + at, g);
    doc.cels.insert(doc.cels.begin() + at,
                    std::vector<CelPtr>(static_cast<std::size_t>(doc.frameCount())));
    activeLayer = at;
    loadActiveCel();
    return at;
}

bool Model::ungroupLayer() {
    if (docMode != DOC_TILE || activeKind() != LK_GROUP) return false;
    pushUndo();
    commitActiveCel();
    const int g = activeLayer;
    const int s = subtreeStart(g);
    for (int l = s; l < g; ++l)
        --layerAt(doc, l).depth;
    doc.layers.erase(doc.layers.begin() + g);
    doc.cels.erase(doc.cels.begin() + g);
    activeLayer = std::max(0, g - 1);
    loadActiveCel();
    return true;
}

bool Model::indentLayer() {
    if (docMode != DOC_TILE) return false;
    const int a = activeLayer;
    const int s = subtreeStart(a);
    const int d = layerAt(doc, a).depth;
    // Над слоем — соседняя группа его глубины: слой встаёт в неё верхним.
    int t = a + 1;
    while (t < doc.layerCount() && layerAt(doc, t).depth > d)
        ++t;
    if (t >= doc.layerCount() || layerAt(doc, t).depth != d || layerAt(doc, t).kind != LK_GROUP)
        return false;
    pushUndo();
    commitActiveCel();
    for (int l = s; l <= a; ++l)
        ++layerAt(doc, l).depth;
    std::rotate(doc.layers.begin() + s, doc.layers.begin() + a + 1, doc.layers.begin() + t);
    std::rotate(doc.cels.begin() + s, doc.cels.begin() + a + 1, doc.cels.begin() + t);
    activeLayer = t - 1;
    loadActiveCel();
    return true;
}

bool Model::outdentLayer() {
    if (docMode != DOC_TILE) return false;
    const int a = activeLayer;
    const int p = layerParent(a);
    if (p < 0) return false;
    pushUndo();
    commitActiveCel();
    // Слой выходит из группы и встаёт прямо над ней.
    const int s = subtreeStart(a);
    for (int l = s; l <= a; ++l)
        --layerAt(doc, l).depth;
    std::rotate(doc.layers.begin() + s, doc.layers.begin() + a + 1, doc.layers.begin() + p + 1);
    std::rotate(doc.cels.begin() + s, doc.cels.begin() + a + 1, doc.cels.begin() + p + 1);
    activeLayer = p;
    loadActiveCel();
    return true;
}

bool Model::flattenGroup() {
    if (docMode != DOC_TILE || activeKind() != LK_GROUP) return false;
    pushUndo();
    commitActiveCel();
    const int g = activeLayer;
    const int s = subtreeStart(g);
    // Сведение детей без самой группы: её видимость и непрозрачность
    // переходят к новому слою и действуют, как действовали.
    Layer flat = layerAt(doc, g);
    flat.kind = LK_RASTER;
    flat.collapsed = false;
    std::vector<CelPtr> row(static_cast<std::size_t>(doc.frameCount()));
    for (int f = 0; f < doc.frameCount(); ++f) {
        Cel c{
            Grid<Color>(tile.w, tile.h, kClear), Grid<PalIndex>(tile.w, tile.h, IDX_NONE), {}, {}};
        bool any = false;
        for (int y = 0; y < tile.h; ++y) {
            for (int x = 0; x < tile.w; ++x) {
                const Color v = s < g ? compositeLayers(f, s, g - 1, x, y) : kClear;
                c.img.at(x, y) = v;
                c.idx.at(x, y) = indexOfColor(v);
                any = any || v.a != 0;
            }
        }
        if (any) row[static_cast<std::size_t>(f)] = std::make_shared<const Cel>(std::move(c));
    }
    doc.layers.erase(doc.layers.begin() + s, doc.layers.begin() + g + 1);
    doc.cels.erase(doc.cels.begin() + s, doc.cels.begin() + g + 1);
    doc.layers.insert(doc.layers.begin() + s, flat);
    doc.cels.insert(doc.cels.begin() + s, row);
    activeLayer = s;
    loadActiveCel();
    return true;
}

void Model::setLayerCollapsed(int l, bool c) {
    if (l < 0 || l >= doc.layerCount()) return;
    layerAt(doc, l).collapsed = c;
    needRepaint = true;
}

// --- вектор ------------------------------------------------------------------

int Model::addVectorLayer() {
    const int at = addLayer();
    if (at < 0) return at;
    Layer& l = layerAt(doc, at);
    l.kind = LK_VECTOR;
    l.name = "Вектор " + std::to_string(doc.layerCount());
    return at;
}

bool Model::rasterizeLayer() {
    if (docMode != DOC_TILE) return false;
    const int k = activeKind();
    if (k != LK_VECTOR && k != LK_TILEMAP) return false;
    pushUndo();
    commitActiveCel();
    // Картинка ячеек уже есть — у слоя отнимаются только фигуры и сетка.
    for (int f = 0; f < doc.frameCount(); ++f) {
        if (const CelPtr c = doc.cel(activeLayer, f)) {
            Cel copy = *c;
            copy.shapes.clear();
            copy.tiles.clear();
            doc.setCel(activeLayer, f, std::make_shared<const Cel>(std::move(copy)));
        }
    }
    layerAt(doc, activeLayer).kind = LK_RASTER;
    layerAt(doc, activeLayer).tileset = -1;
    loadActiveCel();
    return true;
}

void Model::rerenderActiveVector() {
    tile = vec::rasterizeAll(tileShapes, tile.w, tile.h);
    // Сглаженный край (этап 8) в строгом индексном режиме — точками палитры.
    if (strictIndexedOn()) snapStrict(tile, Grid<Color>(tile.w, tile.h, argb(0, 0, 0, 0)));
    ensureTileIdx();
    for (std::size_t k = 0; k < tile.d.size(); ++k)
        tileIdx.d[k] = indexOfColor(tile.d[k]);
    tileBmpDirty = true;
    overDirty = true;
    needRepaint = true;
}

int Model::addVectorShape(const VecShape& s) {
    if (docMode != DOC_TILE || !activeLayerEditable() || s.pts.empty()) return -1;
    pushUndo();
    if (activeKind() == LK_VECTOR) {
        tileShapes.push_back(s);
        rerenderActiveVector();
        return static_cast<int>(tileShapes.size()) - 1;
    }
    // На растровом слое фигура сразу ложится точками — правится потом
    // как любой рисунок.
    Grid<Color> drawn = tile;
    vec::rasterize(s, drawn);
    if (strictIndexedOn()) snapStrict(drawn, tile);
    ensureTileIdx();
    for (std::size_t k = 0; k < drawn.d.size(); ++k) {
        if (drawn.d[k] == tile.d[k]) continue;
        const auto x = static_cast<int>(k % static_cast<std::size_t>(tile.w));
        const auto y = static_cast<int>(k / static_cast<std::size_t>(tile.w));
        if (clipToSelection && hasSelection && !isSelected(x, y)) continue;
        tile.d[k] = drawn.d[k];
        tileIdx.d[k] = indexOfColor(drawn.d[k]);
    }
    enforceIndexed();
    tileBmpDirty = true;
    needRepaint = true;
    return -1;
}

void Model::setVectorShape(int i, const VecShape& s) {
    if (i < 0 || i >= static_cast<int>(tileShapes.size())) return;
    tileShapes[static_cast<std::size_t>(i)] = s;
    rerenderActiveVector();
}

bool Model::deleteVectorShape(int i) {
    if (i < 0 || i >= static_cast<int>(tileShapes.size())) return false;
    pushUndo();
    tileShapes.erase(tileShapes.begin() + i);
    rerenderActiveVector();
    return true;
}

// --- тайлкарта ---------------------------------------------------------------

const Tileset* Model::activeTileset() const {
    if (activeKind() != LK_TILEMAP) return nullptr;
    const int t = layerAt(doc, activeLayer).tileset;
    if (t < 0 || t >= static_cast<int>(doc.tilesets.size())) return nullptr;
    return doc.tilesets[static_cast<std::size_t>(t)].get();
}

int Model::addTilemapLayer(int tw, int th) {
    if (docMode != DOC_TILE || tw < 1 || th < 1) return -1;
    Tileset ts;
    ts.name = "Тайлы " + std::to_string(doc.tilesets.size() + 1);
    ts.tw = tw;
    ts.th = th;
    ts.tiles.emplace_back(tw, th, kClear);
    const int at = addLayer();
    doc.tilesets.push_back(std::make_shared<const Tileset>(std::move(ts)));
    Layer& l = layerAt(doc, at);
    l.kind = LK_TILEMAP;
    l.tileset = static_cast<int>(doc.tilesets.size()) - 1;
    l.name = "Тайлкарта " + std::to_string(doc.layerCount());
    loadActiveCel();
    return at;
}

bool Model::convertLayerToTilemap(int tw, int th) {
    if (docMode != DOC_TILE || activeKind() != LK_RASTER || tw < 1 || th < 1) return false;
    pushUndo();
    commitActiveCel();
    // Один набор на все кадры слоя: одинаковые клетки разных кадров — один
    // тайл, и правка его меняет весь бег сразу.
    Tileset ts;
    ts.name = layerAt(doc, activeLayer).name;
    ts.tw = tw;
    ts.th = th;
    ts.tiles.emplace_back(tw, th, kClear);
    std::vector<Grid<int>> maps(static_cast<std::size_t>(doc.frameCount()));
    for (int f = 0; f < doc.frameCount(); ++f) {
        const CelPtr c = doc.cel(activeLayer, f);
        maps[static_cast<std::size_t>(f)] =
            tmap::slice(c ? c->img : Grid<Color>(tile.w, tile.h, kClear), ts);
    }
    doc.tilesets.push_back(std::make_shared<const Tileset>(ts));
    const int t = static_cast<int>(doc.tilesets.size()) - 1;
    Layer& l = layerAt(doc, activeLayer);
    l.kind = LK_TILEMAP;
    l.tileset = t;
    for (int f = 0; f < doc.frameCount(); ++f) {
        Grid<int>& cells = maps[static_cast<std::size_t>(f)];
        if (std::all_of(cells.d.begin(), cells.d.end(), [](int v) { return v == 0; })) {
            doc.setCel(activeLayer, f, nullptr);
            continue;
        }
        Cel c{tmap::render(ts, cells, tile.w, tile.h),
              Grid<PalIndex>(tile.w, tile.h, IDX_NONE),
              {},
              cells};
        for (std::size_t k = 0; k < c.img.d.size(); ++k)
            c.idx.d[k] = indexOfColor(c.img.d[k]);
        doc.setCel(activeLayer, f, std::make_shared<const Cel>(std::move(c)));
    }
    loadActiveCel();
    return true;
}

void Model::rerenderTileset(int t) {
    if (t < 0 || t >= static_cast<int>(doc.tilesets.size())) return;
    const Tileset& ts = *doc.tilesets[static_cast<std::size_t>(t)];
    for (int l = 0; l < doc.layerCount(); ++l) {
        const Layer& layer = layerAt(doc, l);
        if (layer.kind != LK_TILEMAP || layer.tileset != t) continue;
        for (int f = 0; f < doc.frameCount(); ++f) {
            if (l == activeLayer && f == activeFrame) continue;
            const CelPtr c = doc.cel(l, f);
            if (!c || c->tiles.empty()) continue;
            Cel copy = *c;
            copy.img = tmap::render(ts, copy.tiles, tile.w, tile.h);
            copy.idx.resize(tile.w, tile.h, IDX_NONE);
            for (std::size_t k = 0; k < copy.img.d.size(); ++k)
                copy.idx.d[k] = indexOfColor(copy.img.d[k]);
            doc.setCel(l, f, std::make_shared<const Cel>(std::move(copy)));
        }
    }
    if (const Tileset* a = activeTileset(); a == &ts && !tileMap.empty()) {
        tile = tmap::render(ts, tileMap, tile.w, tile.h);
        ensureTileIdx();
        for (std::size_t k = 0; k < tile.d.size(); ++k)
            tileIdx.d[k] = indexOfColor(tile.d[k]);
    }
    tileBmpDirty = true;
    overDirty = true;
    needRepaint = true;
}

void Model::syncActiveTilemap() {
    const Tileset* cur = activeTileset();
    if (cur == nullptr || tileMap.empty()) return;
    // Любая правка точек тайлкарты — кистью в режиме точек, стиранием
    // выделенного, командой экрана изображения — правка тайлов: клетка,
    // чья картинка разошлась с её тайлом, отдаёт
    // свои точки тайлу — и все копии тайла меняются. Нарисованное в пустой
    // клетке становится новым тайлом.
    // Правкой считается клетка, разошедшаяся с тайлом до правки: нетронутая
    // копия того же тайла не должна затереть правленую.
    const Tileset before = *cur;
    Tileset ts = *cur;
    bool changed = false;
    for (int cy = 0; cy < tileMap.h; ++cy) {
        for (int cx = 0; cx < tileMap.w; ++cx) {
            const Grid<Color> img = tmap::cellImage(tile, cx, cy, ts.tw, ts.th);
            int& v = tileMap.at(cx, cy);
            const bool have = v > 0 && v < static_cast<int>(before.tiles.size());
            if (have && before.tiles[static_cast<std::size_t>(v)] == img) continue;
            if (!have && std::all_of(img.d.begin(), img.d.end(), [](Color c) { return c.a == 0; }))
                continue;
            if (have) {
                ts.tiles[static_cast<std::size_t>(v)] = img;
            } else {
                v = tmap::findOrAdd(ts, img);
            }
            changed = true;
        }
    }
    if (!changed) return;
    const int t = layerAt(doc, activeLayer).tileset;
    doc.tilesets[static_cast<std::size_t>(t)] = std::make_shared<const Tileset>(std::move(ts));
    rerenderTileset(t);
}

int Model::tileIndexAt(int x, int y) const {
    const Tileset* ts = activeTileset();
    if (ts == nullptr || x < 0 || y < 0) return 0;
    const int cx = x / ts->tw;
    const int cy = y / ts->th;
    return tileMap.inside(cx, cy) ? tileMap.at(cx, cy) : 0;
}

void Model::setTileCell(int cx, int cy, int v) {
    const Tileset* ts = activeTileset();
    if (ts == nullptr || !tileMap.inside(cx, cy) || tileMap.at(cx, cy) == v) return;
    tileMap.at(cx, cy) = v;
    tmap::renderCell(*ts, tileMap, cx, cy, tile);
    ensureTileIdx();
    for (int y = cy * ts->th; y < std::min(tile.h, (cy + 1) * ts->th); ++y) {
        for (int x = cx * ts->tw; x < std::min(tile.w, (cx + 1) * ts->tw); ++x) {
            tileIdx.at(x, y) = indexOfColor(tile.at(x, y));
            if (liveDraw && drawTilePixel) drawTilePixel(x, y);
        }
    }
    tileBmpDirty = true;
}

void Model::stampTileAt(int x, int y) {
    const Tileset* ts = activeTileset();
    if (ts == nullptr || x < 0 || y < 0) return;
    const int v =
        toolTile == T_ERASER ? 0 : std::clamp(tileBrush, 0, static_cast<int>(ts->tiles.size()) - 1);
    setTileCell(x / ts->tw, y / ts->th, v);
}

void Model::fillTilesAt(int x, int y) {
    const Tileset* ts = activeTileset();
    if (ts == nullptr || x < 0 || y < 0) return;
    const int cx = x / ts->tw;
    const int cy = y / ts->th;
    if (!tileMap.inside(cx, cy)) return;
    const int from = tileMap.at(cx, cy);
    const int to = std::clamp(tileBrush, 0, static_cast<int>(ts->tiles.size()) - 1);
    if (from == to) return;
    pushUndo();
    // Заливка по клеткам того же тайла, связных по четырём соседям.
    std::vector<std::pair<int, int>> stack{{cx, cy}};
    while (!stack.empty()) {
        const auto [px, py] = stack.back();
        stack.pop_back();
        if (!tileMap.inside(px, py) || tileMap.at(px, py) != from) continue;
        setTileCell(px, py, to);
        stack.emplace_back(px + 1, py);
        stack.emplace_back(px - 1, py);
        stack.emplace_back(px, py + 1);
        stack.emplace_back(px, py - 1);
    }
    needRepaint = true;
}

int Model::addTileToActiveSet(const Grid<Color>& g) {
    const Tileset* cur = activeTileset();
    if (cur == nullptr) return -1;
    pushUndo();
    Tileset ts = *cur;
    Grid<Color> t(ts.tw, ts.th, kClear);
    for (int y = 0; y < std::min(t.h, g.h); ++y) {
        for (int x = 0; x < std::min(t.w, g.w); ++x)
            t.at(x, y) = g.at(x, y);
    }
    ts.tiles.push_back(std::move(t));
    const int idx = static_cast<int>(ts.tiles.size()) - 1;
    doc.tilesets[static_cast<std::size_t>(layerAt(doc, activeLayer).tileset)] =
        std::make_shared<const Tileset>(std::move(ts));
    tileBrush = idx;
    return idx;
}

int Model::addTilesToActiveSet(const std::vector<Grid<Color>>& tiles) {
    const Tileset* cur = activeTileset();
    if (cur == nullptr || tiles.empty()) return 0;
    pushUndo();
    Tileset ts = *cur;
    const std::size_t before = ts.tiles.size();
    for (const Grid<Color>& g : tiles) {
        Grid<Color> t(ts.tw, ts.th, kClear);
        for (int y = 0; y < std::min(t.h, g.h); ++y) {
            for (int x = 0; x < std::min(t.w, g.w); ++x)
                t.at(x, y) = g.at(x, y);
        }
        tmap::findOrAdd(ts, t);
    }
    const auto added = static_cast<int>(ts.tiles.size() - before);
    doc.tilesets[static_cast<std::size_t>(layerAt(doc, activeLayer).tileset)] =
        std::make_shared<const Tileset>(std::move(ts));
    return added;
}

bool Model::deleteTileFromActiveSet(int i) {
    const Tileset* cur = activeTileset();
    if (cur == nullptr || i <= 0 || i >= static_cast<int>(cur->tiles.size())) return false;
    pushUndo();
    commitActiveCel();
    const int t = layerAt(doc, activeLayer).tileset;
    Tileset ts = *cur;
    tmap::removeTile(ts, i);
    doc.tilesets[static_cast<std::size_t>(t)] = std::make_shared<const Tileset>(std::move(ts));
    // Ссылки на удалённый тайл становятся пустыми, на тайлы за ним —
    // сдвигаются: во всех слоях и кадрах этого набора.
    auto fix = [i](Grid<int>& cells) {
        for (int& v : cells.d) {
            if (v == i) {
                v = 0;
            } else if (v > i) {
                --v;
            }
        }
    };
    for (int l = 0; l < doc.layerCount(); ++l) {
        if (layerAt(doc, l).kind != LK_TILEMAP || layerAt(doc, l).tileset != t) continue;
        for (int f = 0; f < doc.frameCount(); ++f) {
            const CelPtr c = doc.cel(l, f);
            if (!c) continue;
            Cel copy = *c;
            fix(copy.tiles);
            doc.setCel(l, f, std::make_shared<const Cel>(std::move(copy)));
        }
    }
    loadActiveCel();
    rerenderTileset(t);
    if (tileBrush >= static_cast<int>(activeTileset()->tiles.size())) tileBrush = 0;
    return true;
}

}  // namespace tjtm
