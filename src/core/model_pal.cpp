// Команды этапа 2 RoadToProLevel: палитра произвольного размера и строгий
// индексный режим.
//
// Любая перестановка ячеек — добавить, удалить, сдвинуть, отсортировать,
// вставить ряд — сводится к remapPalette: новая палитра и таблица «прежний
// номер → новый». Так привязки точек не разъезжаются ни в одной из команд.

#include <algorithm>
#include <numeric>

#include "core/imageops.h"
#include "core/model.h"
#include "core/palette.h"
#include "core/pmath.h"

namespace tjtm {

namespace {

constexpr Color kClear = argb(0, 0, 0, 0);

std::vector<int> identity(int n) {
    std::vector<int> v(static_cast<std::size_t>(std::max(0, n)));
    std::iota(v.begin(), v.end(), 0);
    return v;
}

/// Таблица «прежний номер → новый» по порядку: order[k] — прежний номер
/// ячейки, вставшей на место k.
std::vector<int> inverse(const std::vector<int>& order, int oldCount) {
    std::vector<int> map(static_cast<std::size_t>(oldCount), -1);
    for (std::size_t k = 0; k < order.size(); ++k) {
        if (order[k] >= 0 && order[k] < oldCount)
            map[static_cast<std::size_t>(order[k])] = static_cast<int>(k);
    }
    return map;
}

}  // namespace

// --- панель -----------------------------------------------------------------

int Model::palCellIndex(int j, int i) const {
    const int k = (i + palScroll) * PAL_COLS + j;
    return (j >= 0 && j < PAL_COLS && k >= 0 && k < palCount()) ? k : -1;
}

int Model::palMaxScroll() const {
    const int rows = (palCount() + PAL_COLS - 1) / PAL_COLS;
    return std::max(0, rows - PAL_ROWS);
}

void Model::scrollPalette(int rows) {
    palScroll = clampI(palScroll + rows, 0, palMaxScroll());
    needRepaint = true;
}

// --- номера и цвета ---------------------------------------------------------

Color Model::effectiveColor(PalIndex idx) const {
    if (idx == IDX_NONE || idx >= palette.size()) return kClear;
    const Color c = palette[idx];
    return idx == transparentIdx ? argb(0, c.r, c.g, c.b) : c;
}

PalIndex Model::resolveIndex(Color c, PalIndex bound) const {
    if (bound != IDX_NONE && bound < palette.size()) return bound;
    if (c.a == 0) return transparentIdx;
    const int k = pal::nearest(palette, c);
    return k < 0 ? IDX_NONE : static_cast<PalIndex>(k);
}

Color Model::fillColor() const {
    if (!strictIndexedOn()) return firstColor;
    return effectiveColor(resolveIndex(firstColor, firstIdx));
}

// --- перестановки -----------------------------------------------------------

void Model::remapPalette(std::vector<Color> newPal, const std::vector<int>& oldToNew) {
    if (newPal.empty()) return;
    ensureTileIdx();
    const bool strict = strictIndexedOn();
    auto newIndex = [&](PalIndex v) -> int {
        if (v == IDX_NONE || v >= oldToNew.size()) return -1;
        return oldToNew[v];
    };
    // Прозрачный номер — первым: от него зависят цвета точек ниже.
    const int nt = newIndex(transparentIdx);
    transparentIdx = nt < 0 ? IDX_NONE : static_cast<PalIndex>(nt);
    palette = std::move(newPal);
    // Номера одни на весь документ: переставляются во всех слоях и кадрах.
    forEachCel([&](Grid<Color>& img, Grid<PalIndex>& idx) {
        for (std::size_t k = 0; k < img.d.size(); ++k) {
            const PalIndex v = idx.d[k];
            if (v == IDX_NONE) continue;
            const int nv = newIndex(v);
            if (nv >= 0) {
                idx.d[k] = static_cast<PalIndex>(nv);
            } else if (strict) {
                // Ячейки больше нет: в строгом режиме точка берёт ближайшую.
                idx.d[k] = resolveIndex(img.d[k], IDX_NONE);
            } else {
                idx.d[k] = IDX_NONE;
            }
            if (strict) img.d[k] = effectiveColor(idx.d[k]);
        }
    });
    auto remapWorking = [&](PalIndex& idx, Color& c) {
        const int n = newIndex(idx);
        if (n >= 0) {
            idx = static_cast<PalIndex>(n);
        } else {
            idx = IDX_NONE;
            if (strict) idx = resolveIndex(c, IDX_NONE);
        }
        if (strict && idx != IDX_NONE) {
            const Color p = palette[idx];
            c = argb(c.a, p.r, p.g, p.b);
        }
    };
    remapWorking(firstIdx, firstColor);
    remapWorking(secondIdx, secondColor);
    palScroll = clampI(palScroll, 0, palMaxScroll());
    tileBmpDirty = true;
    overDirty = true;
    needRepaint = true;
}

void Model::setPalette(std::vector<Color> p) {
    if (p.empty()) return;
    if (static_cast<int>(p.size()) > pal::MAX_PALETTE) p.resize(pal::MAX_PALETTE);
    palette = std::move(p);
    if (transparentIdx != IDX_NONE && transparentIdx >= palette.size()) transparentIdx = IDX_NONE;
    if (!tile.empty()) rebindTileToPalette();
    // Привязка рабочего цвета держится, пока ячейка того же цвета; новую
    // не выводим — так поступал и оригинал при загрузке палитры.
    auto keep = [this](PalIndex& idx, Color c) {
        if (idx != IDX_NONE && (idx >= palette.size() || !sameRGB(palette[idx], c))) idx = IDX_NONE;
    };
    keep(firstIdx, firstColor);
    keep(secondIdx, secondColor);
    enforceIndexed();
    palScroll = clampI(palScroll, 0, palMaxScroll());
    tileBmpDirty = true;
    overDirty = true;
    needRepaint = true;
}

int Model::addPaletteColor(Color c, int at) {
    if (docMode != DOC_TILE || palCount() >= pal::MAX_PALETTE) return -1;
    at = clampI(at < 0 ? palCount() : at, 0, palCount());
    pushUndo();
    std::vector<Color> np = palette;
    np.insert(np.begin() + at, c);
    std::vector<int> remap(palette.size());
    for (int k = 0; k < palCount(); ++k)
        remap[static_cast<std::size_t>(k)] = k < at ? k : k + 1;
    remapPalette(std::move(np), remap);
    return at;
}

bool Model::deletePaletteEntry(int idx) {
    if (docMode != DOC_TILE || idx < 0 || idx >= palCount() || palCount() <= 1) return false;
    pushUndo();
    std::vector<Color> np = palette;
    np.erase(np.begin() + idx);
    std::vector<int> remap(palette.size());
    for (int k = 0; k < palCount(); ++k)
        remap[static_cast<std::size_t>(k)] = k < idx ? k : (k == idx ? -1 : k - 1);
    remapPalette(std::move(np), remap);
    return true;
}

bool Model::duplicatePaletteEntry(int idx) {
    if (idx < 0 || idx >= palCount()) return false;
    return addPaletteColor(palette[static_cast<std::size_t>(idx)], idx + 1) >= 0;
}

bool Model::movePaletteEntry(int idx, int to) {
    if (docMode != DOC_TILE || idx < 0 || idx >= palCount()) return false;
    to = clampI(to, 0, palCount() - 1);
    if (to == idx) return false;
    pushUndo();
    std::vector<int> order = identity(palCount());
    order.erase(order.begin() + idx);
    order.insert(order.begin() + to, idx);
    std::vector<Color> np;
    np.reserve(order.size());
    for (const int k : order)
        np.push_back(palette[static_cast<std::size_t>(k)]);
    remapPalette(std::move(np), inverse(order, palCount()));
    return true;
}

void Model::sortPalette(int key, int from, int to) {
    if (docMode != DOC_TILE || palCount() < 2) return;
    from = clampI(from, 0, palCount() - 1);
    to = clampI(to, 0, palCount() - 1);
    if (from > to) std::swap(from, to);
    if (from == to) {
        from = 0;
        to = palCount() - 1;
    }
    pushUndo();
    std::vector<int> order = identity(palCount());
    const std::vector<int> part = pal::sortOrder(palette, key, from, to);
    std::copy(part.begin(), part.end(), order.begin() + from);
    std::vector<Color> np;
    np.reserve(order.size());
    for (const int k : order)
        np.push_back(palette[static_cast<std::size_t>(k)]);
    remapPalette(std::move(np), inverse(order, palCount()));
}

int Model::insertRamp(int a, int b, int steps, int hueShift) {
    if (docMode != DOC_TILE || a < 0 || b < 0 || a >= palCount() || b >= palCount() || steps < 3)
        return 0;
    const std::vector<Color> r = pal::ramp(palette[static_cast<std::size_t>(a)],
                                           palette[static_cast<std::size_t>(b)], steps, hueShift);
    const int add = std::min(static_cast<int>(r.size()) - 2, pal::MAX_PALETTE - palCount());
    if (add <= 0) return 0;
    pushUndo();
    std::vector<Color> np = palette;
    np.insert(np.begin() + a + 1, r.begin() + 1, r.begin() + 1 + add);
    std::vector<int> remap(palette.size());
    for (int k = 0; k < palCount(); ++k)
        remap[static_cast<std::size_t>(k)] = k <= a ? k : k + add;
    remapPalette(std::move(np), remap);
    return add;
}

void Model::adjustPaletteEntries(int from, int to, int dh, int ds, int dv) {
    if (docMode != DOC_TILE || palCount() == 0) return;
    from = clampI(from, 0, palCount() - 1);
    to = clampI(to, 0, palCount() - 1);
    if (from > to) std::swap(from, to);
    pushUndo();
    for (int k = from; k <= to; ++k) {
        const Color old = palette[static_cast<std::size_t>(k)];
        const Color now = pal::adjustHSV(old, dh, ds, dv);
        palette[static_cast<std::size_t>(k)] = now;
        // Точки ячейки перекрашиваются вместе с ней — так меняют тон целого
        // набора тайлов одной правкой палитры, во всех слоях и кадрах.
        const auto pk = static_cast<PalIndex>(k);
        forEachCel([&](Grid<Color>& img, const Grid<PalIndex>& idx) {
            for (std::size_t q = 0; q < img.d.size(); ++q) {
                if (idx.d[q] == pk && sameRGB(img.d[q], old))
                    img.d[q] = argb(img.d[q].a, now.r, now.g, now.b);
            }
        });
        if (firstIdx == k) firstColor = argb(firstColor.a, now.r, now.g, now.b);
        if (secondIdx == k) secondColor = argb(secondColor.a, now.r, now.g, now.b);
    }
    enforceIndexed();
    tileBmpDirty = true;
    overDirty = true;
    needRepaint = true;
}

int Model::extractPaletteFromTile(int maxColors) {
    if (docMode != DOC_TILE || tile.empty()) return 0;
    const Grid<Color> src = hasSelection ? selectionContent() : tile;
    std::vector<Color> p = pal::extract(src, clampI(maxColors, 1, pal::MAX_PALETTE));
    if (p.empty()) return 0;
    pushUndo();
    transparentIdx = IDX_NONE;
    const int n = static_cast<int>(p.size());
    setPalette(std::move(p));
    return n;
}

int Model::removeDuplicateEntries() {
    if (docMode != DOC_TILE) return 0;
    std::vector<Color> np;
    std::vector<int> remap(palette.size(), -1);
    for (std::size_t k = 0; k < palette.size(); ++k) {
        int found = -1;
        for (std::size_t q = 0; q < np.size(); ++q) {
            if (np[q] == palette[k]) {
                found = static_cast<int>(q);
                break;
            }
        }
        if (found < 0) {
            found = static_cast<int>(np.size());
            np.push_back(palette[k]);
        }
        remap[k] = found;
    }
    const int removed = palCount() - static_cast<int>(np.size());
    if (removed == 0) return 0;
    pushUndo();
    remapPalette(std::move(np), remap);
    return removed;
}

void Model::setTransparentIndex(PalIndex idx) {
    if (docMode != DOC_TILE) return;
    if (idx != IDX_NONE && idx >= palette.size()) return;
    if (idx == transparentIdx) return;
    pushUndo();
    transparentIdx = idx;
    // Точки прежнего прозрачного номера снова видны, нового — пропадают.
    if (indexedOn) {
        forEachCel([this](Grid<Color>& img, const Grid<PalIndex>& cel) {
            for (std::size_t k = 0; k < img.d.size(); ++k) {
                if (cel.d[k] != IDX_NONE) img.d[k] = effectiveColor(cel.d[k]);
            }
        });
    }
    tileBmpDirty = true;
    overDirty = true;
    needRepaint = true;
}

int Model::unmatchedPixelCount() const {
    int n = 0;
    forEachCelConst([&](const Grid<Color>& g) {
        for (const Color c : g.d) {
            if (c.a != 0 && indexOfColor(c) == IDX_NONE) ++n;
        }
    });
    return n;
}

int Model::enableIndexed(bool addMissing) {
    if (docMode != DOC_TILE) return 0;
    pushUndo();
    if (addMissing) {
        std::vector<Color> np = palette;
        forEachCelConst([&](const Grid<Color>& g) {
            for (const Color c : g.d) {
                if (c.a == 0 || static_cast<int>(np.size()) >= pal::MAX_PALETTE) continue;
                const bool have =
                    std::any_of(np.begin(), np.end(), [&](Color p) { return sameRGB(p, c); });
                if (!have) np.push_back(argb(255, c.r, c.g, c.b));
            }
        });
        palette = std::move(np);
    }
    indexedOn = true;
    // Привязка и сведение к палитре — во всех слоях и кадрах.
    int changed = 0;
    forEachCel([&](Grid<Color>& img, Grid<PalIndex>& idx) {
        for (std::size_t k = 0; k < img.d.size(); ++k)
            idx.d[k] = indexOfColor(img.d[k]);
        changed += enforceGrid(img, idx);
    });
    firstIdx = resolveIndex(firstColor, indexOfColor(firstColor));
    secondIdx = resolveIndex(secondColor, indexOfColor(secondColor));
    tileBmpDirty = true;
    overDirty = true;
    needRepaint = true;
    return changed;
}

int Model::enforceGrid(Grid<Color>& img, Grid<PalIndex>& idx) const {
    if (palette.empty()) return 0;
    if (!idx.sameSize(img.w, img.h)) idx.resize(img.w, img.h, IDX_NONE);
    int changed = 0;
    for (std::size_t k = 0; k < img.d.size(); ++k) {
        PalIndex v = idx.d[k];
        if (v == IDX_NONE || v >= palette.size()) {
            // Прозрачная точка без прозрачного номера остаётся свободной:
            // она ничего не рисует, и номер ей не нужен.
            if (img.d[k].a == 0 && transparentIdx == IDX_NONE) continue;
            v = resolveIndex(img.d[k], IDX_NONE);
            idx.d[k] = v;
        }
        const Color e = effectiveColor(v);
        if (img.d[k] != e) {
            img.d[k] = e;
            ++changed;
        }
    }
    return changed;
}

void Model::enforceIndexed() {
    if (!strictIndexedOn() || tile.empty() || palette.empty()) return;
    if (activeKind() == LK_REFERENCE) return;
    ensureTileIdx();
    if (enforceGrid(tile, tileIdx) > 0) {
        tileBmpDirty = true;
        overDirty = true;
    }
}

void Model::adjustImageHSV(int dh, int ds, int dv) {
    if (docMode != DOC_TILE || tile.empty()) return;
    pushUndo();
    for (int y = 0; y < tile.h; ++y) {
        for (int x = 0; x < tile.w; ++x) {
            if (hasSelection && !isSelected(x, y)) continue;
            Color& c = tile.at(x, y);
            if (c.a == 0) continue;
            c = pal::adjustHSV(c, dh, ds, dv);
            setIdx(x, y, strictIndexedOn() ? resolveIndex(c, IDX_NONE) : indexOfColor(c));
        }
    }
    enforceIndexed();
    tileBmpDirty = true;
    needRepaint = true;
}

int Model::reduceImageColors(int n) {
    if (docMode != DOC_TILE || tile.empty() || n < 1) return 0;
    // Цвета считаются только с тех точек, что меняются: выделенный кусок
    // сводится к своим n цветам, а не к цветам всего тайла.
    Grid<Color> sample(tile.w, tile.h, argb(0, 0, 0, 0));
    for (int y = 0; y < tile.h; ++y) {
        for (int x = 0; x < tile.w; ++x) {
            if (!hasSelection || isSelected(x, y)) sample.at(x, y) = tile.at(x, y);
        }
    }
    const std::vector<Color> target = pal::extract(sample, n);
    if (target.empty()) return 0;
    int changed = 0;
    Grid<Color> next = tile;
    for (std::size_t k = 0; k < next.d.size(); ++k) {
        const Color c = sample.d[k];
        if (c.a == 0) continue;
        const int i = pal::nearest(target, c);
        if (i < 0) continue;
        // Прозрачность точки остаётся своей: разрез идёт по цвету.
        const Color t = target[static_cast<std::size_t>(i)];
        const Color v = argb(c.a, t.r, t.g, t.b);
        if (v.argb() != next.d[k].argb()) {
            next.d[k] = v;
            ++changed;
        }
    }
    if (changed == 0) return 0;
    pushUndo();
    ensureTileIdx();
    for (std::size_t k = 0; k < next.d.size(); ++k) {
        if (next.d[k].argb() != tile.d[k].argb()) tileIdx.d[k] = indexOfColor(next.d[k]);
    }
    tile = std::move(next);
    enforceIndexed();
    tileBmpDirty = true;
    needRepaint = true;
    return changed;
}

}  // namespace tjtm
