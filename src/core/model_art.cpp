// Инструменты пиксель-арта сверх плана (этап 9): кисть из выделения,
// затенение по ряду палитры, градиент узором Байера и слой-образец.
//
// Все четыре держатся главного правила плана — у каждой операции
// предсказуемое пиксельное представление: кисть кладёт свои же точки,
// затенение и градиент берут цвета только из палитры и двух цветов кисти,
// а образец в рисунок не попадает вовсе.

#include <algorithm>
#include <cmath>

#include "core/filters.h"
#include "core/imageops.h"
#include "core/model.h"

namespace tjtm {

namespace {

constexpr Color kClear = argb(0, 0, 0, 0);

int luma(Color c) {
    return 299 * c.r + 587 * c.g + 114 * c.b;
}

/// Порог узора Байера 8x8 в точке: 0..63. Соседние пороги разнесены как
/// можно дальше, поэтому любая доля смеси выглядит ровной сеткой, а не
/// пятнами, как у случайного шума.
int bayer8(int x, int y) {
    static constexpr int kBayer[8][8] = {
        {0, 32, 8, 40, 2, 34, 10, 42},  {48, 16, 56, 24, 50, 18, 58, 26},
        {12, 44, 4, 36, 14, 46, 6, 38}, {60, 28, 52, 20, 62, 30, 54, 22},
        {3, 35, 11, 43, 1, 33, 9, 41},  {51, 19, 59, 27, 49, 17, 57, 25},
        {15, 47, 7, 39, 13, 45, 5, 37}, {63, 31, 55, 23, 61, 29, 53, 21}};
    return kBayer[y & 7][x & 7];
}

}  // namespace

// --- кисть из выделения ---------------------------------------------------------

bool Model::brushFromSelection() {
    if (docMode != DOC_TILE || !hasSelection || selW < 1 || selH < 1) return false;
    Grid<Color> img = selectionContent();
    if (std::none_of(img.d.begin(), img.d.end(), [](Color c) { return c.a != 0; })) return false;
    ensureTileIdx();
    Grid<PalIndex> idx(img.w, img.h, IDX_NONE);
    for (int y = 0; y < img.h; ++y) {
        for (int x = 0; x < img.w; ++x) {
            if (img.at(x, y).a != 0) idx.at(x, y) = tileIdx.at(selX + x, selY + y);
        }
    }
    customBrush = std::move(img);
    customBrushIdx = std::move(idx);
    penMode = PEN_STAMP;
    return true;
}

void Model::stampOne(int x, int y, Color c, PalIndex idx) {
    if (wrapOn && tile.w > 0 && tile.h > 0) {
        x = ((x % tile.w) + tile.w) % tile.w;
        y = ((y % tile.h) + tile.h) % tile.h;
    }
    if (!tile.inside(x, y) || c.a == 0) return;
    if (clipToSelection && hasSelection && !isSelected(x, y)) return;
    if (!activeLayerEditable()) return;
    if (strictIndexedOn()) {
        // Палитру могли переставить после того, как кисть взяли: номер
        // сверяется с цветом, как у привязки точек.
        const PalIndex i =
            resolveIndex(c, idx < palette.size() && sameRGB(palette[idx], c) ? idx : IDX_NONE);
        tile.at(x, y) = effectiveColor(i);
        setIdx(x, y, i);
    } else if (brushReplaces() || c.a == 255) {
        tile.at(x, y) = c;
        setIdx(x, y, idx < palette.size() && sameRGB(palette[idx], c) ? idx : indexOfColor(c));
    } else {
        // Полупрозрачная точка кисти ложится на картинку до мазка: отпечатки
        // идут внахлёст, и иначе край темнел бы с каждым шагом.
        const Color base =
            strokeBase.sameSize(tile.w, tile.h) ? strokeBase.at(x, y) : tile.at(x, y);
        tile.at(x, y) = blendOver(c, base);
        setIdx(x, y, indexOfColor(tile.at(x, y)));
    }
    tileBmpDirty = true;
    if (liveDraw && drawTilePixel) drawTilePixel(x, y);
}

void Model::stampBrushAt(int cx, int cy) {
    if (customBrush.empty()) return;
    const int ox = cx - (customBrush.w - 1) / 2;
    const int oy = cy - (customBrush.h - 1) / 2;
    for (int y = 0; y < customBrush.h; ++y) {
        for (int x = 0; x < customBrush.w; ++x) {
            const Color c = customBrush.at(x, y);
            if (c.a == 0) continue;
            const PalIndex i = customBrushIdx.sameSize(customBrush.w, customBrush.h)
                                   ? customBrushIdx.at(x, y)
                                   : IDX_NONE;
            forEachSymmetric(ox + x, oy + y, [&](int px, int py) { stampOne(px, py, c, i); });
        }
    }
}

// --- затенение ---------------------------------------------------------------------

PalIndex Model::shadeTarget(PalIndex idx, bool darker) const {
    if (idx >= palette.size()) return idx;
    const int row = idx / PAL_COLS;
    const int first = row * PAL_COLS;
    const int last = std::min(first + PAL_COLS, static_cast<int>(palette.size())) - 1;
    if (last <= first) return idx;
    // Ряд — ступени одного цвета; в какую сторону он светлеет, решают
    // крайние ячейки: так работает и ряд, вставленный «Рядом цветов», и
    // переставленный задом наперёд.
    const bool lightRight = luma(palette[static_cast<std::size_t>(last)]) >=
                            luma(palette[static_cast<std::size_t>(first)]);
    const int step = (lightRight != darker) ? 1 : -1;
    const int to = static_cast<int>(idx) + step;
    if (to < first || to > last) return idx;
    return static_cast<PalIndex>(to);
}

void Model::shadeOne(int x, int y) {
    const Color c = tile.at(x, y);
    if (c.a == 0) return;
    ensureTileIdx();
    PalIndex i = tileIdx.at(x, y);
    if (i >= palette.size() || !sameRGB(effectiveColor(i), c)) i = indexOfColor(c);
    // Цвет не из палитры не затеняется: шагать не по чему.
    if (i == IDX_NONE) return;
    const PalIndex to = shadeTarget(i, shadeDarker);
    if (to == i) return;
    const Color t = effectiveColor(to);
    tile.at(x, y) = strictIndexedOn() ? t : argb(c.a, t.r, t.g, t.b);
    setIdx(x, y, to);
    tileBmpDirty = true;
    if (liveDraw && drawTilePixel) drawTilePixel(x, y);
}

// --- градиент узором ---------------------------------------------------------------

Grid<std::uint8_t> Model::gradientRegion(int x, int y) const {
    if (!tile.inside(x, y)) return {};
    Grid<std::uint8_t> region = img::maskByColor(tile, x, y, true);
    if (clipToSelection && hasSelection) {
        const Grid<std::uint8_t> sel = selectionMask();
        for (std::size_t k = 0; k < region.d.size(); ++k)
            region.d[k] = region.d[k] != 0 && sel.d[k] != 0 ? 1 : 0;
    }
    return region;
}

int Model::applyGradient(const Grid<std::uint8_t>& region, int ax, int ay, int bx, int by) {
    if (!region.sameSize(tile.w, tile.h) || !activeLayerEditable()) return 0;
    ensureTileIdx();
    // Цвета — ровно два цвета кисти; в строгом индексном режиме — их ячейки.
    const bool strict = strictIndexedOn();
    const PalIndex i1 = strict ? resolveIndex(firstColor, firstIdx) : firstIdx;
    const PalIndex i2 = strict ? resolveIndex(secondColor, secondIdx) : secondIdx;
    const Color c1 = strict ? effectiveColor(i1) : firstColor;
    const Color c2 = strict ? effectiveColor(i2) : secondColor;
    const double dx = bx - ax;
    const double dy = by - ay;
    const double len2 = dx * dx + dy * dy;
    const double len = std::sqrt(len2);
    int n = 0;
    for (int y = 0; y < tile.h; ++y) {
        for (int x = 0; x < tile.w; ++x) {
            if (region.at(x, y) == 0) continue;
            // Доля второго цвета: проекция на отрезок нажатия, у кругового —
            // расстояние от точки нажатия. Центры точек — в (x + ½, y + ½).
            const double px = x - ax;
            const double py = y - ay;
            // Щелчок без протяжки — заливка первым цветом, как у обычной.
            double t = 0.0;
            if (len2 > 0) {
                t = gradientRadial ? std::hypot(px, py) / len : (px * dx + py * dy) / len2;
            }
            t = std::clamp(t, 0.0, 1.0);
            // 65 ступеней: при t = 0 — весь первый цвет, при t = 1 — второй.
            const bool second = (bayer8(x, y) + 0.5) / 64.0 < t;
            tile.at(x, y) = second ? c2 : c1;
            setIdx(x, y, second ? i2 : i1);
            ++n;
        }
    }
    tileBmpDirty = true;
    overDirty = true;
    needRepaint = true;
    return n;
}

// --- слой-образец --------------------------------------------------------------

int Model::addReferenceLayer(const Grid<Color>& img, const std::string& name) {
    if (docMode != DOC_TILE || img.empty() || tile.empty()) return -1;
    pushUndo();
    commitActiveCel();
    // Вписать в документ, не искажая: образец — фотография или эскиз, для
    // них плавный масштаб честнее ближайшего соседа.
    const double k =
        std::min(static_cast<double>(tile.w) / img.w, static_cast<double>(tile.h) / img.h);
    const int w = std::max(1, static_cast<int>(std::lround(img.w * k)));
    const int h = std::max(1, static_cast<int>(std::lround(img.h * k)));
    const Grid<Color> fit = (w == img.w && h == img.h) ? img : fx::scaleSmooth(img, w, h);
    Grid<Color> canvas(tile.w, tile.h, kClear);
    const int ox = (tile.w - w) / 2;
    const int oy = (tile.h - h) / 2;
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x)
            canvas.at(ox + x, oy + y) = fit.at(x, y);
    }
    Layer l{name.empty() ? std::string("Образец") : name};
    l.kind = LK_REFERENCE;
    l.opacity = 128;
    l.locked = true;
    doc.layers.insert(doc.layers.begin(), l);
    // Одна картинка на все кадры: образец не анимируется.
    Cel cel;
    cel.img = std::move(canvas);
    const CelPtr shared = std::make_shared<const Cel>(std::move(cel));
    doc.cels.insert(doc.cels.begin(),
                    std::vector<CelPtr>(static_cast<std::size_t>(doc.frameCount()), shared));
    ++activeLayer;
    loadActiveCel();
    tileBmpDirty = true;
    overDirty = true;
    needRepaint = true;
    return 0;
}

}  // namespace tjtm
