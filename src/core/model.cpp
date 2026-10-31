#include "core/model.h"

#include <algorithm>

#include "core/fsutil.h"
#include "core/imageops.h"
#include "core/pmath.h"
#include "core/text.h"

namespace tjtm {

// ---------------------------------------------------------------------------
// Наборы тайлов, значки, карта
// ---------------------------------------------------------------------------

std::string tsetBaseFile(int i) {
    static const char* const kNames[TSET_BASE_COUNT] = {
        "floor", "wall", "water", "tree", "grass", "road",  "deadwater", "player", "npc",
        "mob",   "exit", "sign",  "item", "bed",   "chest", "portal",    "note"};
    return (i >= 0 && i < TSET_BASE_COUNT) ? kNames[i] : "";
}

std::string tsetBaseName(int i) {
    static const char* const kNames[TSET_BASE_COUNT] = {
        "пол",          "стена",   "вода",   "дерево", "трава",   "дорога",
        "стоячая вода", "герой",   "житель", "враг",   "переход", "табличка",
        "предмет",      "лежанка", "сундук", "портал", "записка"};
    return (i >= 0 && i < TSET_BASE_COUNT) ? kNames[i] : "";
}

bool tsetBaseIsTerrain(int i) {
    return i <= 6;
}

std::string tsetKindName(int k) {
    switch (k) {
        case TS_BASE: return "Основные";
        case TS_NPC: return "Жители";
        case TS_MOB: return "Враги";
        case TS_ICON: return "Значки";
        default: return "";
    }
}

int iconCount() {
    return T_TILE_COUNT + T_MAP_COUNT;
}

std::string iconFile(int i) {
    static const char* const kNames[] = {
        "tool_pen",      "tool_eraser",   "tool_fill",       "tool_pick",      "tool_line",
        "tool_rect",     "tool_ellipse",  "tool_dither",     "tool_select",    "tool_map_paint",
        "tool_map_fill", "tool_map_rect", "tool_map_object", "tool_map_select"};
    return (i >= 0 && i < iconCount()) ? kNames[i] : "";
}

std::string iconName(int i) {
    static const char* const kNames[] = {"карандаш",  "ластик",         "заливка",
                                         "пипетка",   "линия",          "прямоугольник",
                                         "овал",      "дизеринг",       "выделение",
                                         "местность", "заливка карты",  "прямоугольник карты",
                                         "объект",    "выделение карты"};
    return (i >= 0 && i < iconCount()) ? kNames[i] : "";
}

std::vector<std::string> scanPrefixed(const std::string& dir, const std::string& prefix) {
    std::vector<std::string> names;
    if (dir.empty()) return names;
    for (const std::string& f : listFiles(dir)) {
        if (pathExtLower(f) != ".png") continue;
        const std::string b = pathStem(f);
        // Префикс сверяется с учётом регистра: каталог в Windows его не
        // различает, а игра ищет файл по точному имени.
        if (!b.empty() && b.starts_with(prefix)) names.push_back(b);
    }
    return names;
}

Color mapTileColor(int code) {
    switch (code) {
        case MT_FLOOR: return rgb(196, 184, 160);
        case MT_WALL: return rgb(72, 70, 76);
        case MT_WATER: return rgb(58, 110, 190);
        case MT_TREE: return rgb(34, 96, 52);
        case MT_GRASS: return rgb(108, 168, 78);
        case MT_ROAD: return rgb(168, 146, 108);
        case MT_DEADWATER: return rgb(80, 128, 128);
        default: return rgb(200, 0, 200);
    }
}

std::string mapObjGlyph(const std::string& kind) {
    if (kind == "npc") return "N";
    if (kind == "item") return "*";
    if (kind == "exit") return ">";
    if (kind == "spawn") return "X";
    if (kind == "chest") return "C";
    if (kind == "note") return "n";
    if (kind == "bed") return "&";
    if (kind == "sign") return "!";
    return "?";
}

std::vector<std::string> splitWords(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == ' ') {
            if (!cur.empty()) {
                out.push_back(cur);
                cur.clear();
            }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

std::vector<std::string> splitLines(const std::string& s) {
    std::vector<std::string> out;
    std::string cur;
    for (char c : s) {
        if (c == '\n' || c == '\r') {
            if (!cur.empty()) {
                out.push_back(cur);
                cur.clear();
            }
        } else {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) out.push_back(cur);
    return out;
}

namespace {

Color checkerAt(int x, int y, Color a, Color b) {
    return ((x / 4) + (y / 4)) % 2 == 0 ? a : b;
}

void putBgra(std::vector<std::uint8_t>& raw, std::size_t k, Color c, std::uint8_t a) {
    raw[k + 0] = c.b;
    raw[k + 1] = c.g;
    raw[k + 2] = c.r;
    raw[k + 3] = a;
}

}  // namespace

std::vector<std::uint8_t> makeThumbRaw(const Grid<Color>& g, int size) {
    std::vector<std::uint8_t> raw;
    if (g.empty() || size < 1) return raw;
    raw.resize(static_cast<std::size_t>(size) * static_cast<std::size_t>(size) * 4);
    for (int py = 0; py < size; ++py) {
        for (int px = 0; px < size; ++px) {
            const int sx = clampI(px * g.w / size, 0, g.w - 1);
            const int sy = clampI(py * g.h / size, 0, g.h - 1);
            // Шахматка по точкам миниатюры, а не по пикселям тайла: клетки
            // шахматки одинаковы при любом размере исходника.
            const Color c = blendOver(g.at(sx, sy), checkerAt(px, py, theme::classicTheme().checkA,
                                                              theme::classicTheme().checkB));
            putBgra(raw,
                    (static_cast<std::size_t>(py) * static_cast<std::size_t>(size) +
                     static_cast<std::size_t>(px)) *
                        4,
                    c, 255);
        }
    }
    return raw;
}

std::vector<std::uint8_t> makeThumbAlphaRaw(const Grid<Color>& g, int size) {
    std::vector<std::uint8_t> raw;
    if (g.empty() || size < 1) return raw;
    raw.resize(static_cast<std::size_t>(size) * static_cast<std::size_t>(size) * 4);
    for (int py = 0; py < size; ++py) {
        for (int px = 0; px < size; ++px) {
            const int sx = clampI(px * g.w / size, 0, g.w - 1);
            const int sy = clampI(py * g.h / size, 0, g.h - 1);
            const Color c = g.at(sx, sy);
            putBgra(raw,
                    (static_cast<std::size_t>(py) * static_cast<std::size_t>(size) +
                     static_cast<std::size_t>(px)) *
                        4,
                    c, c.a);
        }
    }
    return raw;
}

// ---------------------------------------------------------------------------
// Модель
// ---------------------------------------------------------------------------

Model::Model() {
    setDefaultPalette();
    syncHSVFromColor();
}

int Model::docW() const {
    return docMode == DOC_MAP ? map.w : tile.w;
}

int Model::docH() const {
    return docMode == DOC_MAP ? map.h : tile.h;
}

bool Model::isModified() const {
    return docMode == DOC_MAP ? mapModified : tileModified;
}

void Model::markModified() {
    if (docMode == DOC_MAP) {
        mapModified = true;
    } else {
        tileModified = true;
    }
    overDirty = true;
}

int Model::screenToTileX(int sx) const {
    return zoomOut > 1 ? (sx - viewX) * zoomOut : floorDiv(sx - viewX, pixelSize);
}

int Model::screenToTileY(int sy) const {
    return zoomOut > 1 ? (sy - viewY) * zoomOut : floorDiv(sy - viewY, pixelSize);
}

bool Model::inDoc(int x, int y) const {
    return x >= 0 && y >= 0 && x < docW() && y < docH();
}

int Model::gridGap() const {
    return (gridOn && pixelSize >= 4) ? 1 : 0;
}

void Model::clampView() {
    const int fullW = scr(docW());
    const int fullH = scr(docH());
    // Холст — от (X0, Y0) до (W, H): слева и сверху могут стоять линейки.
    const int cw = W - X0;
    const int ch = H - Y0;
    viewX = fullW <= cw ? X0 + (cw - fullW) / 2 : clampI(viewX, W - fullW, X0);
    viewY = fullH <= ch ? Y0 + (ch - fullH) / 2 : clampI(viewY, H - fullH, Y0);
}

void Model::fitScale() {
    pixelSize = std::min((W - X0) / std::max(1, docW()), (H - Y0) / std::max(1, docH()));
    pixelSize = clampI(pixelSize, MIN_ZOOM, MAX_ZOOM);
    zoomOut = 1;
    // Лист больше холста — уменьшить, пока не влезет (не мельче 25 %).
    if (zoomSteps && docMode == DOC_TILE) {
        while (zoomOut < 4 && (scr(docW()) > W - X0 || scr(docH()) > H - Y0))
            zoomOut *= 2;
    }
    clampView();
}

namespace {

/// Ступени масштаба: точек экрана на точку документа и уменьшение.
struct ZoomStep {
    int ps, out;
};
constexpr ZoomStep kZoomSteps[] = {{1, 4}, {1, 2},  {1, 1},  {2, 1},  {3, 1},  {4, 1},  {6, 1},
                                   {8, 1}, {12, 1}, {16, 1}, {24, 1}, {32, 1}, {48, 1}, {64, 1}};

}  // namespace

void Model::setZoom(int ps, int out, int ax, int ay) {
    ps = clampI(ps, MIN_ZOOM, MAX_ZOOM);
    if (out < 1 || docMode != DOC_TILE) out = 1;
    if (out > 1) ps = 1;
    if (ps == pixelSize && out == zoomOut) return;
    const double before = scale();
    pixelSize = ps;
    zoomOut = out;
    const double k = scale() / before;
    viewX = ax - pround((ax - viewX) * k);
    viewY = ay - pround((ay - viewY) * k);
    clampView();
    needRepaint = true;
}

void Model::zoomAt(int dir, int ax, int ay) {
    if (zoomSteps) {
        // Следующая ступень от нынешнего масштаба; вписанный масштаб (5×)
        // бывает и между ступенями.
        const double now = scale();
        const ZoomStep* pick = nullptr;
        if (dir > 0) {
            for (const ZoomStep& z : kZoomSteps) {
                if (static_cast<double>(z.ps) / z.out > now) {
                    pick = &z;
                    break;
                }
            }
        } else {
            for (const ZoomStep& z : kZoomSteps) {
                if (static_cast<double>(z.ps) / z.out >= now) break;
                if (z.out == 1 || docMode == DOC_TILE) pick = &z;
            }
        }
        if (pick != nullptr) setZoom(pick->ps, pick->out, ax, ay);
        return;
    }
    const int old = pixelSize;
    if (dir > 0) {
        if (pixelSize < MAX_ZOOM) pixelSize *= 2;
    } else if (pixelSize > MIN_ZOOM) {
        pixelSize /= 2;
    }
    if (pixelSize == old) return;
    viewX = ax - pround(static_cast<double>((ax - viewX) * pixelSize) / old);
    viewY = ay - pround(static_cast<double>((ay - viewY) * pixelSize) / old);
    clampView();
    needRepaint = true;
}

void Model::resetSelection() {
    selX = selY = selW = selH = 0;
    hasSelection = false;
    draggingSelection = false;
    selMask.clear();
}

// --- палитра и привязка -----------------------------------------------------
//
// У каждого пикселя тайла — номер ячейки палитры, из которой взят его цвет,
// или IDX_NONE. Привязка — подсказка, а не источник истины: цвет остаётся в
// tile и перед перекраской сверяется с прежним цветом ячейки. Устаревшая
// привязка поэтому не портит картинку, она лишь не срабатывает.

Color Model::palColorOf(PalIndex idx) const {
    if (idx >= palette.size()) return argb(0, 0, 0, 0);
    return palette[idx];
}

PalIndex Model::indexOfColor(Color c) const {
    for (std::size_t k = 0; k < palette.size(); ++k) {
        if (sameRGB(c, palette[k])) return static_cast<PalIndex>(k);
    }
    return IDX_NONE;
}

void Model::ensureTileIdx() {
    if (tileIdx.sameSize(tile.w, tile.h)) return;
    tileIdx.resize(tile.w, tile.h, IDX_NONE);
}

void Model::setIdx(int x, int y, PalIndex v) {
    if (!tileIdx.sameSize(tile.w, tile.h)) return;
    if (!tile.inside(x, y)) return;
    tileIdx.at(x, y) = v;
}

int Model::rebindTileToPalette() {
    int n = 0;
    ensureTileIdx();
    for (int i = 0; i < tile.h; ++i) {
        for (int j = 0; j < tile.w; ++j) {
            tileIdx.at(j, i) = indexOfColor(tile.at(j, i));
            if (tileIdx.at(j, i) != IDX_NONE) ++n;
        }
    }
    return n;
}

int Model::recolorPaletteEntry(PalIndex idx, Color oldC, Color newC) {
    int n = 0;
    if (idx == IDX_NONE) return 0;
    if (!tileIdx.sameSize(tile.w, tile.h)) return 0;
    for (int i = 0; i < tile.h; ++i) {
        for (int j = 0; j < tile.w; ++j) {
            if (tileIdx.at(j, i) == idx && sameRGB(tile.at(j, i), oldC)) {
                tile.at(j, i) = argb(tile.at(j, i).a, newC.r, newC.g, newC.b);
                ++n;
            }
        }
    }
    return n;
}

int Model::boundPixelCount() const {
    if (!tileIdx.sameSize(tile.w, tile.h)) return 0;
    return static_cast<int>(std::count_if(tileIdx.d.begin(), tileIdx.d.end(),
                                          [](PalIndex v) { return v != IDX_NONE; }));
}

void Model::setDefaultPalette() {
    palette.assign(PAL_SIZE, Color{});
    for (int i = 0; i < PAL_ROWS; ++i) {
        for (int j = 0; j < PAL_COLS; ++j)
            palAt(j, i) = hsvToRgb(j * 45, 30 + i * 10, 100 - i * 8);
    }
}

void Model::paletteFromGrid(const Grid<Color>& g) {
    if (g.empty()) return;
    palette.resize(PAL_SIZE);
    for (int i = 0; i < PAL_ROWS; ++i) {
        for (int j = 0; j < PAL_COLS; ++j) {
            palAt(j, i) = g.at(clampI(j * g.w / PAL_COLS, 0, g.w - 1),
                               clampI(i * g.h / PAL_ROWS, 0, g.h - 1));
        }
    }
    needRepaint = true;
}

// --- история ----------------------------------------------------------------

int Model::undoDepth() const {
    const std::int64_t per =
        std::max<std::int64_t>(1, static_cast<std::int64_t>(docW()) * docH() * undoBytesPerPx);
    const std::int64_t steps =
        std::clamp<std::int64_t>(undoBudget / per, undoMinSteps, undoMaxSteps);
    return static_cast<int>(steps);
}

void Model::clearTileHistory() {
    undoStack.clear();
    redoStack.clear();
}

void Model::clearMapHistory() {
    mapUndo.clear();
    mapRedo.clear();
}

void Model::clearHistory() {
    clearTileHistory();
    clearMapHistory();
}

void Model::pushUndo() {
    const auto limit = static_cast<std::size_t>(undoDepth());
    if (docMode == DOC_MAP) {
        if (map.w < 1 || map.h < 1) return;
        // Снимок — вся карта, включая название: оригинал брал только сетку
        // и объекты, и отмена переименования ничего не возвращала.
        mapUndo.push_back(map);
        while (mapUndo.size() > limit)
            mapUndo.erase(mapUndo.begin());
        mapRedo.clear();
    } else {
        if (tile.w < 1 || tile.h < 1) return;
        ensureTileIdx();
        // Снимок берётся перед изменением, поэтому пометка здесь накрывает
        // любую отменяемую правку тайла разом.
        tileBmpDirty = true;
        undoStack.push_back(TileSnapshot{tile, tileIdx, palette, transparentIdx, doc, activeLayer,
                                         activeFrame, tileShapes, tileMap});
        while (undoStack.size() > limit)
            undoStack.erase(undoStack.begin());
        redoStack.clear();
    }
    markModified();
}

void Model::doUndo() {
    if (docMode == DOC_MAP) {
        if (mapUndo.empty()) return;
        mapRedo.push_back(map);
        map = std::move(mapUndo.back());
        mapUndo.pop_back();
    } else {
        if (undoStack.empty()) return;
        ensureTileIdx();
        redoStack.push_back(TileSnapshot{tile, tileIdx, palette, transparentIdx, doc, activeLayer,
                                         activeFrame, tileShapes, tileMap});
        TileSnapshot s = std::move(undoStack.back());
        undoStack.pop_back();
        tile = std::move(s.tile);
        tileIdx = std::move(s.idx);
        palette = std::move(s.palette);
        transparentIdx = s.transparentIdx;
        doc = std::move(s.doc);
        activeLayer = s.layer;
        activeFrame = s.frame;
        tileShapes = std::move(s.shapes);
        tileMap = std::move(s.tileMap);
        ensureTileIdx();
    }
    resetSelection();
    clampView();
    markModified();
    tileBmpDirty = true;
    needRepaint = true;
}

void Model::doRedo() {
    if (docMode == DOC_MAP) {
        if (mapRedo.empty()) return;
        mapUndo.push_back(map);
        map = std::move(mapRedo.back());
        mapRedo.pop_back();
    } else {
        if (redoStack.empty()) return;
        ensureTileIdx();
        undoStack.push_back(TileSnapshot{tile, tileIdx, palette, transparentIdx, doc, activeLayer,
                                         activeFrame, tileShapes, tileMap});
        TileSnapshot s = std::move(redoStack.back());
        redoStack.pop_back();
        tile = std::move(s.tile);
        tileIdx = std::move(s.idx);
        palette = std::move(s.palette);
        transparentIdx = s.transparentIdx;
        doc = std::move(s.doc);
        activeLayer = s.layer;
        activeFrame = s.frame;
        tileShapes = std::move(s.shapes);
        tileMap = std::move(s.tileMap);
        ensureTileIdx();
    }
    resetSelection();
    clampView();
    markModified();
    tileBmpDirty = true;
    needRepaint = true;
}

// --- документ ---------------------------------------------------------------

void Model::setTile(Grid<Color> newTile, const std::string& fileName) {
    tile = std::move(newTile);
    resetDoc();
    // Направляющие, начало координат и метаданные — свои у каждого
    // документа; сетка и прочий вид остаются привычкой работающего.
    guidesX2.clear();
    guidesY2.clear();
    originX = 0;
    originY = 0;
    docMeta.clear();
    tileBmpDirty = true;
    // Открытый файл ничего не знает о палитре, поэтому привязки выводятся
    // из его же цветов: совпал с ячейкой — значит, ею и управляется.
    rebindTileToPalette();
    currentFile = fileName;
    docMode = DOC_TILE;
    tileModified = false;
    resetSelection();
    forgetSelection(DOC_TILE);
    clearTileHistory();
    overDirty = true;
    fitScale();
    needRepaint = true;
}

std::string Model::metaValue(const std::string& key) const {
    for (const auto& [k, v] : docMeta) {
        if (k == key) return v;
    }
    return {};
}

void Model::setMeta(const std::string& key, const std::string& value) {
    for (auto& [k, v] : docMeta) {
        if (k == key) {
            v = value;
            return;
        }
    }
    docMeta.emplace_back(key, value);
}

void Model::newTileOfSize(int nw, int nh) {
    nw = clampI(nw, MIN_TILE, MAX_TILE);
    nh = clampI(nh, MIN_TILE, MAX_TILE);
    setTile(Grid<Color>(nw, nh, blankColor()), "");
}

void Model::newMapOfSize(int w, int h) {
    map.reset(w, h);
    currentMapFile.clear();
    mapModified = false;
    docMode = DOC_MAP;
    resetSelection();
    forgetSelection(DOC_MAP);
    clearMapHistory();
    overDirty = true;
    fitScale();
    needRepaint = true;
}

void Model::switchMode(int mode) {
    if (docMode == mode) return;
    if (keepSelectionPerDoc) {
        SavedSelection& s = savedSel[static_cast<std::size_t>(docMode)];
        s = {selX, selY, selW, selH, hasSelection, selMask};
    }
    docMode = mode;
    resetSelection();
    if (keepSelectionPerDoc) {
        // Возвращается только то, что ложится на документ: размер мог
        // смениться, пока он был не на экране.
        const SavedSelection& s = savedSel[static_cast<std::size_t>(mode)];
        const bool fits = s.on && s.x >= 0 && s.y >= 0 && s.x + s.w <= docW() &&
                          s.y + s.h <= docH() &&
                          (s.mask.empty() || (s.mask.w == docW() && s.mask.h == docH()));
        if (fits) {
            selX = s.x;
            selY = s.y;
            selW = s.w;
            selH = s.h;
            hasSelection = true;
            selMask = s.mask;
        }
    }
    fitScale();
    overDirty = true;
    needRepaint = true;
}

Color Model::checkerColor(int x, int y) const {
    return checkerAt(x, y, checkerA, checkerB);
}

Color Model::displayColor(int x, int y) const {
    // Слои и кадры показываются сведением; документ из одной картинки —
    // как в оригинале, прямо из tile.
    Color c = (multiCel() || onionSkin) ? viewColorAt(x, y) : tile.at(x, y);
    if (checkerOn && c.a < 255) c = blendOver(c, checkerColor(x, y));
    return c;
}

// --- кисть ------------------------------------------------------------------

Color Model::brushColorAt(int x, int y) const {
    // Строгий индексный режим: цвет — всегда цвет ячейки, которой кисть пишет.
    if (strictIndexedOn()) {
        const PalIndex i = brushIndexAt(x, y);
        return i == IDX_NONE ? argb(0, 0, 0, 0) : effectiveColor(i);
    }
    if (toolTile == T_ERASER) return argb(0, 0, 0, 0);
    if (toolTile == T_DITHER)
        return img::ditherFirst(ditherPattern, x, y) ? firstColor : secondColor;
    return firstColor;
}

bool Model::brushReplaces() const {
    // Ластик иначе не смог бы стирать: прозрачный цвет поверх по правилу
    // source-over ничего не меняет. В строгом индексном режиме смешивать
    // нельзя: смесь — не цвет палитры.
    return brushReplace || toolTile == T_ERASER || strictIndexedOn();
}

PalIndex Model::brushIndexAt(int x, int y) const {
    if (strictIndexedOn()) {
        if (toolTile == T_ERASER) return transparentIdx;
        if (toolTile == T_DITHER) {
            return img::ditherFirst(ditherPattern, x, y) ? resolveIndex(firstColor, firstIdx)
                                                         : resolveIndex(secondColor, secondIdx);
        }
        return resolveIndex(firstColor, firstIdx);
    }
    if (toolTile == T_ERASER) return IDX_NONE;
    if (toolTile == T_DITHER) return img::ditherFirst(ditherPattern, x, y) ? firstIdx : secondIdx;
    return firstIdx;
}

void Model::paintOne(int x, int y) {
    if (wrapOn && tile.w > 0 && tile.h > 0) {
        x = ((x % tile.w) + tile.w) % tile.w;
        y = ((y % tile.h) + tile.h) % tile.h;
    }
    if (!tile.inside(x, y)) return;
    if (clipToSelection && hasSelection && !isSelected(x, y)) return;
    // Скрытый или запертый слой не правится: рисунок на невидимом слое —
    // всегда ошибка.
    if (!activeLayerEditable()) return;
    // Тайлкарта, которую правят тайлами: кисть кладёт тайл в клетку под
    // точкой, ластик опустошает клетку (этап 6).
    if (activeKind() == LK_TILEMAP && !tilemapPixels) {
        stampTileAt(x, y);
        return;
    }
    if (strokeMask.sameSize(tile.w, tile.h)) {
        if (strokeMask.at(x, y) != 0) return;
        strokeMask.at(x, y) = 1;
    }
    // Затенение (этап 9) меняет цвет самой точки, а не кладёт цвет кисти;
    // маска мазка выше не даёт пройти по точке дважды за мазок.
    if (toolTile == T_PEN && penMode == PEN_SHADE) {
        shadeOne(x, y);
        return;
    }
    tileBmpDirty = true;
    const Color c = brushColorAt(x, y);
    // Неполная непрозрачность кисти — всегда смешивание: заменить точку
    // полупрозрачным цветом значило бы стереть то, что под ней. В строгом
    // индексном режиме смеси нет, и сила кисти не действует.
    if (brushOpacity < 255 && !strictIndexedOn()) {
        const int op = clampI(brushOpacity, 1, 255);
        Color& dst = tile.at(x, y);
        if (toolTile == T_ERASER) {
            dst = argb(static_cast<std::uint8_t>(dst.a * (255 - op) / 255), dst.r, dst.g, dst.b);
        } else {
            dst = blendOver(argb(static_cast<std::uint8_t>(c.a * op / 255), c.r, c.g, c.b), dst);
        }
        setIdx(x, y, indexOfColor(dst));
        if (liveDraw && drawTilePixel) drawTilePixel(x, y);
        return;
    }
    if (brushReplaces()) {
        tile.at(x, y) = c;
        setIdx(x, y, brushIndexAt(x, y));
    } else {
        tile.at(x, y) = blendOver(c, tile.at(x, y));
        // Смешанный цвет чаще всего уже не цвет палитры: привязка выводится
        // из того, что получилось, а не берётся у кисти.
        setIdx(x, y, indexOfColor(tile.at(x, y)));
    }
    if (liveDraw && drawTilePixel) drawTilePixel(x, y);
}

void Model::paintPixel(int x, int y) {
    forEachSymmetric(x, y, [this](int px, int py) { paintOne(px, py); });
}

void Model::putMapOne(int x, int y) {
    if (!map.inside(x, y)) return;
    if (strokeMask.sameSize(map.w, map.h)) {
        if (strokeMask.at(x, y) != 0) return;
        strokeMask.at(x, y) = 1;
    }
    map.at(x, y) = static_cast<std::uint8_t>(mapBrushAt(x, y));
}

void Model::putMapCell(int x, int y) {
    forEachSymmetric(x, y, [this](int px, int py) { putMapOne(px, py); });
}

void Model::emitPixel(int x, int y) {
    // Один растеризатор служит и показу, и применению: разницу задаёт
    // shapePreview.
    if (shapePreview) {
        if (!drawPreviewPixel) return;
        forEachSymmetric(x, y, [this](int px, int py) { drawPreviewPixel(px, py); });
    } else if (docMode == DOC_MAP) {
        putMapCell(x, y);
    } else if (strokeActive && toolTile == T_PEN && penMode == PEN_STAMP && !customBrush.empty()) {
        // Кисть из выделения (этап 9) ложится целиком в каждую точку пути.
        stampBrushAt(x, y);
    } else if (strokeActive && brushSized()) {
        forEachBrushCell(x, y, [this](int px, int py) { paintPixel(px, py); });
    } else if (strokeActive && pixelPerfect &&
               (toolTile == T_PEN || toolTile == T_ERASER || toolTile == T_DITHER)) {
        emitPixelPerfect(x, y);
    } else {
        paintPixel(x, y);
    }
}

bool Model::brushSized() const {
    if (docMode != DOC_TILE || brushSize <= 1) return false;
    if (toolTile == T_ERASER || toolTile == T_DITHER) return true;
    return toolTile == T_PEN && (penMode == PEN_PENCIL || penMode == PEN_SHADE);
}

void Model::forEachBrushCell(int x, int y, const std::function<void(int, int)>& fn) const {
    const int n = std::max(1, brushSize);
    const int o = (n - 1) / 2;
    // Круг: клетки, чьи середины ближе n/2 − 1/4 к середине пятна, — у 3
    // это «плюс», у 4 — квадрат без углов, как у кистей пиксель-арта.
    const double c = (n - 1) / 2.0;
    const double r = n / 2.0 - 0.25;
    for (int j = 0; j < n; ++j) {
        for (int i = 0; i < n; ++i) {
            if (brushRound && n > 2 && (i - c) * (i - c) + (j - c) * (j - c) > r * r) continue;
            fn(x - o + i, y - o + j);
        }
    }
}

void Model::beginStroke() {
    strokeMask.resize(docW(), docH(), 0);
    // Новый мазок — новая раскладка смеси местностей.
    if (docMode == DOC_MAP) ++mapMixSeed;
    strokePts.clear();
    strokeActive = docMode == DOC_TILE;
    const bool stamp = toolTile == T_PEN && penMode == PEN_STAMP && !customBrush.empty();
    if (strokeActive && (pixelPerfect || stamp)) {
        ensureTileIdx();
        strokeBase = tile;
        strokeBaseIdx = tileIdx;
    }
}

void Model::endStroke() {
    strokeMask.clear();
    strokePts.clear();
    strokeBase.clear();
    strokeBaseIdx.clear();
    strokeActive = false;
}

// --- фигуры -----------------------------------------------------------------

void Model::shapeLine(int x0, int y0, int x1, int y1) {
    // Брезенхэм. Он же достраивает штрих, когда мышь двигают быстро.
    const int dx = std::abs(x1 - x0);
    const int dy = -std::abs(y1 - y0);
    const int sx = x0 < x1 ? 1 : -1;
    const int sy = y0 < y1 ? 1 : -1;
    int err = dx + dy;
    while (true) {
        emitPixel(x0, y0);
        if (x0 == x1 && y0 == y1) break;
        const int e2 = 2 * err;
        if (e2 >= dy) {
            err += dy;
            x0 += sx;
        }
        if (e2 <= dx) {
            err += dx;
            y0 += sy;
        }
    }
}

void Model::shapeRect(int x0, int y0, int x1, int y1, bool filled) {
    const int lx = std::min(x0, x1);
    const int hx = std::max(x0, x1);
    const int ly = std::min(y0, y1);
    const int hy = std::max(y0, y1);
    for (int y = ly; y <= hy; ++y) {
        for (int x = lx; x <= hx; ++x) {
            if (filled || x == lx || x == hx || y == ly || y == hy) emitPixel(x, y);
        }
    }
}

void Model::shapeEllipse(int x0, int y0, int x1, int y1, bool filled) {
    // Принадлежность — по уравнению, контур — точки внутри, у которых хоть
    // один сосед снаружи. Так у вырожденных рамок нет особых случаев.
    const int lx = std::min(x0, x1);
    const int hx = std::max(x0, x1);
    const int ly = std::min(y0, y1);
    const int hy = std::max(y0, y1);
    const double cx = (lx + hx) / 2.0;
    const double cy = (ly + hy) / 2.0;
    const double ra = (hx - lx) / 2.0 + 0.5;
    const double rb = (hy - ly) / 2.0 + 0.5;
    auto inside = [&](int x, int y) {
        const double ddx = (x - cx) / ra;
        const double ddy = (y - cy) / rb;
        return ddx * ddx + ddy * ddy <= 1.0;
    };
    for (int y = ly; y <= hy; ++y) {
        for (int x = lx; x <= hx; ++x) {
            if (!inside(x, y)) continue;
            if (filled || !inside(x - 1, y) || !inside(x + 1, y) || !inside(x, y - 1) ||
                !inside(x, y + 1)) {
                emitPixel(x, y);
            }
        }
    }
}

void Model::squarifyTo(int ax, int ay, int& bx, int& by) {
    const int side = std::max(std::abs(bx - ax), std::abs(by - ay));
    bx = bx >= ax ? ax + side : ax - side;
    by = by >= ay ? ay + side : ay - side;
}

// --- заливка ----------------------------------------------------------------

void Model::floodFillTile(int sx, int sy, Color newColor) {
    if (!tile.inside(sx, sy)) return;
    const bool clip = clipToSelection && hasSelection;
    if (clip && !isSelected(sx, sy)) return;
    if (!activeLayerEditable()) return;
    const Color target = tile.at(sx, sy);
    if (sameColor(target, newColor)) return;
    // Привязка одна на всю заливку: палитра просматривается раз.
    const PalIndex ni = indexOfColor(newColor);
    // Посещённые клетки — в отдельном массиве: изображение по ходу не
    // портится служебными цветами.
    Grid<std::uint8_t> visited(tile.w, tile.h, 0);
    std::vector<int> qx(static_cast<std::size_t>(tile.w) * static_cast<std::size_t>(tile.h));
    std::vector<int> qy(qx.size());
    std::size_t head = 0;
    std::size_t tail = 0;
    auto push = [&](int x, int y) {
        if (!tile.inside(x, y) || visited.at(x, y) != 0) return;
        // Граница выделения для заливки — такая же стена, как чужой цвет.
        if (clip && !isSelected(x, y)) return;
        if (!sameColor(tile.at(x, y), target)) return;
        visited.at(x, y) = 1;
        qx[tail] = x;
        qy[tail] = y;
        ++tail;
    };
    push(sx, sy);
    while (head < tail) {
        const int x = qx[head];
        const int y = qy[head];
        ++head;
        tile.at(x, y) = newColor;
        setIdx(x, y, ni);
        push(x + 1, y);
        push(x - 1, y);
        push(x, y + 1);
        push(x, y - 1);
    }
}

void Model::floodFillMap(int sx, int sy, int newCode) {
    if (!map.inside(sx, sy)) return;
    const int target = map.at(sx, sy);
    // Смесь может положить и ту же местность — заливать есть что.
    const bool mix = mapMix && newCode == mapBrush;
    if (target == newCode && !mix) return;
    if (mix) ++mapMixSeed;
    Grid<std::uint8_t> visited(map.w, map.h, 0);
    std::vector<int> qx(static_cast<std::size_t>(map.w) * static_cast<std::size_t>(map.h));
    std::vector<int> qy(qx.size());
    std::size_t head = 0;
    std::size_t tail = 0;
    auto push = [&](int x, int y) {
        if (!map.inside(x, y) || visited.at(x, y) != 0) return;
        if (map.at(x, y) != target) return;
        visited.at(x, y) = 1;
        qx[tail] = x;
        qy[tail] = y;
        ++tail;
    };
    push(sx, sy);
    while (head < tail) {
        const int x = qx[head];
        const int y = qy[head];
        ++head;
        map.at(x, y) = static_cast<std::uint8_t>(mix ? mapBrushAt(x, y) : newCode);
        push(x + 1, y);
        push(x - 1, y);
        push(x, y + 1);
        push(x, y - 1);
    }
}

// --- выделение и буферы -----------------------------------------------------

void Model::copySelectionToBuffer() {
    if (!hasSelection || selW < 1 || selH < 1) return;
    if (docMode == DOC_MAP) {
        mapBuffer.resize(selW, selH);
        for (int i = 0; i < selH; ++i) {
            for (int j = 0; j < selW; ++j)
                mapBuffer.at(j, i) = map.at(selX + j, selY + i);
        }
    } else {
        buffer.resize(selW, selH);
        for (int i = 0; i < selH; ++i) {
            for (int j = 0; j < selW; ++j)
                buffer.at(j, i) = tile.at(selX + j, selY + i);
        }
    }
}

void Model::pasteBufferAt(int px, int py) {
    // Выходящая за край часть отбрасывается.
    if (docMode == DOC_MAP) {
        for (int i = 0; i < mapBuffer.h; ++i) {
            for (int j = 0; j < mapBuffer.w; ++j) {
                if (map.inside(px + j, py + i)) map.at(px + j, py + i) = mapBuffer.at(j, i);
            }
        }
    } else {
        for (int i = 0; i < buffer.h; ++i) {
            for (int j = 0; j < buffer.w; ++j) {
                if (!tile.inside(px + j, py + i) || buffer.at(j, i).a == 0) continue;
                tile.at(px + j, py + i) = buffer.at(j, i);
                // Буфер хранит только цвета: привязка выводится заново.
                setIdx(px + j, py + i, indexOfColor(buffer.at(j, i)));
            }
        }
        tileBmpDirty = true;
    }
}

bool Model::bufferFilled() const {
    return docMode == DOC_MAP ? !mapBuffer.empty() : !buffer.empty();
}

void Model::bufferFlipH() {
    if (buffer.empty()) return;
    Grid<Color> t(buffer.w, buffer.h);
    for (int i = 0; i < buffer.h; ++i) {
        for (int j = 0; j < buffer.w; ++j)
            t.at(j, i) = buffer.at(buffer.w - 1 - j, i);
    }
    buffer = std::move(t);
}

void Model::bufferFlipV() {
    if (buffer.empty()) return;
    Grid<Color> t(buffer.w, buffer.h);
    for (int i = 0; i < buffer.h; ++i) {
        for (int j = 0; j < buffer.w; ++j)
            t.at(j, i) = buffer.at(j, buffer.h - 1 - i);
    }
    buffer = std::move(t);
}

void Model::bufferRotate90() {
    // По часовой стрелке. Стороны меняются местами, поэтому в выделение
    // результат не возвращается: его вставляют вручную.
    if (buffer.empty()) return;
    Grid<Color> t(buffer.h, buffer.w);
    for (int i = 0; i < buffer.h; ++i) {
        for (int j = 0; j < buffer.w; ++j)
            t.at(buffer.h - 1 - i, j) = buffer.at(j, i);
    }
    buffer = std::move(t);
}

bool Model::applyBufferToSelection() {
    if (!hasSelection) return false;
    if (buffer.w != selW || buffer.h != selH) return false;
    // Оригинал писал в тайл мимо истории и не помечал картинку холста
    // устаревшей: отражение выделения нельзя было отменить, а на экране
    // оно появлялось только после следующей правки.
    pushUndo();
    for (int i = 0; i < selH; ++i) {
        for (int j = 0; j < selW; ++j) {
            tile.at(selX + j, selY + i) = buffer.at(j, i);
            setIdx(selX + j, selY + i, indexOfColor(buffer.at(j, i)));
        }
    }
    tileBmpDirty = true;
    return true;
}

// --- операции над тайлом ----------------------------------------------------

void Model::clearTile() {
    pushUndo();
    ensureTileIdx();
    const Color blank = blankColor();
    // Прозрачное ни к какой ячейке не привязано — как после ластика.
    const PalIndex ni = blank.a == 0 ? IDX_NONE : indexOfColor(blank);
    std::fill(tile.d.begin(), tile.d.end(), blank);
    std::fill(tileIdx.d.begin(), tileIdx.d.end(), ni);
    needRepaint = true;
}

void Model::randomizeTile() {
    pushUndo();
    ensureTileIdx();
    // Random(a, b) в PascalABC.NET — включительно с обеих сторон,
    // Random(n) — от 0 до n-1.
    auto rnd = [this](int lo, int hi) { return std::uniform_int_distribution<int>(lo, hi)(rng); };
    for (int i = 0; i < tile.h; ++i) {
        for (int j = 0; j < tile.w; ++j) {
            if (rnd(0, 3) == 0) {
                tile.at(j, i) = rgb(128 + rnd(0, 63), 128 + rnd(0, 63), 128 + rnd(0, 63));
            } else {
                tile.at(j, i) = rgb(64 + rnd(0, 127), 64 + rnd(0, 127), 64 + rnd(0, 127));
            }
            // Случайный цвет палитре не принадлежит.
            tileIdx.at(j, i) = IDX_NONE;
        }
    }
    needRepaint = true;
}

int Model::replaceColorEverywhere(Color fromC, Color toC) {
    int n = 0;
    ensureTileIdx();
    const PalIndex ni = indexOfColor(toC);
    for (int i = 0; i < tile.h; ++i) {
        for (int j = 0; j < tile.w; ++j) {
            if (sameColor(tile.at(j, i), fromC)) {
                tile.at(j, i) = toC;
                tileIdx.at(j, i) = ni;
                ++n;
            }
        }
    }
    tileBmpDirty = true;
    return n;
}

PalIndex Model::nearestPaletteIndex(Color c) const {
    PalIndex res = IDX_NONE;
    int best = -1;
    for (std::size_t k = 0; k < palette.size(); ++k) {
        const int dr = c.r - palette[k].r;
        const int dg = c.g - palette[k].g;
        const int db = c.b - palette[k].b;
        const int d = dr * dr + dg * dg + db * db;
        if (best < 0 || d < best) {
            best = d;
            res = static_cast<PalIndex>(k);
        }
    }
    return res;
}

Color Model::nearestPaletteColor(Color c) const {
    const PalIndex idx = nearestPaletteIndex(c);
    if (idx == IDX_NONE) return c;
    const Color p = palColorOf(idx);
    return argb(c.a, p.r, p.g, p.b);
}

void Model::quantizeTileToPalette() {
    // После этого палитра управляет всем тайлом целиком.
    ensureTileIdx();
    for (int i = 0; i < tile.h; ++i) {
        for (int j = 0; j < tile.w; ++j) {
            if (tile.at(j, i).a == 0) continue;
            const PalIndex idx = nearestPaletteIndex(tile.at(j, i));
            if (idx == IDX_NONE) continue;
            const Color p = palColorOf(idx);
            tile.at(j, i) = argb(tile.at(j, i).a, p.r, p.g, p.b);
            tileIdx.at(j, i) = idx;
        }
    }
    tileBmpDirty = true;
}

void Model::flipTileH() {
    ensureTileIdx();
    Grid<Color> t(tile.w, tile.h);
    Grid<PalIndex> n(tile.w, tile.h);
    for (int i = 0; i < tile.h; ++i) {
        for (int j = 0; j < tile.w; ++j) {
            t.at(j, i) = tile.at(tile.w - 1 - j, i);
            n.at(j, i) = tileIdx.at(tile.w - 1 - j, i);
        }
    }
    tile = std::move(t);
    tileIdx = std::move(n);
    tileBmpDirty = true;
}

void Model::flipTileV() {
    ensureTileIdx();
    Grid<Color> t(tile.w, tile.h);
    Grid<PalIndex> n(tile.w, tile.h);
    for (int i = 0; i < tile.h; ++i) {
        for (int j = 0; j < tile.w; ++j) {
            t.at(j, i) = tile.at(j, tile.h - 1 - i);
            n.at(j, i) = tileIdx.at(j, tile.h - 1 - i);
        }
    }
    tile = std::move(t);
    tileIdx = std::move(n);
    tileBmpDirty = true;
}

// --- рабочие цвета ----------------------------------------------------------

void Model::syncHSVFromColor() {
    // У серого тона нет: при насыщенности ноль ползунок тона остаётся.
    const HSVColor c = colorToHsv(firstColor);
    if (c.s > 0) hue = c.h;
    sat = c.s;
    val = c.v;
}

void Model::applyHSVToColor() {
    const Color c = hsvToRgb(hue, sat, val);
    firstColor = argb(firstColor.a, c.r, c.g, c.b);
    // Набранный ползунками цвет связан с палитрой, только если совпал с
    // какой-то ячейкой.
    firstIdx = indexOfColor(firstColor);
}

void Model::pickColor(Color c) {
    firstColor = c;
    firstIdx = indexOfColor(c);
    syncHSVFromColor();
    needRepaint = true;
}

void Model::pickSecondFromPalette(PalIndex idx) {
    secondColor = effectiveColor(idx);
    secondIdx = idx;
    needRepaint = true;
}

void Model::pickColorFromTile(int x, int y) {
    // Вместе с цветом берётся и привязка: взятый цвет продолжает
    // управляться той же ячейкой, даже если цвет стоит в палитре дважды.
    if (!tile.inside(x, y)) return;
    pickColor(tile.at(x, y));
    if (!tileIdx.sameSize(tile.w, tile.h)) return;
    if (tileIdx.at(x, y) == IDX_NONE) return;
    if (sameRGB(palColorOf(tileIdx.at(x, y)), firstColor)) firstIdx = tileIdx.at(x, y);
}

void Model::swapColors() {
    std::swap(firstColor, secondColor);
    std::swap(firstIdx, secondIdx);
    syncHSVFromColor();
    needRepaint = true;
}

void Model::resetColors() {
    firstColor = argb(255, 0, 0, 0);
    secondColor = argb(255, 255, 255, 255);
    firstIdx = indexOfColor(firstColor);
    secondIdx = indexOfColor(secondColor);
    syncHSVFromColor();
    needRepaint = true;
}

// --- геометрия панели -------------------------------------------------------

int Model::toolCount() const {
    return docMode == DOC_MAP ? T_MAP_COUNT : T_TILE_COUNT;
}

int Model::curTool() const {
    return docMode == DOC_MAP ? toolMap : toolTile;
}

void Model::setTool(int i) {
    if (docMode == DOC_MAP) {
        toolMap = clampI(i, 0, T_MAP_COUNT - 1);
    } else {
        toolTile = clampI(i, 0, T_TILE_COUNT - 1);
    }
    needRepaint = true;
}

int Model::toolCellX(int i) {
    return PANEL_X + 2 + (i % TOOL_COLS) * TOOL_CW;
}

int Model::toolCellY(int i) {
    return TOOLS_Y0 + (i / TOOL_COLS) * TOOL_CH;
}

bool Model::toolHit(int px, int py, int& idx) const {
    for (int i = 0; i < toolCount(); ++i) {
        if (px >= toolCellX(i) && px < toolCellX(i) + TOOL_CW - 2 && py >= toolCellY(i) &&
            py < toolCellY(i) + TOOL_CH - 2) {
            idx = i;
            return true;
        }
    }
    return false;
}

bool Model::palHit(int px, int py, int& cj, int& ci) {
    for (int i = 0; i < PAL_ROWS; ++i) {
        for (int j = 0; j < PAL_COLS; ++j) {
            if (px >= palCellX(j) && px < palCellX(j + 1) && py >= palCellY(i) &&
                py < palCellY(i + 1)) {
                cj = j;
                ci = i;
                return true;
            }
        }
    }
    return false;
}

bool Model::terrHit(int px, int py, int& idx) {
    for (int i = 0; i < MT_COUNT; ++i) {
        if (px >= mselX(i) && px < mselX(i) + MSEL_CW - 2 && py >= terrCellY(i) &&
            py < terrCellY(i) + MTERR_CH - 2) {
            idx = i;
            return true;
        }
    }
    return false;
}

bool Model::objHit(int px, int py, int& idx) {
    for (int i = 0; i < mapKindCount(); ++i) {
        if (px >= mselX(i) && px < mselX(i) + MSEL_CW - 2 && py >= objCellY(i) &&
            py < objCellY(i) + MOBJ_CH - 2) {
            idx = i;
            return true;
        }
    }
    return false;
}

// --- обзор ------------------------------------------------------------------

double Model::overScale() const {
    double r = std::min(static_cast<double>(OVER_W) / std::max(1, docW()),
                        static_cast<double>(OVER_H) / std::max(1, docH()));
    if (r < 0.02) r = 0.02;
    return r;
}

int Model::overPixW() const {
    return clampI(pround(docW() * overScale()), 1, OVER_W);
}

int Model::overPixH() const {
    return clampI(pround(docH() * overScale()), 1, OVER_H);
}

int Model::overOriginX() const {
    return PANEL_X + 2 + (OVER_W - overPixW()) / 2;
}

int Model::overOriginY() const {
    return OVER_Y0 + (OVER_H - overPixH()) / 2;
}

void Model::overviewCenterOn(int px, int py) {
    // Центр холста — на клетку документа, отвечающую точке обзора.
    const double docX = static_cast<double>((px - overOriginX()) * docW()) / overPixW();
    const double docY = static_cast<double>((py - overOriginY()) * docH()) / overPixH();
    viewX = pround((X0 + W) / 2.0 - docX * scale());
    viewY = pround((Y0 + H) / 2.0 - docY * scale());
    clampView();
    needRepaint = true;
}

std::vector<std::uint8_t> Model::buildOverviewRaw() const {
    const int pw = overPixW();
    const int ph = overPixH();
    std::vector<std::uint8_t> raw;
    if (pw < 1 || ph < 1 || docW() < 1 || docH() < 1) return raw;
    raw.resize(static_cast<std::size_t>(pw) * static_cast<std::size_t>(ph) * 4);
    for (int y = 0; y < ph; ++y) {
        for (int x = 0; x < pw; ++x) {
            const int sx = clampI(x * docW() / pw, 0, docW() - 1);
            const int sy = clampI(y * docH() / ph, 0, docH() - 1);
            const Color c =
                docMode == DOC_MAP
                    ? mapTileColor(map.at(sx, sy))
                    : blendOver(multiCel() ? compositeAt(activeFrame, sx, sy) : tile.at(sx, sy),
                                checkerColor(sx, sy));
            putBgra(raw,
                    (static_cast<std::size_t>(y) * static_cast<std::size_t>(pw) +
                     static_cast<std::size_t>(x)) *
                        4,
                    c, 255);
        }
    }
    return raw;
}

bool Model::overviewViewport(int& vx0, int& vy0, int& vx1, int& vy1) const {
    if (docW() <= 0 || docH() <= 0 || pixelSize <= 0) return false;
    const int ox = overOriginX();
    const int oy = overOriginY();
    const int pw = overPixW();
    const int ph = overPixH();
    vx0 = ox + pround((static_cast<double>(X0 - viewX) / scale()) * pw / docW());
    vy0 = oy + pround((static_cast<double>(Y0 - viewY) / scale()) * ph / docH());
    vx1 = ox + pround((static_cast<double>(-viewX + W) / scale()) * pw / docW());
    vy1 = oy + pround((static_cast<double>(-viewY + H) / scale()) * ph / docH());
    vx0 = clampI(vx0, ox, ox + pw);
    vy0 = clampI(vy0, oy, oy + ph);
    vx1 = clampI(vx1, ox, ox + pw);
    vy1 = clampI(vy1, oy, oy + ph);
    return vx1 > vx0 && vy1 > vy0;
}

std::vector<std::uint8_t> Model::tileBmpRaw() const {
    std::vector<std::uint8_t> raw;
    if (tile.empty()) return raw;
    raw.resize(static_cast<std::size_t>(tile.w) * static_cast<std::size_t>(tile.h) * 4);
    for (int i = 0; i < tile.h; ++i) {
        for (int j = 0; j < tile.w; ++j) {
            const Color c = displayColor(j, i);
            putBgra(raw, tile.index(j, i) * 4, c, c.a);
        }
    }
    return raw;
}

// --- карта и набор тайлов ---------------------------------------------------

std::string Model::mapCellTileFile(int x, int y) const {
    if (!map.inside(x, y)) return {};
    const int idx = map.objectAt(x, y);
    if (idx >= 0) {
        const MapObject& o = map.objects[static_cast<std::size_t>(idx)];
        // Первое слово хвоста строки — id существа; дальше числа зоны и
        // количества, они здесь не нужны.
        std::string id = netTrim(o.rest);
        const std::size_t sp = id.find(' ');
        if (sp != std::string::npos) id.resize(sp);
        if (o.kind == "npc" && !id.empty()) return "npc_" + id;
        if (o.kind == "spawn" && !id.empty()) return "mob_" + id;
        if (o.kind == "npc") return "npc";
        if (o.kind == "spawn") return "mob";
        return o.kind;
    }
    // Коды клеток идут в том же порядке, что основные слоты набора.
    return tsetBaseFile(map.at(x, y));
}

}  // namespace tjtm
