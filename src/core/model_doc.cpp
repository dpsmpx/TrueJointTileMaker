// Слои и кадры (этапы 3 и 4 RoadToProLevel): таблица ячеек, сведение,
// команды над слоями, кадрами и метками.
//
// Картинка активной ячейки — Model::tile: инструменты и команды 0.6.0 и
// этапов 1–2 работают с ней, не зная о слоях. Смена ячейки сначала
// записывает tile в таблицу, потом берёт новую.

#include <algorithm>

#include "core/model.h"
#include "core/pmath.h"
#include "core/tilemap.h"

namespace tjtm {

namespace {

constexpr Color kClear = argb(0, 0, 0, 0);

bool blankCel(const Grid<Color>& img, const Grid<PalIndex>& idx) {
    for (const Color c : img.d) {
        if (c.a != 0) return false;
    }
    return std::all_of(idx.d.begin(), idx.d.end(), [](PalIndex v) { return v == IDX_NONE; });
}

/// Цвет точки слоя с его непрозрачностью.
Color withOpacity(Color c, int opacity) {
    if (opacity >= 255) return c;
    return argb(c.a * clampI(opacity, 0, 255) / 255, c.r, c.g, c.b);
}

/// Подкраска соседнего кадра «луковой кожи»: прошлые — к красному,
/// будущие — к синему, и полупрозрачно, чтобы не спорить с текущим.
Color onionTint(Color c, bool past, int distance) {
    if (c.a == 0) return c;
    const Color t = past ? rgb(220, 70, 70) : rgb(70, 110, 230);
    const int r = (c.r + t.r) / 2;
    const int g = (c.g + t.g) / 2;
    const int b = (c.b + t.b) / 2;
    return argb(c.a * 90 / 255 / std::max(1, distance), r, g, b);
}

}  // namespace

void Model::resetDoc() {
    doc = SpriteDoc{};
    doc.layers.push_back(Layer{"Слой 1"});
    doc.frames.push_back(Frame{});
    doc.cels.assign(1, std::vector<CelPtr>(1));
    activeLayer = 0;
    activeFrame = 0;
    tileShapes.clear();
    tileMap.clear();
}

void Model::commitActiveCel() {
    if (doc.layerCount() == 0 || doc.frameCount() == 0) resetDoc();
    activeLayer = clampI(activeLayer, 0, doc.layerCount() - 1);
    activeFrame = clampI(activeFrame, 0, doc.frameCount() - 1);
    ensureTileIdx();
    // У группы ячеек нет: её картинка — сведение детей.
    if (activeKind() == LK_GROUP) return;
    // Пустая картинка — пустая ячейка: файл короче, а на ленте кадров
    // видно, где рисунок есть.
    const CelPtr cur = doc.cel(activeLayer, activeFrame);
    const bool noTiles =
        std::all_of(tileMap.d.begin(), tileMap.d.end(), [](int v) { return v == 0; });
    if (blankCel(tile, tileIdx) && tileShapes.empty() && noTiles) {
        doc.setCel(activeLayer, activeFrame, nullptr);
    } else if (cur && cur->img.sameSize(tile.w, tile.h) && cur->img.d == tile.d &&
               cur->idx.d == tileIdx.d && cur->shapes == tileShapes && cur->tiles == tileMap) {
        // Не правили — ячейка остаётся прежней и общей с копиями кадра:
        // листание кадров не плодит картинок и не рвёт связь копий.
    } else {
        doc.setCel(activeLayer, activeFrame,
                   std::make_shared<const Cel>(Cel{tile, tileIdx, tileShapes, tileMap}));
    }
}

void Model::loadActiveCel() {
    const int w = tile.w;
    const int h = tile.h;
    const CelPtr c = doc.cel(activeLayer, activeFrame);
    if (c && c->img.sameSize(w, h)) {
        tile = c->img;
        tileIdx = c->idx;
        if (!tileIdx.sameSize(w, h)) tileIdx.resize(w, h, IDX_NONE);
        tileShapes = c->shapes;
        tileMap = c->tiles;
    } else {
        tile.resize(w, h, kClear);
        tileIdx.resize(w, h, IDX_NONE);
        tileShapes.clear();
        tileMap.clear();
    }
    // Пустая ячейка тайлкарты — карта из пустых клеток нужного размера.
    if (const Tileset* ts = activeTileset();
        ts != nullptr && !tileMap.sameSize(tmap::cols(w, ts->tw), tmap::rows(h, ts->th))) {
        Grid<int> m(tmap::cols(w, ts->tw), tmap::rows(h, ts->th), 0);
        for (int y = 0; y < std::min(m.h, tileMap.h); ++y) {
            for (int x = 0; x < std::min(m.w, tileMap.w); ++x)
                m.at(x, y) = tileMap.at(x, y);
        }
        tileMap = std::move(m);
    }
    tileBmpDirty = true;
    overDirty = true;
    needRepaint = true;
}

void Model::setActiveCel(int layer, int frame) {
    if (doc.layerCount() == 0) resetDoc();
    layer = clampI(layer, 0, doc.layerCount() - 1);
    frame = clampI(frame, 0, doc.frameCount() - 1);
    if (layer == activeLayer && frame == activeFrame) return;
    commitActiveCel();
    activeLayer = layer;
    activeFrame = frame;
    loadActiveCel();
}

bool Model::activeLayerEditable() const {
    if (activeLayer < 0 || activeLayer >= doc.layerCount()) return true;
    // Группа сама не рисуется, а скрытая или запертая группа прячет и
    // запирает всех своих детей.
    return activeKind() != LK_GROUP && layerShownDeep(activeLayer) && !layerLockedDeep(activeLayer);
}

void Model::forEachCel(const std::function<void(Grid<Color>&, Grid<PalIndex>&)>& fn, int what,
                       const std::function<void(VecShape&)>& shapeFn) {
    ensureTileIdx();
    // Цвет фигуры меняется той же правкой, что и точки: она идёт по
    // картинке 1x1 этого цвета.
    auto recolor = [&](Color& c) {
        if (c.a == 0) return;
        Grid<Color> g(1, 1, c);
        Grid<PalIndex> i(1, 1, indexOfColor(c));
        fn(g, i);
        c = g.at(0, 0);
    };
    std::vector<int> touchedSets;
    std::vector<int> baked;
    auto apply = [&](int l, Grid<Color>& img, Grid<PalIndex>& idx, std::vector<VecShape>& shapes,
                     Grid<int>& tiles) {
        const Layer& layer = doc.layers[static_cast<std::size_t>(l)];
        if (layer.kind == LK_TILEMAP && what == CEL_COLOR) {
            // Тайлкарта перекрашивается в своём наборе, отрисовка — потом.
            if (std::find(touchedSets.begin(), touchedSets.end(), layer.tileset) ==
                touchedSets.end())
                touchedSets.push_back(layer.tileset);
            return;
        }
        fn(img, idx);
        if (layer.kind == LK_VECTOR) {
            if (what == CEL_COLOR) {
                for (VecShape& s : shapes) {
                    recolor(s.stroke);
                    recolor(s.fill);
                }
            } else if (shapeFn) {
                for (VecShape& s : shapes)
                    shapeFn(s);
            } else {
                shapes.clear();
                if (std::find(baked.begin(), baked.end(), l) == baked.end()) baked.push_back(l);
            }
        }
        if (layer.kind == LK_TILEMAP) {
            // Сетка клеток после сдвига или поворота уже не совпадает с
            // тайлами: слой остаётся картинкой.
            tiles.clear();
            if (std::find(baked.begin(), baked.end(), l) == baked.end()) baked.push_back(l);
        }
    };
    if (activeKind() != LK_GROUP) apply(activeLayer, tile, tileIdx, tileShapes, tileMap);
    for (int l = 0; l < doc.layerCount(); ++l) {
        if (doc.layers[static_cast<std::size_t>(l)].kind == LK_GROUP) continue;
        for (int f = 0; f < doc.frameCount(); ++f) {
            if (l == activeLayer && f == activeFrame) continue;
            const CelPtr c = doc.cel(l, f);
            if (!c) continue;
            // Картинка ячейки неизменяема: правка — новая ячейка, а старую
            // держат снимки истории.
            Cel copy = *c;
            if (!copy.idx.sameSize(copy.img.w, copy.img.h))
                copy.idx.resize(copy.img.w, copy.img.h, IDX_NONE);
            apply(l, copy.img, copy.idx, copy.shapes, copy.tiles);
            doc.setCel(l, f, std::make_shared<const Cel>(std::move(copy)));
        }
    }
    for (const int l : baked) {
        doc.layers[static_cast<std::size_t>(l)].kind = LK_RASTER;
        doc.layers[static_cast<std::size_t>(l)].tileset = -1;
    }
    for (const int t : touchedSets) {
        if (t < 0 || t >= static_cast<int>(doc.tilesets.size())) continue;
        Tileset ts = *doc.tilesets[static_cast<std::size_t>(t)];
        for (Grid<Color>& g : ts.tiles) {
            Grid<PalIndex> i(g.w, g.h, IDX_NONE);
            for (std::size_t k = 0; k < g.d.size(); ++k)
                i.d[k] = indexOfColor(g.d[k]);
            fn(g, i);
        }
        doc.tilesets[static_cast<std::size_t>(t)] = std::make_shared<const Tileset>(std::move(ts));
        rerenderTileset(t);
    }
    tileBmpDirty = true;
    overDirty = true;
}

void Model::forEachCelConst(const std::function<void(const Grid<Color>&)>& fn) const {
    fn(tile);
    for (int l = 0; l < doc.layerCount(); ++l) {
        for (int f = 0; f < doc.frameCount(); ++f) {
            if (l == activeLayer && f == activeFrame) continue;
            if (const CelPtr c = doc.cel(l, f)) fn(c->img);
        }
    }
}

Color Model::compositeLayers(int f, int from, int to, int x, int y) const {
    // Слои снизу вверх; группа лежит над своими детьми. Дети копятся на
    // своём уровне стопки, а группа сводит его на уровень ниже — со своей
    // видимостью и непрозрачностью, как отдельная картинка.
    std::vector<Color> stack{kClear};
    const int base =
        from >= 0 && from < doc.layerCount() ? doc.layers[static_cast<std::size_t>(from)].depth : 0;
    auto fold = [&stack](std::size_t keep) {
        while (stack.size() > keep) {
            const Color c = stack.back();
            stack.pop_back();
            stack.back() = blendOver(c, stack.back());
        }
    };
    for (int l = from; l <= to && l < doc.layerCount(); ++l) {
        const Layer& layer = doc.layers[static_cast<std::size_t>(l)];
        const auto d = static_cast<std::size_t>(std::clamp(layer.depth - base, 0, 64));
        while (stack.size() < d + 1)
            stack.push_back(kClear);
        if (layer.kind == LK_GROUP) {
            fold(d + 2);
            const Color inner = stack.size() > d + 1 ? stack[d + 1] : kClear;
            stack.resize(d + 1);
            if (layer.visible) {
                const Color src = withOpacity(inner, layer.opacity);
                if (src.a != 0) stack[d] = blendOver(src, stack[d]);
            }
            continue;
        }
        fold(d + 1);
        if (!layer.visible) continue;
        Color src = kClear;
        if (l == activeLayer && f == activeFrame) {
            if (tile.inside(x, y)) src = tile.at(x, y);
        } else if (const CelPtr c = doc.cel(l, f); c && c->img.inside(x, y)) {
            src = c->img.at(x, y);
        }
        src = withOpacity(src, layer.opacity);
        if (src.a != 0) stack[d] = blendOver(src, stack[d]);
    }
    fold(1);
    return stack[0];
}

Color Model::compositeAt(int f, int x, int y) const {
    if (doc.layerCount() == 0) return tile.inside(x, y) ? tile.at(x, y) : kClear;
    return compositeLayers(f, 0, doc.layerCount() - 1, x, y);
}

Grid<Color> Model::compositeFrame(int f) const {
    Grid<Color> g(tile.w, tile.h, kClear);
    for (int y = 0; y < g.h; ++y) {
        for (int x = 0; x < g.w; ++x)
            g.at(x, y) = compositeAt(f, x, y);
    }
    return g;
}

Color Model::viewColorAt(int x, int y) const {
    const Color cur = compositeAt(activeFrame, x, y);
    if (!onionSkin || doc.frameCount() < 2) return cur;
    Color under = kClear;
    // Дальние кадры — бледнее ближних, и рисуются раньше них.
    for (int k = onionNext; k >= 1; --k) {
        const int f = activeFrame + k;
        if (f < doc.frameCount())
            under = blendOver(onionTint(compositeAt(f, x, y), false, k), under);
    }
    for (int k = onionPrev; k >= 1; --k) {
        const int f = activeFrame - k;
        if (f >= 0) under = blendOver(onionTint(compositeAt(f, x, y), true, k), under);
    }
    return blendOver(cur, under);
}

// --- слои -------------------------------------------------------------------

int Model::addLayer() {
    if (docMode != DOC_TILE) return -1;
    pushUndo();
    commitActiveCel();
    // Новый слой — сосед активного сверху, на его глубине: над группой —
    // рядом с ней, внутри группы — внутри.
    const int at = activeLayer + 1;
    Layer l{"Слой " + std::to_string(doc.layerCount() + 1)};
    l.depth = doc.layers[static_cast<std::size_t>(activeLayer)].depth;
    doc.layers.insert(doc.layers.begin() + at, l);
    doc.cels.insert(doc.cels.begin() + at,
                    std::vector<CelPtr>(static_cast<std::size_t>(doc.frameCount())));
    activeLayer = at;
    loadActiveCel();
    return at;
}

bool Model::deleteLayer() {
    if (docMode != DOC_TILE) return false;
    const int s = subtreeStart(activeLayer);
    const int n = activeLayer - s + 1;
    // Без единого слоя с картинкой документ не бывает.
    int drawable = 0;
    for (int l = 0; l < doc.layerCount(); ++l) {
        if ((l < s || l > activeLayer) && doc.layers[static_cast<std::size_t>(l)].kind != LK_GROUP)
            ++drawable;
    }
    if (drawable == 0) return false;
    pushUndo();
    commitActiveCel();
    doc.layers.erase(doc.layers.begin() + s, doc.layers.begin() + s + n);
    doc.cels.erase(doc.cels.begin() + s, doc.cels.begin() + s + n);
    activeLayer = std::min(s, doc.layerCount() - 1);
    loadActiveCel();
    return true;
}

int Model::duplicateLayer() {
    if (docMode != DOC_TILE) return -1;
    pushUndo();
    commitActiveCel();
    // Копия группы — копия всего её поддерева; ячейки общие, пока их не
    // правят.
    const int s = subtreeStart(activeLayer);
    const int n = activeLayer - s + 1;
    std::vector<Layer> layers(doc.layers.begin() + s, doc.layers.begin() + activeLayer + 1);
    layers.back().name += " копия";
    std::vector<std::vector<CelPtr>> cels(doc.cels.begin() + s, doc.cels.begin() + activeLayer + 1);
    const int at = activeLayer + 1;
    doc.layers.insert(doc.layers.begin() + at, layers.begin(), layers.end());
    doc.cels.insert(doc.cels.begin() + at, cels.begin(), cels.end());
    activeLayer = at + n - 1;
    loadActiveCel();
    return activeLayer;
}

namespace {

/// Поменять местами соседние ряды [a0, a1] и [a1 + 1, b1] в списке.
template <class V>
void swapBlocks(V& v, int a0, int a1, int b1) {
    std::rotate(v.begin() + a0, v.begin() + a1 + 1, v.begin() + b1 + 1);
}

}  // namespace

bool Model::moveLayer(int dir) {
    if (docMode != DOC_TILE || dir == 0) return false;
    const int a = activeLayer;
    const int s = subtreeStart(a);
    const int d = doc.layers[static_cast<std::size_t>(a)].depth;
    // Слой ходит среди соседей своей глубины целым поддеревом и из группы
    // так не выходит: для этого есть «из группы».
    if (dir > 0) {
        int t = a + 1;
        while (t < doc.layerCount() && doc.layers[static_cast<std::size_t>(t)].depth > d)
            ++t;
        if (t >= doc.layerCount() || doc.layers[static_cast<std::size_t>(t)].depth != d ||
            subtreeStart(t) != a + 1)
            return false;
        pushUndo();
        commitActiveCel();
        swapBlocks(doc.layers, s, a, t);
        swapBlocks(doc.cels, s, a, t);
        activeLayer = t;
    } else {
        if (s == 0) return false;
        const int below = s - 1;
        if (doc.layers[static_cast<std::size_t>(below)].depth != d) return false;
        const int bs = subtreeStart(below);
        pushUndo();
        commitActiveCel();
        swapBlocks(doc.layers, bs, below, a);
        swapBlocks(doc.cels, bs, below, a);
        activeLayer = bs + (a - s);
    }
    loadActiveCel();
    return true;
}

bool Model::mergeLayerDown() {
    if (docMode != DOC_TILE || activeLayer <= 0) return false;
    const int up = activeLayer;
    const int lo = activeLayer - 1;
    const Layer& upper0 = doc.layers[static_cast<std::size_t>(up)];
    const Layer& lower0 = doc.layers[static_cast<std::size_t>(lo)];
    // Сливаются соседи одной глубины, и в растровый слой: сводить группы —
    // своя команда.
    if (upper0.kind == LK_GROUP || lower0.kind != LK_RASTER || lower0.depth != upper0.depth)
        return false;
    pushUndo();
    commitActiveCel();
    const Layer& upper = doc.layers[static_cast<std::size_t>(up)];
    for (int f = 0; f < doc.frameCount(); ++f) {
        const CelPtr a = doc.cel(up, f);
        if (!a || !upper.visible) continue;
        const CelPtr b = doc.cel(lo, f);
        Cel merged = b ? *b
                       : Cel{Grid<Color>(tile.w, tile.h, kClear),
                             Grid<PalIndex>(tile.w, tile.h, IDX_NONE),
                             {},
                             {}};
        if (!merged.idx.sameSize(merged.img.w, merged.img.h))
            merged.idx.resize(merged.img.w, merged.img.h, IDX_NONE);
        for (std::size_t k = 0; k < merged.img.d.size() && k < a->img.d.size(); ++k) {
            const Color src = withOpacity(a->img.d[k], upper.opacity);
            if (src.a == 0) continue;
            merged.img.d[k] = blendOver(src, merged.img.d[k]);
            // Верхняя точка легла целиком — её номер; иначе вышла смесь, и
            // номер выводится по цвету.
            const bool whole = src.a == 255 && k < a->idx.d.size();
            merged.idx.d[k] = whole ? a->idx.d[k] : indexOfColor(merged.img.d[k]);
        }
        doc.setCel(lo, f, std::make_shared<const Cel>(std::move(merged)));
    }
    doc.layers.erase(doc.layers.begin() + up);
    doc.cels.erase(doc.cels.begin() + up);
    activeLayer = lo;
    loadActiveCel();
    enforceIndexed();
    return true;
}

void Model::setLayerVisible(int l, bool v) {
    if (l < 0 || l >= doc.layerCount()) return;
    doc.layers[static_cast<std::size_t>(l)].visible = v;
    markModified();
    tileBmpDirty = true;
    needRepaint = true;
}

void Model::setLayerLocked(int l, bool v) {
    if (l < 0 || l >= doc.layerCount()) return;
    doc.layers[static_cast<std::size_t>(l)].locked = v;
    markModified();
    needRepaint = true;
}

void Model::renameLayer(int l, const std::string& name) {
    if (l < 0 || l >= doc.layerCount() || name.empty()) return;
    pushUndo();
    doc.layers[static_cast<std::size_t>(l)].name = name;
    needRepaint = true;
}

void Model::setLayerOpacity(int l, int opacity) {
    if (l < 0 || l >= doc.layerCount()) return;
    pushUndo();
    doc.layers[static_cast<std::size_t>(l)].opacity = clampI(opacity, 0, 255);
    tileBmpDirty = true;
    needRepaint = true;
}

// --- кадры ------------------------------------------------------------------

int Model::addFrame(bool copy) {
    if (docMode != DOC_TILE) return -1;
    pushUndo();
    commitActiveCel();
    const int at = activeFrame + 1;
    doc.frames.insert(doc.frames.begin() + at, doc.frames[static_cast<std::size_t>(activeFrame)]);
    for (auto& row : doc.cels) {
        // Копия кадра делит картинки с исходным: они неизменяемы, и
        // правка любого из двух создаст свою.
        const CelPtr src = copy ? row[static_cast<std::size_t>(activeFrame)] : nullptr;
        row.insert(row.begin() + at, src);
    }
    // Метки после вставки сдвигаются, а метка с активным кадром растёт на
    // новый кадр.
    for (AnimTag& t : doc.tags) {
        if (t.from >= at) {
            ++t.from;
            ++t.to;
        } else if (t.to >= at - 1) {
            ++t.to;
        }
    }
    activeFrame = at;
    loadActiveCel();
    return at;
}

bool Model::deleteFrame() {
    if (docMode != DOC_TILE || doc.frameCount() <= 1) return false;
    pushUndo();
    commitActiveCel();
    const int f = activeFrame;
    doc.frames.erase(doc.frames.begin() + f);
    for (auto& row : doc.cels)
        row.erase(row.begin() + f);
    for (std::size_t i = 0; i < doc.tags.size();) {
        AnimTag& t = doc.tags[i];
        if (t.from > f) --t.from;
        if (t.to >= f) --t.to;
        // Метка из одного удалённого кадра пропадает.
        if (t.to < t.from) {
            doc.tags.erase(doc.tags.begin() + static_cast<std::ptrdiff_t>(i));
        } else {
            ++i;
        }
    }
    activeFrame = std::min(f, doc.frameCount() - 1);
    loadActiveCel();
    return true;
}

bool Model::moveFrame(int dir) {
    const int to = activeFrame + dir;
    if (docMode != DOC_TILE || to < 0 || to >= doc.frameCount() || dir == 0) return false;
    pushUndo();
    commitActiveCel();
    std::swap(doc.frames[static_cast<std::size_t>(activeFrame)],
              doc.frames[static_cast<std::size_t>(to)]);
    for (auto& row : doc.cels)
        std::swap(row[static_cast<std::size_t>(activeFrame)], row[static_cast<std::size_t>(to)]);
    activeFrame = to;
    loadActiveCel();
    return true;
}

void Model::setFrameDuration(int f, int ms) {
    if (f < 0 || f >= doc.frameCount()) return;
    pushUndo();
    doc.frames[static_cast<std::size_t>(f)].durationMs = clampI(ms, 10, 60000);
}

void Model::setFramesDuration(int from, int to, int ms) {
    from = std::max(0, from);
    to = std::min(doc.frameCount() - 1, to);
    if (from > to) return;
    pushUndo();
    for (int f = from; f <= to; ++f)
        doc.frames[static_cast<std::size_t>(f)].durationMs = clampI(ms, 10, 60000);
}

int Model::addTag(const std::string& name, int from, int to, int direction) {
    if (doc.frameCount() == 0) return -1;
    from = clampI(from, 0, doc.frameCount() - 1);
    to = clampI(to, 0, doc.frameCount() - 1);
    if (from > to) std::swap(from, to);
    pushUndo();
    doc.tags.push_back(AnimTag{name, from, to, clampI(direction, TAG_FORWARD, TAG_PINGPONG)});
    return static_cast<int>(doc.tags.size()) - 1;
}

bool Model::deleteTag(int i) {
    if (i < 0 || i >= static_cast<int>(doc.tags.size())) return false;
    pushUndo();
    doc.tags.erase(doc.tags.begin() + i);
    return true;
}

bool Model::setTag(int i, const AnimTag& t) {
    if (i < 0 || i >= static_cast<int>(doc.tags.size()) || doc.frameCount() == 0) return false;
    AnimTag v = t;
    v.from = clampI(v.from, 0, doc.frameCount() - 1);
    v.to = clampI(v.to, 0, doc.frameCount() - 1);
    if (v.from > v.to) std::swap(v.from, v.to);
    v.direction = clampI(v.direction, TAG_FORWARD, TAG_PINGPONG);
    pushUndo();
    doc.tags[static_cast<std::size_t>(i)] = v;
    return true;
}

int Model::tagOfFrame(int f) const {
    for (std::size_t i = 0; i < doc.tags.size(); ++i) {
        if (f >= doc.tags[i].from && f <= doc.tags[i].to) return static_cast<int>(i);
    }
    return -1;
}

std::vector<AnimFrame> Model::animationFrames(int from, int to) const {
    std::vector<AnimFrame> out;
    from = std::max(0, from);
    to = std::min(doc.frameCount() - 1, to);
    for (int f = from; f <= to; ++f)
        out.push_back(
            AnimFrame{compositeFrame(f), doc.frames[static_cast<std::size_t>(f)].durationMs});
    return out;
}

void Model::setAnimation(const std::vector<AnimFrame>& frames, const std::string& fileName) {
    if (frames.empty()) return;
    setTile(frames.front().image, fileName);
    const int w = tile.w;
    const int h = tile.h;
    doc.frames.clear();
    doc.cels.assign(1, {});
    for (const AnimFrame& fr : frames) {
        doc.frames.push_back(Frame{clampI(fr.durationMs, 10, 60000)});
        // Кадры чужого файла могут быть меньше холста: недостающее прозрачно.
        auto c = std::make_shared<Cel>(
            Cel{Grid<Color>(w, h, kClear), Grid<PalIndex>(w, h, IDX_NONE), {}, {}});
        for (int y = 0; y < std::min(h, fr.image.h); ++y) {
            for (int x = 0; x < std::min(w, fr.image.w); ++x) {
                c->img.at(x, y) = fr.image.at(x, y);
                c->idx.at(x, y) = indexOfColor(fr.image.at(x, y));
            }
        }
        doc.cels[0].push_back(blankCel(c->img, c->idx) ? nullptr : CelPtr(std::move(c)));
    }
    activeLayer = 0;
    activeFrame = 0;
    loadActiveCel();
    overDirty = true;
    needRepaint = true;
}

int Model::nextPlayFrame(int f, int tag, int& pingDir) const {
    const int n = doc.frameCount();
    if (n <= 1) return 0;
    int a = 0;
    int b = n - 1;
    int dir = TAG_FORWARD;
    if (tag >= 0 && tag < static_cast<int>(doc.tags.size())) {
        const AnimTag& t = doc.tags[static_cast<std::size_t>(tag)];
        a = clampI(t.from, 0, n - 1);
        b = clampI(t.to, a, n - 1);
        dir = t.direction;
    }
    if (f < a || f > b) return dir == TAG_REVERSE ? b : a;
    if (a == b) return a;
    if (dir == TAG_REVERSE) return f - 1 < a ? b : f - 1;
    if (dir == TAG_PINGPONG) {
        if (pingDir == 0) pingDir = 1;
        if (f + pingDir > b || f + pingDir < a) pingDir = -pingDir;
        return f + pingDir;
    }
    return f + 1 > b ? a : f + 1;
}

}  // namespace tjtm
