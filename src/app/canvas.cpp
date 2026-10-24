// Отрисовка холста (TJTM.pas: «Отрисовка холста»).

#include <algorithm>

#include "app/editor.h"
#include "core/pmath.h"
#include "core/theme.h"

namespace tjtm {

using namespace theme;

namespace {

Grid<Color> gridFromBgra(const std::vector<std::uint8_t>& raw, int w, int h) {
    Grid<Color> g(w, h);
    for (std::size_t i = 0; i < g.d.size() && i * 4 + 3 < raw.size(); ++i) {
        g.d[i] = Color(raw[i * 4 + 3], raw[i * 4 + 2], raw[i * 4 + 1], raw[i * 4]);
    }
    return g;
}

}  // namespace

/// Один пиксель тайла: при штрихе не перерисовывается весь холст на
/// каждое движение мыши.
void Editor::drawTilePixel(int x, int y) {
    if (!m.tile.inside(x, y)) return;
    const int g = m.gridGap();
    // Под холстом может лежать лента кадров: мазок у края её не задевает.
    fb_.setClip(canvasRect());
    fb_.fillRect(m.tileToScreenX(x), m.tileToScreenY(y), m.tileToScreenX(x + 1) - g,
                 m.tileToScreenY(y + 1) - g, m.displayColor(x, y));
    fb_.resetClip();
}

/// Клетка предпросмотра фигуры. В документ ничего не пишется.
void Editor::drawPreviewPixel(int x, int y) {
    if (!m.inDoc(x, y)) return;
    const int g = m.gridGap();
    fb_.setClip(canvasRect());
    fb_.fillRect(m.tileToScreenX(x), m.tileToScreenY(y), m.tileToScreenX(x + 1) - g,
                 m.tileToScreenY(y + 1) - g, cAccent);
    fb_.resetClip();
}

/// Пересобирает картинку тайла, если она устарела, не того размера или
/// собрана в другом режиме шахматки: шахматка заложена в неё, и весь
/// холст — один рисунок, а не два наложенных.
void Editor::ensureTileBmp() {
    if (!m.tileBmpDirty && tileBmpChecker_ == m.checkerOn &&
        tileBmp_.sameSize(m.tile.w, m.tile.h) && !tileBmp_.empty()) {
        return;
    }
    m.tileBmpDirty = false;
    tileBmpChecker_ = m.checkerOn;
    tileBmp_.clear();
    if (m.tile.empty()) return;
    tileBmp_ = gridFromBgra(m.tileBmpRaw(), m.tile.w, m.tile.h);
}

/// Клетки тайла. В бесшовном режиме рисуется повторяющийся узор, и швы
/// видно сразу. Весь холст — несколько выводов картинки, поэтому стоимость
/// не зависит ни от размера тайла, ни от масштаба.
void Editor::drawTileCells() {
    if (m.tile.empty()) return;
    ensureTileBmp();
    if (tileBmp_.empty()) return;
    const int W = m.W;
    const int H = m.H;
    const int X0 = m.X0;
    const int Y0 = m.Y0;
    const int ox = m.tileToScreenX(0);
    const int oy = m.tileToScreenY(0);
    const int sw = m.tile.w * m.pixelSize;
    const int sh = m.tile.h * m.pixelSize;

    int cl = X0;
    int ct = Y0;
    int cr = W;
    int cb = H;
    if (!m.seamlessOn) {
        cl = std::max(X0, ox);
        ct = std::max(Y0, oy);
        cr = std::min(W, ox + sw);
        cb = std::min(H, oy + sh);
    }
    // Тайл местности в окружении настоящей карты (этап 7): соседи — на весь
    // холст, как копии в бесшовном виде.
    const bool context = m.mapContext && !m.seamlessOn && editedTerrain() >= 0;
    if (context) {
        cl = X0;
        ct = Y0;
        cr = W;
        cb = H;
    }
    if (cr <= cl || cb <= ct) return;

    // Холст — левая часть окна; панель справа его не касается, линейки
    // слева и сверху — тоже.
    fb_.setClip(canvasRect());
    if (context) drawMapContext(ox, oy, sw, sh);
    int kx0 = 0;
    int kx1 = 0;
    int ky0 = 0;
    int ky1 = 0;
    if (m.seamlessOn) {
        kx0 = floorDiv(X0 - ox, sw);
        kx1 = floorDiv(W - 1 - ox, sw);
        ky0 = floorDiv(Y0 - oy, sh);
        ky1 = floorDiv(H - 1 - oy, sh);
    }
    // Пиксель-арт не сглаживается: drawImage — ближайший сосед.
    for (int ky = ky0; ky <= ky1; ++ky) {
        for (int kx = kx0; kx <= kx1; ++kx)
            fb_.drawImage(tileBmp_, ox + kx * sw, oy + ky * sh, sw, sh);
    }

    // Соседние копии притушены, чтобы было видно, где сам тайл.
    if (m.seamlessOn || context) {
        const Color dim = argb(56, 0, 0, 0);
        if (oy > Y0) fb_.fillRect(X0, Y0, W, std::min(oy, H), dim);
        if (oy + sh < H) fb_.fillRect(X0, std::max(Y0, oy + sh), W, H, dim);
        if (ox > X0) fb_.fillRect(X0, std::max(Y0, oy), std::min(ox, W), std::min(H, oy + sh), dim);
        if (ox + sw < W)
            fb_.fillRect(std::max(X0, ox + sw), std::max(Y0, oy), W, std::min(H, oy + sh), dim);
        fb_.frameRect(ox - 1, oy - 1, ox + sw + 1, oy + sh + 1, cAccent);
    }
    if (context) drawMapContextLabel();

    // Выделение подкрашивается одним прямоугольником поверх; выделение
    // произвольной формы подкрашивается по точкам (drawSelectionMask).
    if (m.hasSelection && m.selW > 0 && m.selH > 0 && m.selMask.empty()) {
        fb_.fillRect(m.tileToScreenX(m.selX), m.tileToScreenY(m.selY),
                     m.tileToScreenX(m.selX + m.selW), m.tileToScreenY(m.selY + m.selH),
                     argb(64, cAccent.r, cAccent.g, cAccent.b));
    }

    // Сетка — линии поверх по последнему столбцу и строке каждой клетки.
    if (m.gridGap() > 0) {
        const int ps = m.pixelSize;
        for (int k = (cl - ox) / ps - 1; k <= (cr - ox) / ps + 1; ++k) {
            const int gp = ox + k * ps - 1;
            if (gp >= cl && gp < cr) fb_.line(gp, ct, gp, cb - 1, cCanvas);
        }
        for (int k = (ct - oy) / ps - 1; k <= (cb - oy) / ps + 1; ++k) {
            const int gp = oy + k * ps - 1;
            if (gp >= ct && gp < cb) fb_.line(cl, gp, cr - 1, gp, cCanvas);
        }
    }
    fb_.resetClip();
}

/// Клетки карты и знаки объектов поверх них.
void Editor::drawMapCells() {
    const int g = m.gridGap();
    const int j0 = std::max(0, m.screenToTileX(m.X0));
    const int j1 = std::min(m.map.w - 1, m.screenToTileX(m.W - 1));
    const int i0 = std::max(0, m.screenToTileY(m.Y0));
    const int i1 = std::min(m.map.h - 1, m.screenToTileY(m.H - 1));
    fb_.setClip(canvasRect());
    const int ps = m.pixelSize;
    for (int i = i0; i <= i1; ++i) {
        for (int j = j0; j <= j1; ++j) {
            const bool sel = m.hasSelection && j >= m.selX && j < m.selX + m.selW && i >= m.selY &&
                             i < m.selY + m.selH;
            // Карта картинками набора игры (этап 7): клетка — её тайл или
            // вариант стыка; нет картинки — цвет оригинала.
            if (const Grid<Color>* img = m.mapTiles ? mapCellImage(j, i) : nullptr) {
                fb_.drawImage(*img, m.tileToScreenX(j), m.tileToScreenY(i), ps - g, ps - g);
                if (sel)
                    fb_.fillRect(m.tileToScreenX(j), m.tileToScreenY(i), m.tileToScreenX(j + 1) - g,
                                 m.tileToScreenY(i + 1) - g,
                                 argb(80, cAccent.r, cAccent.g, cAccent.b));
                continue;
            }
            Color c = mapTileColor(m.map.at(j, i));
            if (sel) c = blendOver(argb(80, cAccent.r, cAccent.g, cAccent.b), c);
            fb_.fillRect(m.tileToScreenX(j), m.tileToScreenY(i), m.tileToScreenX(j + 1) - g,
                         m.tileToScreenY(i + 1) - g, c);
        }
    }
    // Объект — знаком в клетке. На мелком масштабе знак не влезает, и
    // остаётся заметная точка.
    for (const MapObject& o : m.map.objects) {
        if (o.x < j0 || o.x > j1 || o.y < i0 || o.y > i1) continue;
        const int sx = m.tileToScreenX(o.x);
        const int sy = m.tileToScreenY(o.y);
        if (m.mapTiles) {
            // Объект — своей картинкой: житель и враг — по id, если нарисован,
            // иначе общим тайлом вида.
            const Grid<Color>* img = gameTile(m.mapCellTileFile(o.x, o.y));
            if (img == nullptr) img = gameTile(o.kind == "spawn" ? "mob" : o.kind);
            if (img != nullptr) {
                fb_.drawImage(*img, sx, sy, ps - g, ps - g);
                continue;
            }
        }
        fb_.fillRect(sx + 1, sy + 1, sx + m.pixelSize - 1, sy + m.pixelSize - 1,
                     argb(210, 250, 250, 210));
        if (m.pixelSize >= 10)
            textCentered(sx, sy, sx + m.pixelSize, sy + m.pixelSize, mapObjGlyph(o.kind),
                         rgb(20, 20, 30));
    }
    fb_.resetClip();
}

/// Рамка выделения «бегущими муравьями»: сплошная светлая линия и поверх
/// тёмный пунктир со сдвигом фазы. Статическая рамка на пёстром тайле
/// теряется, а бегущая читается на любом фоне.
void Editor::drawAnts(int x0, int y0, int x1, int y1) {
    if (x1 <= x0 || y1 <= y0) return;
    fb_.setClip(canvasRect());
    fb_.dashedRect(x0, y0, x1, y1, argb(255, 245, 245, 250), 1 << 20, 0);
    fb_.dashedRect(x0, y0, x1, y1, argb(255, 20, 20, 24), 4, antsPhase_);
    fb_.resetClip();
}

void Editor::drawCanvas() {
    fb_.fillRect(m.X0, m.Y0, m.W, m.H, cCanvas);
    const int g = m.gridGap();
    if (m.docMode == DOC_MAP) {
        drawMapCells();
    } else {
        drawTileCells();
    }

    // Предпросмотр буфера на новом месте, пока выделение перетаскивают.
    if (m.draggingSelection && m.docMode == DOC_TILE) {
        fb_.setClip(canvasRect());
        for (int i = 0; i < m.buffer.h; ++i) {
            for (int j = 0; j < m.buffer.w; ++j) {
                if (!m.inDoc(m.selX + j, m.selY + i) || m.buffer.at(j, i).a == 0) continue;
                fb_.fillRect(m.tileToScreenX(m.selX + j), m.tileToScreenY(m.selY + i),
                             m.tileToScreenX(m.selX + j + 1) - g,
                             m.tileToScreenY(m.selY + i + 1) - g,
                             blendOver(m.buffer.at(j, i), m.checkerColor(m.selX + j, m.selY + i)));
            }
        }
        fb_.resetClip();
    }

    if (m.hasSelection && m.selW > 0 && m.selH > 0) {
        if (m.selMask.empty()) {
            drawAnts(m.tileToScreenX(m.selX) - 1, m.tileToScreenY(m.selY) - 1,
                     m.tileToScreenX(m.selX + m.selW) + 1, m.tileToScreenY(m.selY + m.selH) + 1);
        } else {
            drawSelectionMask();
        }
    }

    // Оси симметрии видно на холсте, иначе легко забыть, что они включены.
    fb_.setClip(canvasRect());
    const Color axis = argb(90, 220, 60, 60);
    if (m.symX) {
        // Ось может стоять не посередине: её место — в половинах точки.
        const int cx = m.tileToScreenX(0) + m.axisX2() * m.pixelSize / 2;
        fb_.fillRect(cx - 1, m.tileToScreenY(0), cx + 1, m.tileToScreenY(m.docH()), axis);
    }
    if (m.symY) {
        const int cy = m.tileToScreenY(0) + m.axisY2() * m.pixelSize / 2;
        fb_.fillRect(m.tileToScreenX(0), cy - 1, m.tileToScreenX(m.docW()), cy + 1, axis);
    }
    fb_.resetClip();
    drawVectorHandles();
    drawRadialAxes();
    drawGuides();
    drawRulers();
}

}  // namespace tjtm
