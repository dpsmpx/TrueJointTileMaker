// Команды этапа 1 RoadToProLevel: выделение произвольной формы, его
// содержимое, преобразования картинки, «пиксель-перфект».
//
// Отдельно от model.cpp: там — перенос 0.6.0, сверенный с оригиналом, здесь
// — то, чего у оригинала не было. Команды, меняющие картинку, сами кладут
// шаг в историю: их зовут из меню и клавиш, и забыть отмену негде.

#include <algorithm>
#include <cmath>
#include <numbers>
#include <set>

#include "core/imageops.h"
#include "core/model.h"
#include "core/pmath.h"
#include "core/vector.h"

namespace tjtm {

namespace {

constexpr Color kClear = argb(0, 0, 0, 0);

}  // namespace

// --- симметрия --------------------------------------------------------------

int Model::mirrorX(int x) const {
    return axisX2() - 1 - x;
}

int Model::mirrorY(int y) const {
    return axisY2() - 1 - y;
}

void Model::forEachSymmetric(int x, int y, const std::function<void(int, int)>& fn) const {
    // Порядок оригинала: точка, зеркало по X, по Y, по обоим. Совпавшие
    // точки не отсеиваются — так и было, а повтор в мазке гасит strokeMask.
    std::pair<int, int> base[4];
    int n = 0;
    base[n++] = {x, y};
    if (symX) base[n++] = {mirrorX(x), y};
    if (symY) base[n++] = {x, mirrorY(y)};
    if (symX && symY) base[n++] = {mirrorX(x), mirrorY(y)};
    for (int k = 0; k < n; ++k)
        fn(base[k].first, base[k].second);
    if (symRadial < 2) return;
    // Повороты вокруг точки пересечения осей: центр точки (x + ½, y + ½)
    // поворачивается и попадает в ту точку, в которую лёг. Зеркала тоже
    // поворачиваются — включённые вместе, оси и лучи дают узор снежинки.
    const double cx = axisX2() / 2.0;
    const double cy = axisY2() / 2.0;
    for (int r = 1; r < symRadial; ++r) {
        const double a = 2.0 * std::numbers::pi * r / symRadial;
        const double ca = std::cos(a);
        const double sa = std::sin(a);
        for (int k = 0; k < n; ++k) {
            const double dx = base[k].first + 0.5 - cx;
            const double dy = base[k].second + 0.5 - cy;
            fn(static_cast<int>(std::floor(cx + dx * ca - dy * sa + 1e-7)),
               static_cast<int>(std::floor(cy + dx * sa + dy * ca + 1e-7)));
        }
    }
}

// --- «пиксель-перфект» ------------------------------------------------------

void Model::emitPixelPerfect(int x, int y) {
    // Отрезок, которым штрих догоняет мышь, начинается с прежней точки:
    // повтор — не новая точка.
    if (!strokePts.empty() && strokePts.back() == std::make_pair(x, y)) return;
    strokePts.emplace_back(x, y);
    paintPixel(x, y);
    const std::size_t n = strokePts.size();
    if (n < 3) return;
    const auto [ax, ay] = strokePts[n - 3];
    const auto [bx, by] = strokePts[n - 2];
    const auto [cx, cy] = strokePts[n - 1];
    if (!img::isPixelPerfectCorner(ax, ay, bx, by, cx, cy)) return;
    strokePts.erase(strokePts.begin() + static_cast<std::ptrdiff_t>(n - 2));

    // Точки, в которые легла средняя: сама она, её зеркала и перенос через
    // край. Те из них, что заняты другими точками мазка, остаются.
    auto targets = [this](int px, int py) {
        std::set<std::pair<int, int>> t;
        auto add = [&](int qx, int qy) {
            if (wrapOn && tile.w > 0 && tile.h > 0) {
                qx = ((qx % tile.w) + tile.w) % tile.w;
                qy = ((qy % tile.h) + tile.h) % tile.h;
            }
            if (tile.inside(qx, qy)) t.emplace(qx, qy);
        };
        forEachSymmetric(px, py, add);
        return t;
    };
    std::set<std::pair<int, int>> drop = targets(bx, by);
    for (const auto& p : strokePts) {
        for (const auto& q : targets(p.first, p.second))
            drop.erase(q);
    }
    for (const auto& [qx, qy] : drop) {
        if (!strokeBase.sameSize(tile.w, tile.h)) break;
        tile.at(qx, qy) = strokeBase.at(qx, qy);
        if (strokeBaseIdx.sameSize(tile.w, tile.h)) setIdx(qx, qy, strokeBaseIdx.at(qx, qy));
        if (strokeMask.sameSize(tile.w, tile.h)) strokeMask.at(qx, qy) = 0;
        tileBmpDirty = true;
        if (liveDraw && drawTilePixel) drawTilePixel(qx, qy);
    }
}

// --- выделение произвольной формы -------------------------------------------

bool Model::isSelected(int x, int y) const {
    if (!hasSelection || !inDoc(x, y)) return false;
    if (selMask.sameSize(docW(), docH())) return selMask.at(x, y) != 0;
    return x >= selX && x < selX + selW && y >= selY && y < selY + selH;
}

Grid<std::uint8_t> Model::selectionMask() const {
    if (hasSelection && selMask.sameSize(docW(), docH())) return selMask;
    Grid<std::uint8_t> m(docW(), docH(), 0);
    if (!hasSelection) return m;
    return img::maskRect(docW(), docH(), selX, selY, selX + selW - 1, selY + selH - 1);
}

void Model::setSelectionMask(const Grid<std::uint8_t>& mask) {
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    if (!mask.sameSize(docW(), docH()) || !img::maskBounds(mask, x, y, w, h)) {
        resetSelection();
        needRepaint = true;
        return;
    }
    hasSelection = true;
    selX = x;
    selY = y;
    selW = w;
    selH = h;
    // Прямоугольник хранится рамкой: отрисовка и операции оригинала
    // работают с ней, и для обычной рамки ничего не меняется.
    if (img::maskIsRect(mask, x, y, w, h)) {
        selMask.clear();
    } else {
        selMask = mask;
    }
    needRepaint = true;
}

void Model::applySelection(const Grid<std::uint8_t>& mask, int op) {
    Grid<std::uint8_t> cur = selectionMask();
    img::combine(cur, mask, op);
    setSelectionMask(cur);
}

void Model::selectAll() {
    if (docW() < 1 || docH() < 1) return;
    setSelectionMask(img::maskRect(docW(), docH(), 0, 0, docW() - 1, docH() - 1));
}

void Model::deselect() {
    if (hasSelection) lastSelMask = selectionMask();
    resetSelection();
    needRepaint = true;
}

void Model::invertSelection() {
    if (!hasSelection) {
        selectAll();
        return;
    }
    setSelectionMask(img::invert(selectionMask()));
}

bool Model::reselect() {
    if (!lastSelMask.sameSize(docW(), docH())) return false;
    setSelectionMask(lastSelMask);
    return hasSelection;
}

void Model::growSelection(int n) {
    if (!hasSelection || n == 0) return;
    setSelectionMask(img::grow(selectionMask(), n));
}

void Model::selectByColorAt(int x, int y, bool contiguous, int op) {
    if (docMode != DOC_TILE || !tile.inside(x, y)) return;
    applySelection(img::maskByColor(tile, x, y, contiguous), op);
}

void Model::selectOpaque(int op) {
    if (docMode != DOC_TILE) return;
    applySelection(img::maskOpaque(tile), op);
}

Grid<Color> Model::selectionContent() const {
    Grid<Color> c(selW, selH, kClear);
    if (!hasSelection) return c;
    for (int y = 0; y < selH; ++y) {
        for (int x = 0; x < selW; ++x) {
            if (isSelected(selX + x, selY + y)) c.at(x, y) = tile.at(selX + x, selY + y);
        }
    }
    return c;
}

bool Model::copySelection() {
    if (!hasSelection || selW < 1 || selH < 1) return false;
    if (docMode == DOC_MAP) {
        copySelectionToBuffer();
        return true;
    }
    buffer = selectionContent();
    return true;
}

bool Model::eraseSelection() {
    if (docMode != DOC_TILE || !hasSelection) return false;
    pushUndo();
    for (int y = selY; y < selY + selH; ++y) {
        for (int x = selX; x < selX + selW; ++x) {
            if (!isSelected(x, y)) continue;
            tile.at(x, y) = kClear;
            setIdx(x, y, IDX_NONE);
        }
    }
    tileBmpDirty = true;
    needRepaint = true;
    return true;
}

bool Model::cutSelection() {
    return docMode == DOC_TILE && copySelection() && eraseSelection();
}

void Model::pasteAsSelection(int px, int py) {
    if (!bufferFilled()) return;
    pushUndo();
    pasteBufferAt(px, py);
    const int bw = docMode == DOC_MAP ? mapBuffer.w : buffer.w;
    const int bh = docMode == DOC_MAP ? mapBuffer.h : buffer.h;
    setSelectionMask(img::maskRect(docW(), docH(), px, py, px + bw - 1, py + bh - 1));
    needRepaint = true;
}

void Model::fillSelectionOrAll(Color c) {
    if (docMode != DOC_TILE || tile.empty()) return;
    pushUndo();
    const PalIndex ni = c.a == 0 ? IDX_NONE : indexOfColor(c);
    for (int y = 0; y < tile.h; ++y) {
        for (int x = 0; x < tile.w; ++x) {
            if (hasSelection && !isSelected(x, y)) continue;
            tile.at(x, y) = c;
            setIdx(x, y, ni);
        }
    }
    tileBmpDirty = true;
    needRepaint = true;
}

Color Model::blankColor() const {
    return transparentBlank ? kClear : argb(255, 255, 255, 255);
}

// --- преобразования ---------------------------------------------------------

namespace {

template <class T>
Grid<T> applyTf(const Grid<T>& g, int op) {
    switch (op) {
        case TF_FLIP_H: return img::flipH(g);
        case TF_FLIP_V: return img::flipV(g);
        case TF_ROT90: return img::rotate90(g);
        case TF_ROT180: return img::rotate180(g);
        default: return img::rotate270(g);
    }
}

}  // namespace

void Model::liftSelection(bool erase, Grid<Color>& content, Grid<std::uint8_t>& mask) {
    content = selectionContent();
    const Grid<std::uint8_t> full = selectionMask();
    mask = img::crop(full, selX, selY, selW, selH, std::uint8_t{0});
    if (!erase) return;
    // Вынимают то, что сами только что положили, — под ним прежняя
    // картинка, а не дыра: кусок можно двигать и вертеть сколько угодно.
    const bool floating = floatResult == tile && floatSel == full &&
                          floatUnder.sameSize(tile.w, tile.h) &&
                          floatUnderIdx.sameSize(tile.w, tile.h);
    for (int y = selY; y < selY + selH; ++y) {
        for (int x = selX; x < selX + selW; ++x) {
            if (!isSelected(x, y)) continue;
            tile.at(x, y) = floating ? floatUnder.at(x, y) : kClear;
            setIdx(x, y, floating ? floatUnderIdx.at(x, y) : IDX_NONE);
        }
    }
    tileBmpDirty = true;
}

void Model::placeFloating(const Grid<Color>& content, const Grid<std::uint8_t>& mask, int px,
                          int py) {
    ensureTileIdx();
    floatUnder = tile;
    floatUnderIdx = tileIdx;
    Grid<std::uint8_t> sel(docW(), docH(), 0);
    for (int y = 0; y < content.h; ++y) {
        for (int x = 0; x < content.w; ++x) {
            const int tx = px + x;
            const int ty = py + y;
            if (!tile.inside(tx, ty) || mask.at(x, y) == 0) continue;
            sel.at(tx, ty) = 1;
            const Color c = content.at(x, y);
            if (c.a == 0) continue;
            tile.at(tx, ty) = c;
            setIdx(tx, ty, indexOfColor(c));
        }
    }
    tileBmpDirty = true;
    setSelectionMask(sel);
    floatResult = tile;
    floatSel = selectionMask();
}

/// После смены размеров картинки: выделение и штрих относились к старой.
static void afterResize(Model& m) {
    m.resetSelection();
    m.strokeMask.clear();
    m.ensureTileIdx();
    m.clampView();
    m.tileBmpDirty = true;
    m.overDirty = true;
    m.needRepaint = true;
}

void Model::transformImage(int op) {
    if (docMode != DOC_TILE || tile.empty()) return;
    pushUndo();
    ensureTileIdx();
    if (hasSelection) {
        Grid<Color> content;
        Grid<std::uint8_t> mask;
        liftSelection(true, content, mask);
        content = applyTf(content, op);
        mask = applyTf(mask, op);
        // Поворот — вокруг середины рамки: содержимое остаётся на месте.
        placeFloating(content, mask, selX + (selW - content.w) / 2, selY + (selH - content.h) / 2);
        needRepaint = true;
        return;
    }
    // Вся картинка — это все слои и кадры: поворачивается спрайт целиком,
    // и точки векторных фигур — тем же преобразованием (центр точки x —
    // x + ½, поэтому отражение — w - x, а не w - 1 - x).
    const double w = tile.w;
    const double h = tile.h;
    forEachCel(
        [op](Grid<Color>& img, Grid<PalIndex>& idx) {
            img = applyTf(img, op);
            idx = applyTf(idx, op);
        },
        CEL_GEOMETRY,
        [op, w, h](VecShape& s) {
            for (VecPoint& p : s.pts) {
                const VecPoint q = p;
                switch (op) {
                    case TF_FLIP_H: p.x = w - q.x; break;
                    case TF_FLIP_V: p.y = h - q.y; break;
                    case TF_ROT90: p = {h - q.y, q.x}; break;
                    case TF_ROT180: p = {w - q.x, h - q.y}; break;
                    default: p = {q.y, w - q.x}; break;
                }
            }
        });
    afterResize(*this);
}

bool Model::scaleImageTo(int nw, int nh) {
    if (docMode != DOC_TILE || tile.empty()) return false;
    if (nw < 1 || nh < 1 || nw > MAX_TILE || nh > MAX_TILE) return false;
    pushUndo();
    const double sx = static_cast<double>(nw) / tile.w;
    const double sy = static_cast<double>(nh) / tile.h;
    forEachCel(
        [nw, nh](Grid<Color>& img, Grid<PalIndex>& idx) {
            img = img::scaleNearest(img, nw, nh);
            idx = img::scaleNearest(idx, nw, nh);
        },
        CEL_GEOMETRY,
        [sx, sy](VecShape& s) {
            for (VecPoint& p : s.pts) {
                p.x *= sx;
                p.y *= sy;
            }
            // Толщина растёт вместе с картинкой, как у растра.
            s.width = std::max(1, pround(s.width * (sx + sy) / 2));
        });
    afterResize(*this);
    return true;
}

bool Model::scaleBy(int num, int den) {
    if (docMode != DOC_TILE || tile.empty() || num < 1 || den < 1) return false;
    if (!hasSelection) return scaleImageTo(tile.w * num / den, tile.h * num / den);
    const int nw = std::max(1, selW * num / den);
    const int nh = std::max(1, selH * num / den);
    if (nw > MAX_TILE || nh > MAX_TILE) return false;
    pushUndo();
    ensureTileIdx();
    Grid<Color> content;
    Grid<std::uint8_t> mask;
    liftSelection(true, content, mask);
    // Масштаб — от левого верхнего угла рамки: так увеличенный спрайт
    // остаётся там, где его поставили.
    placeFloating(img::scaleNearest(content, nw, nh), img::scaleNearest(mask, nw, nh), selX, selY);
    needRepaint = true;
    return true;
}

bool Model::cropToSelection() {
    if (docMode != DOC_TILE || !hasSelection || selW < 1 || selH < 1) return false;
    if (selW == tile.w && selH == tile.h) return false;
    pushUndo();
    const int x = selX;
    const int y = selY;
    const int w = selW;
    const int h = selH;
    forEachCel(
        [=](Grid<Color>& img, Grid<PalIndex>& idx) {
            img = img::crop(img, x, y, w, h, kClear);
            idx = img::crop(idx, x, y, w, h, IDX_NONE);
        },
        CEL_GEOMETRY, [x, y](VecShape& s) { vec::translate(s, -x, -y); });
    afterResize(*this);
    return true;
}

bool Model::cropToContent() {
    if (docMode != DOC_TILE) return false;
    // Содержимое — всех слоёв и кадров: обрезка не должна срезать то, что
    // нарисовано в другом кадре.
    Grid<std::uint8_t> any(tile.w, tile.h, 0);
    forEachCelConst([&](const Grid<Color>& g) {
        for (std::size_t k = 0; k < g.d.size() && k < any.d.size(); ++k)
            if (g.d[k].a != 0) any.d[k] = 1;
    });
    int x = 0;
    int y = 0;
    int w = 0;
    int h = 0;
    if (!img::maskBounds(any, x, y, w, h)) return false;
    if (x == 0 && y == 0 && w == tile.w && h == tile.h) return false;
    pushUndo();
    forEachCel(
        [=](Grid<Color>& img, Grid<PalIndex>& idx) {
            img = img::crop(img, x, y, w, h, kClear);
            idx = img::crop(idx, x, y, w, h, IDX_NONE);
        },
        CEL_GEOMETRY, [x, y](VecShape& s) { vec::translate(s, -x, -y); });
    afterResize(*this);
    return true;
}

bool Model::resizeCanvas(int nw, int nh, int anchorX, int anchorY) {
    if (docMode != DOC_TILE || tile.empty()) return false;
    if (nw < MIN_TILE || nh < MIN_TILE || nw > MAX_TILE || nh > MAX_TILE) return false;
    if (nw == tile.w && nh == tile.h) return false;
    pushUndo();
    const Color blank = blankColor();
    const PalIndex bi = blank.a == 0 ? IDX_NONE : indexOfColor(blank);
    // У слоёв новое место пустое: белый холст верхнего слоя закрыл бы всё
    // под ним. Фон документа — только у картинки из одного слоя.
    const bool single = doc.layerCount() <= 1;
    const Color fill = single ? blank : kClear;
    const PalIndex fillIdx = single ? bi : IDX_NONE;
    const int ox = img::anchorOffset(tile.w, nw, anchorX);
    const int oy = img::anchorOffset(tile.h, nh, anchorY);
    forEachCel(
        [&](Grid<Color>& img, Grid<PalIndex>& idx) {
            img = img::resizeCanvas(img, nw, nh, anchorX, anchorY, fill);
            idx = img::resizeCanvas(idx, nw, nh, anchorX, anchorY, fillIdx);
        },
        CEL_GEOMETRY, [ox, oy](VecShape& s) { vec::translate(s, ox, oy); });
    afterResize(*this);
    return true;
}

void Model::offsetImage(int dx, int dy) {
    if (docMode != DOC_TILE || tile.empty() || (dx == 0 && dy == 0)) return;
    pushUndo();
    // Перенос через край рвёт фигуры пополам: векторный слой после него —
    // растр, как и тайлкарта.
    forEachCel(
        [dx, dy](Grid<Color>& img, Grid<PalIndex>& idx) {
            img = img::offsetWrap(img, dx, dy);
            idx = img::offsetWrap(idx, dx, dy);
        },
        CEL_GEOMETRY);
    resetSelection();
    tileBmpDirty = true;
    needRepaint = true;
}

/// Правка картинки целиком через копию: шаг истории кладётся, только если
/// что-то изменилось, а привязки чинятся там, где сменился цвет.
template <class F>
static int editWithUndo(Model& m, F&& f) {
    Grid<Color> t = m.tile;
    const int n = f(t);
    if (n <= 0) return 0;
    m.pushUndo();
    m.ensureTileIdx();
    for (std::size_t k = 0; k < t.d.size(); ++k) {
        if (t.d[k] == m.tile.d[k]) continue;
        m.tileIdx.d[k] = t.d[k].a == 0 ? IDX_NONE : m.indexOfColor(t.d[k]);
    }
    m.tile = std::move(t);
    m.tileBmpDirty = true;
    m.needRepaint = true;
    return n;
}

int Model::outlineImage(Color c, bool eightWay, bool outside, int thickness) {
    if (docMode != DOC_TILE || tile.empty()) return 0;
    const Grid<std::uint8_t> within = hasSelection ? selectionMask() : Grid<std::uint8_t>{};
    return editWithUndo(*this, [&](Grid<Color>& t) {
        return img::outlineThick(t, within, c, eightWay, outside, thickness);
    });
}

int Model::growImageEdge(bool eightWay) {
    if (docMode != DOC_TILE || tile.empty()) return 0;
    const Grid<std::uint8_t> within = hasSelection ? selectionMask() : Grid<std::uint8_t>{};
    return editWithUndo(*this, [&](Grid<Color>& t) { return img::growEdge(t, within, eightWay); });
}

int Model::shrinkImageEdge(bool eightWay) {
    if (docMode != DOC_TILE || tile.empty()) return 0;
    const Grid<std::uint8_t> within = hasSelection ? selectionMask() : Grid<std::uint8_t>{};
    return editWithUndo(*this,
                        [&](Grid<Color>& t) { return img::shrinkEdge(t, within, eightWay); });
}

bool Model::rotateImageBy(double degrees, int method, bool expand) {
    if (docMode != DOC_TILE || tile.empty()) return false;
    // Цвет и номер палитры поворачиваются одним значением: RotSprite
    // сравнивает точки, и пара должна ехать вместе.
    auto pack = [](const Grid<Color>& img, const Grid<PalIndex>& idx) {
        Grid<std::uint64_t> p(img.w, img.h, 0);
        for (std::size_t k = 0; k < p.d.size(); ++k)
            p.d[k] = (static_cast<std::uint64_t>(img.d[k].argb()) << 16) |
                     (k < idx.d.size() ? idx.d[k] : IDX_NONE);
        return p;
    };
    auto unpack = [](const Grid<std::uint64_t>& p, Grid<Color>& img, Grid<PalIndex>& idx) {
        img.resize(p.w, p.h);
        idx.resize(p.w, p.h);
        for (std::size_t k = 0; k < p.d.size(); ++k) {
            img.d[k] = Color::fromArgb(static_cast<std::uint32_t>(p.d[k] >> 16));
            idx.d[k] = static_cast<PalIndex>(p.d[k] & 0xFFFF);
        }
    };
    const std::uint64_t clear = IDX_NONE;  // прозрачный цвет и «без номера»
    ensureTileIdx();
    if (hasSelection) {
        pushUndo();
        Grid<Color> content;
        Grid<std::uint8_t> mask;
        liftSelection(true, content, mask);
        Grid<PalIndex> noIdx;
        const Grid<std::uint64_t> r =
            img::rotateAny(pack(content, noIdx), degrees, method, true, clear);
        Grid<PalIndex> ridx;
        unpack(r, content, ridx);
        mask = img::rotateAny(mask, degrees, method, true, std::uint8_t{0});
        placeFloating(content, mask, selX + (selW - content.w) / 2, selY + (selH - content.h) / 2);
        needRepaint = true;
        return true;
    }
    int nw = tile.w;
    int nh = tile.h;
    if (expand) img::rotatedSize(tile.w, tile.h, degrees, nw, nh);
    if (nw > MAX_TILE || nh > MAX_TILE) return false;
    pushUndo();
    const double a = degrees * std::numbers::pi / 180.0;
    const double ca = std::cos(a);
    const double sa = std::sin(a);
    const double scx = tile.w / 2.0;
    const double scy = tile.h / 2.0;
    const double dcx = nw / 2.0;
    const double dcy = nh / 2.0;
    forEachCel(
        [&](Grid<Color>& img, Grid<PalIndex>& idx) {
            unpack(img::rotateAny(pack(img, idx), degrees, method, expand, clear), img, idx);
        },
        CEL_GEOMETRY,
        [=](VecShape& s) {
            for (VecPoint& p : s.pts) {
                const double dx = p.x - scx;
                const double dy = p.y - scy;
                p = {dcx + dx * ca - dy * sa, dcy + dx * sa + dy * ca};
            }
        });
    afterResize(*this);
    return true;
}

int Model::removeIsolatedPixels() {
    if (docMode != DOC_TILE || tile.empty()) return 0;
    const Grid<std::uint8_t> within = hasSelection ? selectionMask() : Grid<std::uint8_t>{};
    return editWithUndo(*this, [&](Grid<Color>& t) { return img::removeIsolated(t, within); });
}

}  // namespace tjtm
