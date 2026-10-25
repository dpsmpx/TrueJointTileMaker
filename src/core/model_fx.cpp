// Вне пиксель-арта в модели (этап 8 RoadToProLevel): фильтр над активной
// ячейкой, сведение итога к палитре и мазок аэрографа. Сами фильтры —
// src/core/filters.*, плавные масштаб и поворот — в model_pro.cpp рядом с
// пиксельными.

#include <algorithm>
#include <cmath>

#include "core/filters.h"
#include "core/imageops.h"
#include "core/model.h"
#include "core/palette.h"

namespace tjtm {

namespace {

constexpr Color kClear = argb(0, 0, 0, 0);

}  // namespace

void Model::snapStrict(Grid<Color>& next, const Grid<Color>& before) const {
    if (palette.empty()) return;
    const Color clear = transparentIdx != IDX_NONE ? effectiveColor(transparentIdx) : kClear;
    for (std::size_t k = 0; k < next.d.size(); ++k) {
        const Color was = k < before.d.size() ? before.d[k] : kClear;
        const Color c = next.d[k];
        if (c == was) continue;
        if (c.a < 128) {
            next.d[k] = was.a == 0 ? was : clear;
            continue;
        }
        const int i = pal::nearest(palette, argb(255, c.r, c.g, c.b));
        next.d[k] = i < 0 ? was : effectiveColor(static_cast<PalIndex>(i));
    }
}

Grid<Color> Model::filterResult(const std::function<Grid<Color>(const Grid<Color>&)>& f,
                                bool boxed) const {
    if (docMode != DOC_TILE || tile.empty() || !activeLayerEditable()) return {};
    // Вектор и тайлкарта тайлами перерисовываются из своих данных: фильтр
    // по их точкам пропал бы при первой правке.
    const bool pixels = activeKind() == LK_RASTER || (activeKind() == LK_TILEMAP && tilemapPixels);
    if (!pixels) return {};
    Grid<Color> next;
    if (boxed && hasSelection && selW > 0 && selH > 0) {
        const Grid<Color> part = img::crop(tile, selX, selY, selW, selH, kClear);
        const Grid<Color> done = f(part);
        if (!done.sameSize(part.w, part.h)) return {};
        next = tile;
        for (int y = 0; y < part.h; ++y) {
            for (int x = 0; x < part.w; ++x) {
                if (tile.inside(selX + x, selY + y)) next.at(selX + x, selY + y) = done.at(x, y);
            }
        }
    } else {
        next = f(tile);
    }
    if (!next.sameSize(tile.w, tile.h)) return {};
    if (hasSelection) {
        const Grid<std::uint8_t> within = selectionMask();
        for (std::size_t k = 0; k < next.d.size(); ++k) {
            if (within.d[k] == 0) next.d[k] = tile.d[k];
        }
    }
    if (strictIndexedOn()) snapStrict(next, tile);
    return next;
}

int Model::applyFilter(const std::function<Grid<Color>(const Grid<Color>&)>& f, bool boxed) {
    Grid<Color> next = filterResult(f, boxed);
    if (next.empty()) return 0;
    int changed = 0;
    for (std::size_t k = 0; k < next.d.size(); ++k)
        changed += next.d[k] != tile.d[k] ? 1 : 0;
    if (changed == 0) return 0;
    pushUndo();
    ensureTileIdx();
    for (std::size_t k = 0; k < next.d.size(); ++k) {
        if (next.d[k] == tile.d[k]) continue;
        tileIdx.d[k] =
            next.d[k].a == 0 && transparentIdx == IDX_NONE ? IDX_NONE : indexOfColor(next.d[k]);
    }
    tile = std::move(next);
    enforceIndexed();
    tileBmpDirty = true;
    overDirty = true;
    needRepaint = true;
    return changed;
}

// --- аэрограф ----------------------------------------------------------------

void Model::airbrushBegin(int x, int y) {
    ensureTileIdx();
    strokeBase = tile;
    strokeBaseIdx = tileIdx;
    airAcc = Grid<float>(tile.w, tile.h, 0.0F);
    airCarry = 0;
    airLastX = x;
    airLastY = y;
    strokeActive = true;
    forEachSymmetric(x, y, [this](int px, int py) { airbrushDab(px, py); });
}

void Model::airbrushTo(int x, int y) {
    const double dx = x - airLastX;
    const double dy = y - airLastY;
    const double len = std::hypot(dx, dy);
    if (len <= 0) return;
    // Шаг — четверть диаметра: отпечатки сливаются в ровную полосу, а
    // напор не зависит от того, как быстро ведут мышь.
    const double step = std::max(1.0, airSize / 4.0);
    double t = step - airCarry;
    double last = -1;
    while (t <= len + 1e-9) {
        const int px = static_cast<int>(std::lround(airLastX + dx * t / len));
        const int py = static_cast<int>(std::lround(airLastY + dy * t / len));
        forEachSymmetric(px, py, [this](int sx, int sy) { airbrushDab(sx, sy); });
        last = t;
        t += step;
    }
    // Остаток — путь от последнего отпечатка: следующий отрезок его учтёт.
    airCarry = last >= 0 ? len - last : airCarry + len;
    airLastX = x;
    airLastY = y;
}

void Model::airbrushDab(int cx, int cy) {
    if (!airAcc.sameSize(tile.w, tile.h) || !activeLayerEditable()) return;
    const Grid<float> dab = fx::softDab(airSize, airHardness);
    const float flow = static_cast<float>(std::clamp(airFlow, 1, 100)) / 100.0F;
    const int ox = cx - (dab.w - 1) / 2;
    const int oy = cy - (dab.h - 1) / 2;
    const bool strict = strictIndexedOn();
    const PalIndex brushIdx = strict ? resolveIndex(firstColor, firstIdx) : IDX_NONE;
    for (int j = 0; j < dab.h; ++j) {
        for (int i = 0; i < dab.w; ++i) {
            const float v = dab.at(i, j) * flow;
            if (v <= 0) continue;
            int x = ox + i;
            int y = oy + j;
            if (wrapOn) {
                x = ((x % tile.w) + tile.w) % tile.w;
                y = ((y % tile.h) + tile.h) % tile.h;
            }
            if (!tile.inside(x, y)) continue;
            if (clipToSelection && hasSelection && !isSelected(x, y)) continue;
            float& acc = airAcc.at(x, y);
            acc = 1.0F - (1.0F - acc) * (1.0F - v);
            if (strict) {
                // Смеси в строгом индексном режиме нет: точка становится
                // цветом кисти, когда напор перевалит за половину.
                if (acc < 0.5F) continue;
                tile.at(x, y) = effectiveColor(brushIdx);
                tileIdx.at(x, y) = brushIdx;
            } else {
                const Color c = firstColor;
                const auto a = static_cast<int>(std::lround(acc * static_cast<float>(c.a)));
                tile.at(x, y) = blendOver(argb(a, c.r, c.g, c.b), strokeBase.at(x, y));
                tileIdx.at(x, y) = indexOfColor(tile.at(x, y));
            }
            tileBmpDirty = true;
            if (liveDraw && drawTilePixel) drawTilePixel(x, y);
        }
    }
}

void Model::airbrushEnd() {
    airAcc.clear();
    strokeBase.clear();
    strokeBaseIdx.clear();
    strokeActive = false;
    enforceIndexed();
    overDirty = true;
    needRepaint = true;
}

}  // namespace tjtm
