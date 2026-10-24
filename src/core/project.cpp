#include "core/project.h"

#include <algorithm>
#include <cstring>
#include <memory>

#include "core/fsutil.h"
#include "core/model.h"
#include "core/png.h"
#include "core/zlib.h"

namespace tjtm {

namespace {

constexpr int kMaxLayers = 256;
constexpr int kMaxFrames = 4096;

// --- запись -----------------------------------------------------------------

struct Writer {
    std::vector<std::uint8_t> b;
    void u8(int v) { b.push_back(static_cast<std::uint8_t>(v)); }
    void u16(unsigned v) {
        u8(static_cast<int>(v & 0xFF));
        u8(static_cast<int>((v >> 8) & 0xFF));
    }
    void u32(std::uint32_t v) {
        for (int k = 0; k < 4; ++k)
            u8(static_cast<int>((v >> (8 * k)) & 0xFF));
    }
    void i32(int v) { u32(static_cast<std::uint32_t>(v)); }
    void str(const std::string& s) {
        u32(static_cast<std::uint32_t>(s.size()));
        b.insert(b.end(), s.begin(), s.end());
    }
    void section(const char* tag, const Writer& body) {
        b.insert(b.end(), tag, tag + 4);
        u32(static_cast<std::uint32_t>(body.b.size()));
        b.insert(b.end(), body.b.begin(), body.b.end());
    }
};

// --- чтение -----------------------------------------------------------------

struct Reader {
    const std::uint8_t* p = nullptr;
    std::size_t n = 0;
    std::size_t pos = 0;
    bool bad = false;
    bool need(std::size_t k) {
        if (bad || n - pos < k) {
            bad = true;
            return false;
        }
        return true;
    }
    int u8() { return need(1) ? p[pos++] : 0; }
    unsigned u16() {
        if (!need(2)) return 0;
        const unsigned v = p[pos] | (static_cast<unsigned>(p[pos + 1]) << 8);
        pos += 2;
        return v;
    }
    std::uint32_t u32() {
        if (!need(4)) return 0;
        std::uint32_t v = 0;
        for (int k = 3; k >= 0; --k)
            v = (v << 8) | p[pos + static_cast<std::size_t>(k)];
        pos += 4;
        return v;
    }
    int i32() { return static_cast<int>(u32()); }
    std::string str() {
        const std::uint32_t len = u32();
        if (!need(len)) return {};
        std::string s(reinterpret_cast<const char*>(p + pos), len);
        pos += len;
        return s;
    }
};

/// Разобранный проект — сперва целиком сюда, в модель только при успехе.
struct Parsed {
    int w = 0;
    int h = 0;
    bool indexed = false;
    int activeLayer = 0;
    int activeFrame = 0;
    std::vector<Color> palette;
    PalIndex transparentIdx = IDX_NONE;
    SpriteDoc doc;
    Grid<std::uint8_t> sel;
    bool haveView = false;
    int pixelSize = 0;
    unsigned flags = 0;
    int axisX2 = -1;
    int axisY2 = -1;
    int dither = 0;
    bool onion = false;
    int onionPrev = 1;
    int onionNext = 1;
    Color first;
    Color second;
    bool haveGuides = false;
    unsigned guideFlags = 0;
    int majorW = 0, majorH = 0;
    int originX = 0, originY = 0;
    std::vector<int> guidesX2, guidesY2;
    int radial = 0;
    int opacity = 255;
    std::vector<std::pair<std::string, std::string>> meta;
};

bool parseBody(const std::vector<std::uint8_t>& body, Parsed& out, std::string& error) {
    Reader r{body.data(), body.size()};
    if (body.size() < 8 || std::memcmp(body.data(), "TJTP", 4) != 0) {
        error = "кусок проекта испорчен";
        return false;
    }
    r.pos = 4;
    const std::uint32_t version = r.u32();
    if (version > kProjectVersion) {
        error = "проект записан более новой версией редактора";
        return false;
    }
    bool haveHead = false;
    bool haveLayers = false;
    bool haveFrames = false;
    std::vector<std::pair<std::pair<int, int>, Cel>> cels;
    while (r.pos + 8 <= r.n) {
        const std::string tag(reinterpret_cast<const char*>(r.p + r.pos), 4);
        r.pos += 4;
        const std::uint32_t len = r.u32();
        if (!r.need(len)) break;
        Reader s{r.p + r.pos, len};
        r.pos += len;
        if (tag == "HEAD") {
            out.w = static_cast<int>(s.u32());
            out.h = static_cast<int>(s.u32());
            out.indexed = s.u8() != 0;
            out.activeLayer = static_cast<int>(s.u32());
            out.activeFrame = static_cast<int>(s.u32());
            haveHead = !s.bad;
        } else if (tag == "PALT") {
            const std::uint32_t n = s.u32();
            if (n > 0x10000) {
                error = "палитра слишком длинная";
                return false;
            }
            for (std::uint32_t k = 0; k < n && !s.bad; ++k)
                out.palette.push_back(Color::fromArgb(s.u32()));
            const std::uint32_t t = s.u32();
            out.transparentIdx = t < out.palette.size() ? static_cast<PalIndex>(t) : IDX_NONE;
        } else if (tag == "LAYR") {
            const std::uint32_t n = s.u32();
            if (n == 0 || n > kMaxLayers) {
                error = "неверное число слоёв";
                return false;
            }
            for (std::uint32_t k = 0; k < n && !s.bad; ++k) {
                Layer l;
                l.name = s.str();
                l.visible = s.u8() != 0;
                l.locked = s.u8() != 0;
                l.opacity = s.u8();
                out.doc.layers.push_back(std::move(l));
            }
            haveLayers = !s.bad;
        } else if (tag == "FRAM") {
            const std::uint32_t n = s.u32();
            if (n == 0 || n > kMaxFrames) {
                error = "неверное число кадров";
                return false;
            }
            for (std::uint32_t k = 0; k < n && !s.bad; ++k)
                out.doc.frames.push_back(Frame{std::clamp(static_cast<int>(s.u32()), 10, 60000)});
            haveFrames = !s.bad;
        } else if (tag == "TAGS") {
            const std::uint32_t n = s.u32();
            for (std::uint32_t k = 0; k < n && k < 10000 && !s.bad; ++k) {
                AnimTag t;
                t.name = s.str();
                t.from = static_cast<int>(s.u32());
                t.to = static_cast<int>(s.u32());
                t.direction = std::clamp(s.u8(), TAG_FORWARD, TAG_PINGPONG);
                out.doc.tags.push_back(std::move(t));
            }
        } else if (tag == "CELS") {
            if (!haveHead) {
                error = "ячейки раньше заголовка";
                return false;
            }
            const std::uint32_t n = s.u32();
            const auto px = static_cast<std::size_t>(out.w) * static_cast<std::size_t>(out.h);
            for (std::uint32_t k = 0; k < n && !s.bad; ++k) {
                const int l = static_cast<int>(s.u32());
                const int f = static_cast<int>(s.u32());
                if (!s.need(px * 6)) break;
                Cel c{Grid<Color>(out.w, out.h), Grid<PalIndex>(out.w, out.h, IDX_NONE)};
                for (std::size_t q = 0; q < px; ++q) {
                    const std::uint8_t* d = s.p + s.pos + q * 4;
                    c.img.d[q] = argb(d[3], d[0], d[1], d[2]);
                }
                s.pos += px * 4;
                for (std::size_t q = 0; q < px; ++q)
                    c.idx.d[q] = static_cast<PalIndex>(s.u16());
                cels.emplace_back(std::make_pair(l, f), std::move(c));
            }
            if (s.bad) {
                error = "ячейки обрезаны";
                return false;
            }
        } else if (tag == "SELM") {
            if (s.u8() != 0 && haveHead) {
                const auto px = static_cast<std::size_t>(out.w) * static_cast<std::size_t>(out.h);
                if (s.need(px)) {
                    out.sel.resize(out.w, out.h, 0);
                    std::memcpy(out.sel.d.data(), s.p + s.pos, px);
                }
            }
        } else if (tag == "VIEW") {
            out.pixelSize = static_cast<int>(s.u32());
            out.flags = s.u32();
            out.axisX2 = s.i32();
            out.axisY2 = s.i32();
            out.dither = static_cast<int>(s.u32());
            out.onion = s.u8() != 0;
            out.onionPrev = s.u8();
            out.onionNext = s.u8();
            out.first = Color::fromArgb(s.u32());
            out.second = Color::fromArgb(s.u32());
            out.haveView = !s.bad;
        } else if (tag == "GUID") {
            out.guideFlags = static_cast<unsigned>(s.u8());
            out.majorW = static_cast<int>(s.u32());
            out.majorH = static_cast<int>(s.u32());
            out.originX = s.i32();
            out.originY = s.i32();
            for (auto* list : {&out.guidesX2, &out.guidesY2}) {
                const std::uint32_t n = s.u32();
                for (std::uint32_t k = 0; k < n && k < 4096 && !s.bad; ++k)
                    list->push_back(s.i32());
            }
            out.radial = s.u8();
            out.opacity = std::max(1, s.u8());
            out.haveGuides = !s.bad;
        } else if (tag == "META") {
            const std::uint32_t n = s.u32();
            for (std::uint32_t k = 0; k < n && k < 4096 && !s.bad; ++k) {
                std::string key = s.str();
                std::string value = s.str();
                // Имя программы пишется заново при каждом сохранении.
                if (!s.bad && key != "app") out.meta.emplace_back(std::move(key), std::move(value));
            }
        }
        // Незнакомый раздел пропускается целиком: длина его известна.
    }
    if (!haveHead || !haveLayers || !haveFrames) {
        error = "в проекте нет заголовка, слоёв или кадров";
        return false;
    }
    if (out.w < 1 || out.h < 1 || out.w > MAX_TILE || out.h > MAX_TILE) {
        error = "недопустимый размер проекта";
        return false;
    }
    if (out.palette.empty()) {
        error = "в проекте нет палитры";
        return false;
    }
    out.doc.cels.assign(out.doc.layers.size(), std::vector<CelPtr>(out.doc.frames.size()));
    for (auto& [lf, c] : cels) {
        if (lf.first < 0 || lf.second < 0 || lf.first >= out.doc.layerCount() ||
            lf.second >= out.doc.frameCount())
            continue;
        out.doc.setCel(lf.first, lf.second, std::make_shared<const Cel>(std::move(c)));
    }
    for (AnimTag& t : out.doc.tags) {
        t.from = std::clamp(t.from, 0, out.doc.frameCount() - 1);
        t.to = std::clamp(t.to, t.from, out.doc.frameCount() - 1);
    }
    out.activeLayer = std::clamp(out.activeLayer, 0, out.doc.layerCount() - 1);
    out.activeFrame = std::clamp(out.activeFrame, 0, out.doc.frameCount() - 1);
    return true;
}

// Флаги раздела VIEW.
constexpr unsigned VF_GRID = 1, VF_CHECKER = 2, VF_SEAMLESS = 4, VF_WRAP = 8, VF_SYMX = 16,
                   VF_SYMY = 32, VF_PERFECT = 64, VF_REPLACE = 128;

}  // namespace

std::vector<std::uint8_t> projectEncode(Model& m) {
    m.commitActiveCel();
    const int w = m.tile.w;
    const int h = m.tile.h;
    Writer all;
    all.b = {'T', 'J', 'T', 'P'};
    all.u32(kProjectVersion);

    Writer head;
    head.u32(static_cast<std::uint32_t>(w));
    head.u32(static_cast<std::uint32_t>(h));
    head.u8(m.indexedOn ? 1 : 0);
    head.u32(static_cast<std::uint32_t>(m.activeLayer));
    head.u32(static_cast<std::uint32_t>(m.activeFrame));
    all.section("HEAD", head);

    Writer palt;
    palt.u32(static_cast<std::uint32_t>(m.palette.size()));
    for (const Color c : m.palette)
        palt.u32(c.argb());
    palt.u32(m.transparentIdx == IDX_NONE ? 0xFFFFFFFFu : m.transparentIdx);
    all.section("PALT", palt);

    Writer layr;
    layr.u32(static_cast<std::uint32_t>(m.doc.layerCount()));
    for (const Layer& l : m.doc.layers) {
        layr.str(l.name);
        layr.u8(l.visible ? 1 : 0);
        layr.u8(l.locked ? 1 : 0);
        layr.u8(l.opacity);
    }
    all.section("LAYR", layr);

    Writer fram;
    fram.u32(static_cast<std::uint32_t>(m.doc.frameCount()));
    for (const Frame& f : m.doc.frames)
        fram.u32(static_cast<std::uint32_t>(f.durationMs));
    all.section("FRAM", fram);

    Writer tags;
    tags.u32(static_cast<std::uint32_t>(m.doc.tags.size()));
    for (const AnimTag& t : m.doc.tags) {
        tags.str(t.name);
        tags.u32(static_cast<std::uint32_t>(t.from));
        tags.u32(static_cast<std::uint32_t>(t.to));
        tags.u8(t.direction);
    }
    all.section("TAGS", tags);

    Writer cels;
    std::uint32_t count = 0;
    for (int l = 0; l < m.doc.layerCount(); ++l) {
        for (int f = 0; f < m.doc.frameCount(); ++f) {
            if (m.doc.cel(l, f)) ++count;
        }
    }
    cels.u32(count);
    for (int l = 0; l < m.doc.layerCount(); ++l) {
        for (int f = 0; f < m.doc.frameCount(); ++f) {
            const CelPtr c = m.doc.cel(l, f);
            if (!c) continue;
            cels.u32(static_cast<std::uint32_t>(l));
            cels.u32(static_cast<std::uint32_t>(f));
            for (const Color px : c->img.d) {
                cels.u8(px.r);
                cels.u8(px.g);
                cels.u8(px.b);
                cels.u8(px.a);
            }
            for (std::size_t q = 0; q < c->img.d.size(); ++q)
                cels.u16(q < c->idx.d.size() ? c->idx.d[q] : IDX_NONE);
        }
    }
    all.section("CELS", cels);

    Writer selm;
    selm.u8(m.hasSelection ? 1 : 0);
    if (m.hasSelection) {
        const Grid<std::uint8_t> mask = m.selectionMask();
        selm.b.insert(selm.b.end(), mask.d.begin(), mask.d.end());
    }
    all.section("SELM", selm);

    Writer view;
    view.u32(static_cast<std::uint32_t>(m.pixelSize));
    unsigned flags = 0;
    flags |= m.gridOn ? VF_GRID : 0U;
    flags |= m.checkerOn ? VF_CHECKER : 0U;
    flags |= m.seamlessOn ? VF_SEAMLESS : 0U;
    flags |= m.wrapOn ? VF_WRAP : 0U;
    flags |= m.symX ? VF_SYMX : 0U;
    flags |= m.symY ? VF_SYMY : 0U;
    flags |= m.pixelPerfect ? VF_PERFECT : 0U;
    flags |= m.brushReplace ? VF_REPLACE : 0U;
    view.u32(flags);
    view.i32(m.symAxisX2);
    view.i32(m.symAxisY2);
    view.u32(static_cast<std::uint32_t>(m.ditherPattern));
    view.u8(m.onionSkin ? 1 : 0);
    view.u8(m.onionPrev);
    view.u8(m.onionNext);
    view.u32(m.firstColor.argb());
    view.u32(m.secondColor.argb());
    all.section("VIEW", view);

    // Направляющие, сетка, начало координат, лучи и сила кисти (этап 5) —
    // отдельным разделом: версия 1.0 его пропустит, не потеряв остального.
    Writer guid;
    guid.u8((m.guidesOn ? 1 : 0) | (m.guideCenter ? 2 : 0) | (m.guideCellCenters ? 4 : 0) |
            (m.rulersOn ? 8 : 0));
    guid.u32(static_cast<std::uint32_t>(std::max(0, m.majorGridW)));
    guid.u32(static_cast<std::uint32_t>(std::max(0, m.majorGridH)));
    guid.i32(m.originX);
    guid.i32(m.originY);
    for (const auto* list : {&m.guidesX2, &m.guidesY2}) {
        guid.u32(static_cast<std::uint32_t>(list->size()));
        for (const int v : *list)
            guid.i32(v);
    }
    guid.u8(std::clamp(m.symRadial, 0, 255));
    guid.u8(std::clamp(m.brushOpacity, 1, 255));
    all.section("GUID", guid);

    Writer meta;
    meta.u32(static_cast<std::uint32_t>(1 + m.docMeta.size()));
    meta.str("app");
    meta.str(std::string(kAppTitle) + " " + kAppVersion);
    for (const auto& [k, v] : m.docMeta) {
        meta.str(k);
        meta.str(v);
    }
    all.section("META", meta);

    // Картинка файла — сведённый первый кадр: так проект узнают в
    // проводнике и в любом просмотрщике.
    return pngEncodeWith(m.compositeFrame(0), {PngChunk{kProjectChunk, zlibCompress(all.b, 9)}}, 9);
}

bool isProjectFile(const std::vector<std::uint8_t>& file) {
    std::vector<std::uint8_t> data;
    return pngFindChunk(file, kProjectChunk, data);
}

bool projectDecode(const std::vector<std::uint8_t>& file, Model& m, std::string& error) {
    std::vector<std::uint8_t> packed;
    if (!pngFindChunk(file, kProjectChunk, packed)) {
        error = "это не проект редактора: в PNG нет куска проекта";
        return false;
    }
    std::vector<std::uint8_t> body;
    if (!zlibDecompress(packed.data(), packed.size(), body, error, std::size_t(1) << 30)) {
        error = "кусок проекта испорчен: " + error;
        return false;
    }
    Parsed p;
    if (!parseBody(body, p, error)) return false;

    m.docMode = DOC_TILE;
    m.palette = std::move(p.palette);
    m.transparentIdx = p.transparentIdx;
    m.indexedOn = p.indexed;
    m.doc = std::move(p.doc);
    m.activeLayer = p.activeLayer;
    m.activeFrame = p.activeFrame;
    m.tile.resize(p.w, p.h, argb(0, 0, 0, 0));
    m.tileIdx.resize(p.w, p.h, IDX_NONE);
    m.loadActiveCel();
    m.resetSelection();
    if (!p.sel.empty()) m.setSelectionMask(p.sel);
    if (p.haveView) {
        m.gridOn = (p.flags & VF_GRID) != 0;
        m.checkerOn = (p.flags & VF_CHECKER) != 0;
        m.seamlessOn = (p.flags & VF_SEAMLESS) != 0;
        m.wrapOn = (p.flags & VF_WRAP) != 0;
        m.symX = (p.flags & VF_SYMX) != 0;
        m.symY = (p.flags & VF_SYMY) != 0;
        m.pixelPerfect = (p.flags & VF_PERFECT) != 0;
        m.brushReplace = (p.flags & VF_REPLACE) != 0;
        m.symAxisX2 = p.axisX2;
        m.symAxisY2 = p.axisY2;
        m.ditherPattern = p.dither;
        m.onionSkin = p.onion;
        m.onionPrev = std::clamp(p.onionPrev, 0, 8);
        m.onionNext = std::clamp(p.onionNext, 0, 8);
        m.firstColor = p.first;
        m.secondColor = p.second;
        m.firstIdx = m.indexOfColor(m.firstColor);
        m.secondIdx = m.indexOfColor(m.secondColor);
        m.syncHSVFromColor();
    }
    // Направляющие и метаданные — свои у документа: у проекта без раздела
    // GUID их просто нет.
    m.guidesX2 = p.guidesX2;
    m.guidesY2 = p.guidesY2;
    m.originX = p.originX;
    m.originY = p.originY;
    m.docMeta = p.meta;
    if (p.haveGuides) {
        m.guidesOn = (p.guideFlags & 1U) != 0;
        m.guideCenter = (p.guideFlags & 2U) != 0;
        m.guideCellCenters = (p.guideFlags & 4U) != 0;
        m.rulersOn = (p.guideFlags & 8U) != 0;
        m.majorGridW = std::clamp(p.majorW, 0, MAX_TILE);
        m.majorGridH = std::clamp(p.majorH, 0, MAX_TILE);
        m.symRadial = std::clamp(p.radial, 0, 32);
        m.brushOpacity = std::clamp(p.opacity, 1, 255);
    }
    m.clearTileHistory();
    m.tileModified = false;
    m.palScroll = 0;
    m.tileBmpDirty = true;
    m.overDirty = true;
    m.needRepaint = true;
    if (p.haveView && p.pixelSize >= MIN_ZOOM && p.pixelSize <= MAX_ZOOM) {
        m.pixelSize = p.pixelSize;
        m.clampView();
    } else {
        m.fitScale();
    }
    return true;
}

bool projectSave(const std::string& path, Model& m, std::string& error) {
    const std::vector<std::uint8_t> data = projectEncode(m);
    if (!writeFile(path,
                   std::string_view(reinterpret_cast<const char*>(data.data()), data.size()))) {
        error = "не удалось записать файл";
        return false;
    }
    return true;
}

bool projectLoad(const std::string& path, Model& m, std::string& error) {
    const auto text = readFile(path);
    if (!text) {
        error = "не открыть файл";
        return false;
    }
    const std::vector<std::uint8_t> data(text->begin(), text->end());
    return projectDecode(data, m, error);
}

}  // namespace tjtm
