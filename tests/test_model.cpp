// Логика редактора против внутренностей TJTM.pas.
//
// tools/golden/GoldenEditorBody.pas подставляется вместо главного блока
// оригинала и прогоняет сценарии, порождённые генератором с известным
// зерном. Здесь те же сценарии идут через Model, и хэши состояния
// сравниваются построчно.
//
// Одно правило важнее остальных: в Паскале аргументы вычисляются слева
// направо, а в C++ порядок не определён. Поэтому каждое обращение к
// генератору — отдельной строкой, в том порядке, в каком его делает
// харнесс.

#include <gtest/gtest.h>

#include <map>

#include "core/model.h"
#include "golden_util.h"

using namespace tjtm;

namespace {

struct Harness {
    Model m;
    long long seed = 0;

    int next() {
        seed = (seed * 1103515245 + 12345) & 0x7FFFFFFF;
        return static_cast<int>(seed >> 4);
    }
    int nextIn(int lo, int hi) { return lo + next() % (hi - lo + 1); }

    Color randColor() {
        const int k = nextIn(0, 3);
        switch (k) {
            case 0: {
                const int j = nextIn(0, 7);
                const int i = nextIn(0, 7);
                return m.palAt(j, i);
            }
            case 1: {
                const int r = nextIn(0, 255);
                const int g = nextIn(0, 255);
                const int b = nextIn(0, 255);
                return rgb(r, g, b);
            }
            case 2: {
                const int a = nextIn(1, 254);
                const int r = nextIn(0, 255);
                const int g = nextIn(0, 255);
                const int b = nextIn(0, 255);
                return argb(a, r, g, b);
            }
            default: return argb(0, 0, 0, 0);
        }
    }

    // IDX_NONE оригинала — байт 255.
    static int idxByte(PalIndex v) { return v == IDX_NONE ? 255 : static_cast<int>(v); }

    void hashTile(golden::Fnv& f) const {
        for (int y = 0; y < m.tile.h; ++y) {
            for (int x = 0; x < m.tile.w; ++x) {
                f.i32(static_cast<int>(m.tile.at(x, y).argb()));
                if (m.tileIdx.sameSize(m.tile.w, m.tile.h)) {
                    f.i32(idxByte(m.tileIdx.at(x, y)));
                } else {
                    f.i32(-1);
                }
            }
        }
    }

    void hashMap(golden::Fnv& f) const {
        for (int y = 0; y < m.map.h; ++y) {
            for (int x = 0; x < m.map.w; ++x)
                f.byte(m.map.at(x, y));
        }
    }

    void randomTile(int w, int h) {
        Grid<Color> t(w, h);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x)
                t.at(x, y) = randColor();
        }
        m.tile = std::move(t);
        m.tileIdx.clear();
        m.rebindTileToPalette();
    }

    void emptyTile(int w, int h) {
        m.tile = Grid<Color>(w, h);
        m.tileIdx.clear();
    }

    void randomMap(int w, int h) {
        m.map.reset(w, h);
        for (int y = 0; y < m.map.h; ++y) {
            for (int x = 0; x < m.map.w; ++x)
                m.map.at(x, y) = static_cast<std::uint8_t>(nextIn(0, MT_COUNT - 1));
        }
    }

    // InitCollections оригинала.
    void initCollections() {
        m.setDefaultPalette();
        m.buffer.clear();
        m.mapBuffer.clear();
        m.strokeMask.clear();
        m.clearHistory();
    }

    void resetBrushState() {
        m.docMode = DOC_TILE;
        m.shapePreview = false;
        m.liveDraw = false;
        m.wrapOn = false;
        m.symX = false;
        m.symY = false;
        m.brushReplace = false;
        m.toolTile = T_PEN;
    }
};

const std::vector<std::vector<std::string>>& editorGolden() {
    static const auto lines = golden::readLines("editor.txt");
    return lines;
}

/// Хэши эталона по виду сценария: kind -> (номер -> хэш).
std::map<int, std::string> goldenHashes(const std::string& kind) {
    std::map<int, std::string> out;
    for (const auto& w : editorGolden()) {
        if (w.size() == 3 && w[0] == kind) out[std::stoi(w[1])] = w[2];
    }
    return out;
}

std::vector<std::vector<std::string>> goldenRows(const std::string& kind) {
    std::vector<std::vector<std::string>> out;
    for (const auto& w : editorGolden()) {
        if (w[0] == kind) out.push_back(w);
    }
    return out;
}

}  // namespace

TEST(Model, DefaultPalette) {
    Harness hs;
    hs.initCollections();
    const auto rows = goldenRows("palette");
    ASSERT_EQ(rows.size(), 8u);
    for (int i = 0; i < PAL_ROWS; ++i) {
        for (int j = 0; j < PAL_COLS; ++j) {
            EXPECT_EQ(static_cast<int>(hs.m.palAt(j, i).argb()),
                      std::stoi(rows[static_cast<std::size_t>(i)][static_cast<std::size_t>(j) + 1]))
                << j << "," << i;
        }
    }
}

TEST(Model, ShapesAndBrush) {
    Harness hs;
    Model& m = hs.m;
    const auto want = goldenHashes("shape");
    ASSERT_EQ(want.size(), 3000u);
    hs.seed = 4242;
    for (int k = 0; k < 3000; ++k) {
        hs.initCollections();
        hs.resetBrushState();
        const int w = hs.nextIn(1, 26);
        const int h = hs.nextIn(1, 22);
        hs.randomTile(w, h);
        static const int kTools[] = {T_PEN, T_ERASER, T_DITHER};
        m.toolTile = kTools[hs.nextIn(0, 2)];
        m.brushReplace = hs.nextIn(0, 1) == 1;
        m.wrapOn = hs.nextIn(0, 1) == 1;
        m.symX = hs.nextIn(0, 2) == 0;
        m.symY = hs.nextIn(0, 2) == 0;
        m.firstColor = hs.randColor();
        m.secondColor = hs.randColor();
        m.firstIdx = m.indexOfColor(m.firstColor);
        m.secondIdx = m.indexOfColor(m.secondColor);
        if (hs.nextIn(0, 4) == 0) m.firstIdx = static_cast<PalIndex>(hs.nextIn(0, 63));
        const bool useMask = hs.nextIn(0, 3) != 0;
        m.strokeMask.clear();
        if (useMask) m.strokeMask.resize(m.tile.w, m.tile.h, 0);
        const int kind = hs.nextIn(0, 5);
        const int x0 = hs.nextIn(-6, m.tile.w + 5);
        const int y0 = hs.nextIn(-6, m.tile.h + 5);
        const int x1 = hs.nextIn(-6, m.tile.w + 5);
        const int y1 = hs.nextIn(-6, m.tile.h + 5);
        switch (kind) {
            case 0: m.shapeLine(x0, y0, x1, y1); break;
            case 1: m.shapeRect(x0, y0, x1, y1, false); break;
            case 2: m.shapeRect(x0, y0, x1, y1, true); break;
            case 3: m.shapeEllipse(x0, y0, x1, y1, false); break;
            case 4: m.shapeEllipse(x0, y0, x1, y1, true); break;
            default: {
                m.emitPixel(x0, y0);
                int lx = x0;
                int ly = y0;
                const int steps = hs.nextIn(1, 6);
                for (int s = 1; s <= steps; ++s) {
                    const int nx = hs.nextIn(-6, m.tile.w + 5);
                    const int ny = hs.nextIn(-6, m.tile.h + 5);
                    m.shapeLine(lx, ly, nx, ny);
                    lx = nx;
                    ly = ny;
                }
                break;
            }
        }
        golden::Fnv f;
        hs.hashTile(f);
        ASSERT_EQ(f.hex(), want.at(k)) << "сценарий " << k << " фигура " << kind;
    }
}

TEST(Model, MapShapesAndFill) {
    Harness hs;
    Model& m = hs.m;
    const auto want = goldenHashes("mapshape");
    ASSERT_EQ(want.size(), 1000u);
    hs.seed = 777;
    for (int k = 0; k < 1000; ++k) {
        hs.resetBrushState();
        m.docMode = DOC_MAP;
        const int w = hs.nextIn(4, 30);
        const int h = hs.nextIn(4, 20);
        hs.randomMap(w, h);
        m.mapBrush = hs.nextIn(0, MT_COUNT - 1);
        m.symX = hs.nextIn(0, 2) == 0;
        m.symY = hs.nextIn(0, 2) == 0;
        m.strokeMask.clear();
        if (hs.nextIn(0, 3) != 0) m.strokeMask.resize(m.map.w, m.map.h, 0);
        const int kind = hs.nextIn(0, 3);
        const int x0 = hs.nextIn(-3, m.map.w + 2);
        const int y0 = hs.nextIn(-3, m.map.h + 2);
        const int x1 = hs.nextIn(-3, m.map.w + 2);
        const int y1 = hs.nextIn(-3, m.map.h + 2);
        switch (kind) {
            case 0: m.shapeLine(x0, y0, x1, y1); break;
            case 1: m.shapeRect(x0, y0, x1, y1, false); break;
            case 2: m.shapeRect(x0, y0, x1, y1, true); break;
            default: m.floodFillMap(x0, y0, m.mapBrush); break;
        }
        golden::Fnv f;
        hs.hashMap(f);
        ASSERT_EQ(f.hex(), want.at(k)) << "сценарий " << k;
    }
}

TEST(Model, FloodFillTile) {
    Harness hs;
    Model& m = hs.m;
    const auto want = goldenHashes("flood");
    ASSERT_EQ(want.size(), 1000u);
    hs.seed = 99;
    for (int k = 0; k < 1000; ++k) {
        hs.initCollections();
        hs.resetBrushState();
        const int w = hs.nextIn(1, 30);
        const int h = hs.nextIn(1, 30);
        std::vector<Color> cols(static_cast<std::size_t>(hs.nextIn(1, 4)));
        for (auto& c : cols)
            c = hs.randColor();
        Grid<Color> t(w, h);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                t.at(x, y) =
                    cols[static_cast<std::size_t>(hs.nextIn(0, static_cast<int>(cols.size()) - 1))];
            }
        }
        m.tile = std::move(t);
        m.tileIdx.clear();
        m.rebindTileToPalette();
        const Color c = hs.randColor();
        const int sx = hs.nextIn(-1, w);
        const int sy = hs.nextIn(-1, h);
        m.floodFillTile(sx, sy, c);
        golden::Fnv f;
        hs.hashTile(f);
        ASSERT_EQ(f.hex(), want.at(k)) << "сценарий " << k;
    }
}

TEST(Model, PaletteOps) {
    Harness hs;
    Model& m = hs.m;
    const auto want = goldenHashes("palop");
    ASSERT_EQ(want.size(), 500u);
    hs.seed = 31337;
    for (int k = 0; k < 500; ++k) {
        hs.initCollections();
        hs.resetBrushState();
        for (int i = 0; i < 8; ++i) {
            for (int j = 0; j < 8; ++j) {
                if (hs.nextIn(0, 5) == 0) {
                    const int sj = hs.nextIn(0, 7);
                    const int si = hs.nextIn(0, 7);
                    m.palAt(j, i) = m.palAt(sj, si);
                }
            }
        }
        const int w = hs.nextIn(1, 20);
        const int h = hs.nextIn(1, 20);
        hs.randomTile(w, h);
        const int op = hs.nextIn(0, 4);
        int n = 0;
        switch (op) {
            case 0: m.quantizeTileToPalette(); break;
            case 1: n = m.rebindTileToPalette(); break;
            case 2: {
                const auto idx = static_cast<PalIndex>(hs.nextIn(0, 63));
                const Color oldC = m.palColorOf(idx);
                const Color newC = hs.randColor();
                m.palette[idx] = newC;
                n = m.recolorPaletteEntry(idx, oldC, newC);
                break;
            }
            case 3: {
                const int px = hs.nextIn(0, m.tile.w - 1);
                const int py = hs.nextIn(0, m.tile.h - 1);
                const Color from = m.tile.at(px, py);
                const Color to = hs.randColor();
                n = m.replaceColorEverywhere(from, to);
                break;
            }
            default:
                if (hs.nextIn(0, 1) == 0) {
                    m.flipTileH();
                } else {
                    m.flipTileV();
                }
                break;
        }
        const Color probe = hs.randColor();
        golden::Fnv f;
        hs.hashTile(f);
        f.i32(n);
        f.i32(Harness::idxByte(m.nearestPaletteIndex(probe)));
        f.i32(static_cast<int>(m.nearestPaletteColor(probe).argb()));
        f.i32(Harness::idxByte(m.indexOfColor(probe)));
        f.i32(m.boundPixelCount());
        ASSERT_EQ(f.hex(), want.at(k)) << "сценарий " << k << " операция " << op;
    }
}

TEST(Model, Buffers) {
    Harness hs;
    Model& m = hs.m;
    const auto want = goldenHashes("buffer");
    ASSERT_EQ(want.size(), 700u);
    hs.seed = 2024;
    for (int k = 0; k < 700; ++k) {
        hs.initCollections();
        hs.resetBrushState();
        const int w = hs.nextIn(2, 24);
        const int h = hs.nextIn(2, 24);
        hs.randomTile(w, h);
        m.hasSelection = true;
        m.selX = hs.nextIn(0, m.tile.w - 1);
        m.selY = hs.nextIn(0, m.tile.h - 1);
        m.selW = hs.nextIn(1, m.tile.w - m.selX);
        m.selH = hs.nextIn(1, m.tile.h - m.selY);
        m.copySelectionToBuffer();
        switch (hs.nextIn(0, 4)) {
            case 0: m.bufferFlipH(); break;
            case 1: m.bufferFlipV(); break;
            case 2: m.bufferRotate90(); break;
            default: break;
        }
        if (hs.nextIn(0, 1) == 0) {
            m.applyBufferToSelection();
        } else {
            const int px = hs.nextIn(-8, m.tile.w + 2);
            const int py = hs.nextIn(-8, m.tile.h + 2);
            m.pasteBufferAt(px, py);
        }
        golden::Fnv f;
        hs.hashTile(f);
        f.i32(m.buffer.w);
        f.i32(m.buffer.h);
        for (int y = 0; y < m.buffer.h; ++y) {
            for (int x = 0; x < m.buffer.w; ++x)
                f.i32(static_cast<int>(m.buffer.at(x, y).argb()));
        }
        ASSERT_EQ(f.hex(), want.at(k)) << "сценарий " << k;
    }
}

TEST(Model, ViewAndOverviewMath) {
    Harness hs;
    Model& m = hs.m;
    const auto want = goldenHashes("view");
    ASSERT_EQ(want.size(), 1000u);
    hs.seed = 555;
    for (int k = 0; k < 1000; ++k) {
        hs.initCollections();
        hs.resetBrushState();
        if (hs.nextIn(0, 3) == 0) {
            m.docMode = DOC_MAP;
            const int w = hs.nextIn(4, 256);
            const int h = hs.nextIn(4, 256);
            m.map.reset(w, h);
        } else {
            const int w = hs.nextIn(1, 512);
            const int h = hs.nextIn(1, 512);
            hs.emptyTile(w, h);
        }
        m.W = CANVAS_W;
        m.H = CANVAS_H;
        m.fitScale();
        golden::Fnv f;
        f.i32(m.pixelSize);
        f.i32(m.viewX);
        f.i32(m.viewY);
        const int steps = hs.nextIn(1, 8);
        for (int s = 1; s <= steps; ++s) {
            switch (hs.nextIn(0, 4)) {
                case 0: {
                    const int ax = hs.nextIn(0, m.W - 1);
                    const int ay = hs.nextIn(0, m.H - 1);
                    m.zoomAt(1, ax, ay);
                    break;
                }
                case 1: {
                    const int ax = hs.nextIn(0, m.W - 1);
                    const int ay = hs.nextIn(0, m.H - 1);
                    m.zoomAt(-1, ax, ay);
                    break;
                }
                case 2: {
                    m.viewX += hs.nextIn(-500, 500);
                    m.viewY += hs.nextIn(-500, 500);
                    m.clampView();
                    break;
                }
                case 3: {
                    const int px = hs.nextIn(PANEL_X - 10, PANEL_X + PANEL_W + 10);
                    const int py = hs.nextIn(OVER_Y0 - 10, OVER_Y1 + 10);
                    m.overviewCenterOn(px, py);
                    break;
                }
                default: m.fitScale(); break;
            }
            f.i32(m.pixelSize);
            f.i32(m.viewX);
            f.i32(m.viewY);
        }
        const int sx = hs.nextIn(-50, 650);
        const int sy = hs.nextIn(-50, 600);
        f.i32(m.screenToTileX(sx));
        f.i32(m.screenToTileY(sy));
        f.i32(m.tileToScreenX(sx));
        f.i32(m.tileToScreenY(sy));
        f.i32(m.overPixW());
        f.i32(m.overPixH());
        f.i32(m.overOriginX());
        f.i32(m.overOriginY());
        f.i32(m.undoDepth());
        m.gridOn = hs.nextIn(0, 1) == 1;
        f.i32(m.gridGap());
        int vx0 = 0;
        int vy0 = 0;
        int vx1 = 0;
        int vy1 = 0;
        m.overviewViewport(vx0, vy0, vx1, vy1);
        f.i32(vx0);
        f.i32(vy0);
        f.i32(vx1);
        f.i32(vy1);
        ASSERT_EQ(f.hex(), want.at(k)) << "сценарий " << k;
        m.docMode = DOC_TILE;
    }
}

TEST(Model, OverviewCanvasAndThumbs) {
    Harness hs;
    Model& m = hs.m;
    const auto want = goldenHashes("raster");
    ASSERT_EQ(want.size(), 300u);
    hs.seed = 8080;
    for (int k = 0; k < 300; ++k) {
        hs.initCollections();
        hs.resetBrushState();
        if (hs.nextIn(0, 3) == 0) {
            m.docMode = DOC_MAP;
            const int w = hs.nextIn(4, 120);
            const int h = hs.nextIn(4, 120);
            hs.randomMap(w, h);
        } else {
            const int w = hs.nextIn(1, 200);
            const int h = hs.nextIn(1, 200);
            hs.randomTile(w, h);
        }
        m.checkerOn = hs.nextIn(0, 1) == 1;
        golden::Fnv f;
        for (std::uint8_t b : m.buildOverviewRaw())
            f.byte(b);
        if (m.docMode == DOC_TILE) {
            for (std::uint8_t b : m.tileBmpRaw())
                f.byte(b);
            const int size = hs.nextIn(1, 64);
            const auto thumb = makeThumbRaw(m.tile, size);
            const auto alpha = makeThumbAlphaRaw(m.tile, size);
            for (std::size_t p = 0; p < thumb.size(); p += 4) {
                const Color c = argb(thumb[p + 3], thumb[p + 2], thumb[p + 1], thumb[p]);
                f.i32(static_cast<int>(c.argb()));
                const Color c2 = argb(alpha[p + 3], alpha[p + 2], alpha[p + 1], alpha[p]);
                f.byte(c2.a);
                if (c2.a == 255) f.i32(static_cast<int>(c2.argb()));
            }
        }
        ASSERT_EQ(f.hex(), want.at(k)) << "сценарий " << k;
        m.docMode = DOC_TILE;
    }
}

TEST(Model, WorkingColorsAndPick) {
    Harness hs;
    Model& m = hs.m;
    m.hue = m.sat = m.val = 0;
    const auto want = goldenHashes("pick");
    ASSERT_EQ(want.size(), 1000u);
    hs.seed = 1234;
    for (int k = 0; k < 1000; ++k) {
        hs.initCollections();
        hs.resetBrushState();
        const int w = hs.nextIn(1, 16);
        const int h = hs.nextIn(1, 16);
        hs.randomTile(w, h);
        m.firstColor = hs.randColor();
        m.secondColor = hs.randColor();
        m.firstIdx = m.indexOfColor(m.firstColor);
        m.secondIdx = static_cast<PalIndex>(hs.nextIn(0, 63));
        m.hue = hs.nextIn(0, 359);
        switch (hs.nextIn(0, 4)) {
            case 0: {
                const int x = hs.nextIn(-1, m.tile.w);
                const int y = hs.nextIn(-1, m.tile.h);
                m.pickColorFromTile(x, y);
                break;
            }
            case 1: m.swapColors(); break;
            case 2: m.syncHSVFromColor(); break;
            case 3:
                m.sat = hs.nextIn(0, 100);
                m.val = hs.nextIn(0, 100);
                m.hue = hs.nextIn(0, 359);
                m.applyHSVToColor();
                break;
            default: m.pickColor(hs.randColor()); break;
        }
        golden::Fnv f;
        f.i32(static_cast<int>(m.firstColor.argb()));
        f.i32(static_cast<int>(m.secondColor.argb()));
        f.i32(Harness::idxByte(m.firstIdx));
        f.i32(Harness::idxByte(m.secondIdx));
        f.i32(m.hue);
        f.i32(m.sat);
        f.i32(m.val);
        ASSERT_EQ(f.hex(), want.at(k)) << "сценарий " << k;
    }
}

TEST(Model, UndoHistory) {
    Harness hs;
    Model& m = hs.m;
    hs.initCollections();
    hs.resetBrushState();
    hs.seed = 4711;
    hs.randomTile(300, 300);
    m.clearTileHistory();
    const int limit = m.undoDepth();
    std::vector<std::string> got;
    got.push_back("undoDepth " + std::to_string(limit));
    for (int s = 1; s <= limit + 5; ++s) {
        m.pushUndo();
        m.tile.at(s % m.tile.w, 0) = rgb(s, s, s);
    }
    got.push_back("undoCount " + std::to_string(m.undoStack.size()));
    m.doUndo();
    m.doUndo();
    m.doUndo();
    got.push_back("afterUndo " + std::to_string(m.undoStack.size()) + " " +
                  std::to_string(m.redoStack.size()) + " " +
                  std::to_string(static_cast<int>(m.tile.at(0, 0).argb())) + " " +
                  std::to_string(static_cast<int>(m.tile.at(1, 0).argb())));
    m.doRedo();
    got.push_back("afterRedo " + std::to_string(m.undoStack.size()) + " " +
                  std::to_string(m.redoStack.size()) + " " +
                  std::to_string(static_cast<int>(m.tile.at(0, 0).argb())));
    m.pushUndo();
    got.push_back("afterPush " + std::to_string(m.undoStack.size()) + " " +
                  std::to_string(m.redoStack.size()));
    for (int sz = 1; sz <= 600; ++sz) {
        if (sz % 37 != 0) continue;
        m.tile = Grid<Color>(sz, sz / 2 + 1);
        got.push_back("depth " + std::to_string(sz) + " " + std::to_string(m.undoDepth()));
    }
    std::vector<std::string> want;
    for (const auto& w : editorGolden()) {
        if (w[0] == "undoDepth" || w[0] == "undoCount" || w[0] == "afterUndo" ||
            w[0] == "afterRedo" || w[0] == "afterPush" || w[0] == "depth") {
            std::string line = w[0];
            for (std::size_t i = 1; i < w.size(); ++i)
                line += " " + w[i];
            want.push_back(line);
        }
    }
    EXPECT_EQ(got, want);
}

TEST(Model, MapCellTileFiles) {
    Model m;
    m.map.reset(10, 6);
    m.map.at(3, 3) = MT_WATER;
    m.map.addObject("npc", 1, 1, "elder");
    m.map.addObject("npc", 2, 1, "");
    m.map.addObject("spawn", 3, 1, " rat 2 4");
    m.map.addObject("spawn", 4, 1, "   ");
    m.map.addObject("chest", 5, 1, "0 - bread:1");
    m.map.addObject("exit", 6, 1, "forest 2 8");
    m.map.addObject("npc", 6, 1, "smith  extra");
    std::vector<std::string> got;
    for (int y = 0; y < m.map.h; ++y) {
        for (int x = 0; x < m.map.w; ++x) {
            got.push_back("cell " + std::to_string(x) + " " + std::to_string(y) + " [" +
                          m.mapCellTileFile(x, y) + "]");
        }
    }
    got.push_back("cell -1 0 [" + m.mapCellTileFile(-1, 0) + "]");
    std::vector<std::string> want;
    for (const auto& w : editorGolden()) {
        if (w[0] != "cell") continue;
        std::string line = w[0];
        for (std::size_t i = 1; i < w.size(); ++i)
            line += " " + w[i];
        want.push_back(line);
    }
    EXPECT_EQ(got, want);
}

TEST(Model, PaletteFromPicture) {
    Model m;
    Grid<Color> g(13, 5);
    for (int y = 0; y < 5; ++y) {
        for (int x = 0; x < 13; ++x)
            g.at(x, y) = argb(255, x * 19, y * 50, (x * y * 11) % 256);
    }
    m.paletteFromGrid(g);
    const auto rows = goldenRows("palfile");
    ASSERT_EQ(rows.size(), 8u);
    for (int i = 0; i < PAL_ROWS; ++i) {
        for (int j = 0; j < PAL_COLS; ++j) {
            EXPECT_EQ(
                static_cast<int>(m.palAt(j, i).argb()),
                std::stoi(rows[static_cast<std::size_t>(i)][static_cast<std::size_t>(j) + 1]));
        }
    }
}
