// Связь с игрой (этап 7 RoadToProLevel, раздел 21 плана): карта
// картинками тайлов со стыками, тайл среди соседей настоящей карты и экран
// «Набор для игры» — лист 8x8 в формате игры, атлас, варианты стыков.

#include <algorithm>

#include "app/editor.h"
#include "core/anim.h"
#include "core/fsutil.h"
#include "core/gameset.h"
#include "core/imageops.h"
#include "core/pmath.h"
#include "core/png.h"
#include "core/theme.h"

namespace tjtm {

using namespace theme;

// ---------------------------------------------------------------------------
// Картинки набора
// ---------------------------------------------------------------------------

const Grid<Color>* Editor::gameTile(const std::string& name) {
    if (gameTilesDirty_) {
        gameTiles_.clear();
        gameTilesDirty_ = false;
    }
    auto it = gameTiles_.find(name);
    if (it == gameTiles_.end()) {
        Grid<Color> g;
        const std::string path = tsetPathOf(TS_BASE, name);
        std::string err;
        if (path.empty() || !fileExists(path) || !pngLoad(path, g, err)) g = Grid<Color>{};
        it = gameTiles_.emplace(name, std::move(g)).first;
    }
    return it->second.empty() ? nullptr : &it->second;
}

const Grid<Color>* Editor::mapCellImage(int x, int y) {
    if (!m.map.inside(x, y)) return nullptr;
    const std::string slot = tsetBaseFile(m.map.at(x, y));
    if (slot.empty()) return nullptr;
    // Вариант стыка — если он нарисован; иначе основной тайл.
    if (m.mapAutotile) {
        if (const Grid<Color>* v =
                gameTile(game::autotileName(slot, game::autotileMask(m.map, x, y))))
            return v;
    }
    return gameTile(slot);
}

int Editor::editedTerrain() const {
    std::string slot = m.metaValue("tileset.slot");
    if (slot.empty()) slot = pathStem(m.currentFile);
    for (int i = 0; i < TSET_BASE_COUNT; ++i) {
        if (tsetBaseIsTerrain(i) && tsetBaseFile(i) == slot) return i;
    }
    return -1;
}

bool Editor::mapContextCell(int& cx, int& cy, int& count) {
    const int t = editedTerrain();
    count = 0;
    if (t < 0 || m.map.w < 1) return false;
    for (int y = 0; y < m.map.h; ++y) {
        for (int x = 0; x < m.map.w; ++x)
            count += m.map.at(x, y) == t ? 1 : 0;
    }
    if (count == 0) return false;
    // Места местности по строкам; номер — по кругу.
    const int want = ((mapCtxIndex_ % count) + count) % count;
    int k = 0;
    for (int y = 0; y < m.map.h; ++y) {
        for (int x = 0; x < m.map.w; ++x) {
            if (m.map.at(x, y) != t) continue;
            if (k++ == want) {
                cx = x;
                cy = y;
                return true;
            }
        }
    }
    return false;
}

/// Соседи тайла с настоящей карты: клетки той же местности — живым тайлом
/// (так видно стык с собой), прочие — картинками набора, объекты — поверх.
void Editor::drawMapContext(int ox, int oy, int sw, int sh) {
    int cx = 0;
    int cy = 0;
    int count = 0;
    if (!mapContextCell(cx, cy, count)) return;
    const int t = editedTerrain();
    const int kx0 = floorDiv(m.X0 - ox, sw);
    const int kx1 = floorDiv(m.W - 1 - ox, sw);
    const int ky0 = floorDiv(m.Y0 - oy, sh);
    const int ky1 = floorDiv(m.H - 1 - oy, sh);
    for (int ky = ky0; ky <= ky1; ++ky) {
        for (int kx = kx0; kx <= kx1; ++kx) {
            if (kx == 0 && ky == 0) continue;
            const int mx = cx + kx;
            const int my = cy + ky;
            if (!m.map.inside(mx, my)) continue;
            const int sx = ox + kx * sw;
            const int sy = oy + ky * sh;
            if (m.map.at(mx, my) == t) {
                fb_.drawImage(tileBmp_, sx, sy, sw, sh);
            } else if (const Grid<Color>* img = mapCellImage(mx, my)) {
                fb_.drawImage(*img, sx, sy, sw, sh);
            } else {
                fb_.fillRect(sx, sy, sx + sw, sy + sh, mapTileColor(m.map.at(mx, my)));
            }
            const int o = m.map.objectAt(mx, my);
            if (o >= 0) {
                const std::string name = m.mapCellTileFile(mx, my);
                const Grid<Color>* img = gameTile(name);
                if (img == nullptr) {
                    const MapObject& obj = m.map.objects[static_cast<std::size_t>(o)];
                    img = gameTile(obj.kind == "spawn" ? "mob" : obj.kind);
                }
                if (img != nullptr) fb_.drawImage(*img, sx, sy, sw, sh);
            }
        }
    }
}

/// Где мы на карте — подписью в углу холста, поверх тайла и притенения.
void Editor::drawMapContextLabel() {
    int cx = 0;
    int cy = 0;
    int count = 0;
    if (!mapContextCell(cx, cy, count)) return;
    const std::string label = (m.map.name.empty() ? std::string("карта") : m.map.name) + ": " +
                              std::to_string(cx) + ", " + std::to_string(cy) + "   " +
                              std::to_string(((mapCtxIndex_ % count) + count) % count + 1) +
                              " из " + std::to_string(count) + "   Ctrl+B — дальше";
    const int lw = textWidth(label) + 12;
    fb_.fillRect(m.X0 + 4, m.Y0 + 4, m.X0 + 4 + lw, m.Y0 + 4 + textHeight() + 6, active.noteBack);
    textOut(m.X0 + 10, m.Y0 + 7, label, cText);
}

void Editor::toggleMapContext() {
    if (!m.mapContext && editedTerrain() < 0) {
        showMessage(
            "Тайл не из набора",
            "Окружение карты показывается у тайла местности: открытого на экране наборов "
            "(T, правая кнопка) или названного по слоту — wall.png, floor.png и так далее.");
        return;
    }
    m.mapContext = !m.mapContext;
    gameTilesDirty_ = true;
    if (!m.mapContext) {
        m.fitScale();
        return;
    }
    // Соседям нужно место: тайл во весь холст закрыл бы их. Влезают три на
    // три — видны все четыре стыка и углы.
    const int fit = std::max(MIN_ZOOM, std::min((m.W - m.X0) / (3 * std::max(1, m.docW())),
                                                (m.H - m.Y0) / (3 * std::max(1, m.docH()))));
    if (fit < m.pixelSize) {
        m.pixelSize = fit;
        m.clampView();
    }
}

// ---------------------------------------------------------------------------
// Экран «Набор для игры»
// ---------------------------------------------------------------------------

void Editor::exportGameSheet() {
    std::vector<Grid<Color>> slots(static_cast<std::size_t>(game::kSheetSlots));
    int found = 0;
    for (int i = 0; i < TSET_BASE_COUNT; ++i) {
        if (const Grid<Color>* g = gameTile(tsetBaseFile(i))) {
            slots[static_cast<std::size_t>(i)] = *g;
            ++found;
        }
    }
    if (found == 0) {
        showMessage("Набор пуст", "В папке набора нет ни одного основного тайла.");
        return;
    }
    Grid<Color> sheet;
    std::string err;
    if (!game::composeSheet(slots, sheet, err)) {
        showMessage("Лист не собран", err);
        return;
    }
    const std::string path = askExportFile("Лист 8×8 для игры", "tiles.png", "png", "PNG");
    if (path.empty()) return;
    if (!pngSave(path, sheet, err)) errorSave("Не удалось сохранить лист", path + ": " + err);
}

void Editor::exportGameAtlas() {
    const std::string dir = tilesDir();
    std::vector<game::AtlasEntry> entries;
    for (const std::string& f : listFiles(dir)) {
        if (pathExtLower(f) != ".png") continue;
        Grid<Color> g;
        std::string err;
        if (pngLoad(pathCombine(dir, f), g, err)) entries.push_back({pathStem(f), std::move(g)});
    }
    if (entries.empty()) {
        showMessage("Набор пуст", "В папке набора нет картинок.");
        return;
    }
    std::vector<game::AtlasPlace> places;
    const Grid<Color> atlas = game::packAtlas(entries, 1024, 1, places);
    const std::string path = askExportFile("Атлас набора", "atlas.png", "png", "PNG");
    if (path.empty()) return;
    std::string err;
    if (!pngSave(path, atlas, err)) {
        errorSave("Не удалось сохранить атлас", path + ": " + err);
        return;
    }
    const std::string json = game::atlasJson(places, pathFileName(path), atlas.w, atlas.h);
    const std::string jsonPath = path.substr(0, path.size() - pathExtLower(path).size()) + ".json";
    if (!writeFile(jsonPath, json))
        errorSave("Не удалось сохранить описание", jsonPath + ": не удалось записать файл.");
}

void Editor::sliceGameSheetAsk() {
    const std::string dir = tilesDir();
    const std::string path = askOpenFile("Лист 8×8 игры");
    if (path.empty()) return;
    Grid<Color> sheet;
    std::string err;
    std::vector<Grid<Color>> slots;
    if (!pngLoad(path, sheet, err) || !game::sliceSheet(sheet, slots, err)) {
        errorOpen("Лист не разрезан", path + ": " + err);
        return;
    }
    int n = 0;
    for (int i = 0; i < TSET_BASE_COUNT; ++i)
        n += slots[static_cast<std::size_t>(i)].empty() ? 0 : 1;
    if (!askYesNo("Разрезать лист",
                  "В листе " + std::to_string(n) + " основных тайлов. Записать их в папку набора " +
                      dir + "? Файлы с теми же именами заменятся.",
                  "Записать", "Отмена"))
        return;
    for (int i = 0; i < TSET_BASE_COUNT; ++i) {
        const Grid<Color>& g = slots[static_cast<std::size_t>(i)];
        if (g.empty()) continue;
        if (!pngSave(tsetPathOf(TS_BASE, tsetBaseFile(i)), g, err)) {
            errorSave("Не удалось записать тайл", tsetBaseFile(i) + ": " + err);
            break;
        }
    }
    gameTilesDirty_ = true;
}

bool Editor::autotileTemplate(int terrain) {
    const std::string slot = tsetBaseFile(terrain);
    const Grid<Color>* base = gameTile(slot);
    if (base == nullptr) {
        showMessage("Нет основного тайла", "Сначала нарисуйте " + slot +
                                               ".png: варианты стыков "
                                               "начинаются с него.");
        return false;
    }
    if (!confirmDiscard("Открыть заготовку вариантов?")) return false;
    // Шестнадцать вариантов сеткой 4x4; нарисованные — как есть, прочие —
    // копией основного тайла: править удобнее, чем рисовать с нуля.
    std::vector<Grid<Color>> variants;
    for (int k = 0; k < 16; ++k) {
        const Grid<Color>* v = gameTile(game::autotileName(slot, k));
        variants.push_back(v != nullptr ? *v : *base);
    }
    const Grid<Color> sheet = game::autotileSheet(variants, base->w, base->h);
    if (sheet.w > MAX_TILE || sheet.h > MAX_TILE) {
        showMessage("Тайл великоват", "Сетка 4x4 из тайлов " + std::to_string(base->w) + "×" +
                                          std::to_string(base->h) + " больше " +
                                          std::to_string(MAX_TILE) + " точек.");
        return false;
    }
    m.setTile(sheet, "");
    m.setMeta("autotile.slot", slot);
    m.majorGridW = base->w;
    m.majorGridH = base->h;
    m.guideCellCenters = false;
    m.tileModified = true;
    showMessage("Заготовка стыков «" + slot + "»",
                "Клетка k сетки 4x4 — вариант с соседями по маске k: север 1, восток 2, юг 4, "
                "запад 8 (клетка 5 — соседи сверху и снизу). Нарисуйте варианты и разрежьте их "
                "кнопкой «Разрезать на варианты» на экране «Набор для игры».");
    return true;
}

void Editor::autotileSlice() {
    const std::string slot = m.metaValue("autotile.slot");
    if (slot.empty()) return;
    m.commitActiveCel();
    const std::vector<Grid<Color>> variants = game::sliceAutotileSheet(m.compositeFrame(0));
    std::string err;
    std::string failed;
    for (std::size_t k = 0; k < variants.size() && failed.empty(); ++k) {
        const std::string name = game::autotileName(slot, static_cast<int>(k));
        const std::string path = tsetPathOf(TS_BASE, name);
        if (path.empty() || !pngSave(path, variants[k], err)) failed = name + ".png";
    }
    gameTilesDirty_ = true;
    if (!failed.empty()) {
        errorSave("Не удалось записать вариант", failed + ": " + err);
        return;
    }
    std::string text = "Шестнадцать файлов ";
    text += slot + "_0.png … " + slot;
    text += "_15.png — в папке набора. Карта картинками уже подбирает их по соседям.";
    notify("Варианты стыков записаны", text);
}

bool Editor::showGameSetScreen() {
    bool done = false;
    bool opened = false;
    dropInput();
    pf_.input().keys.clear();
    constexpr int LX = 40;
    constexpr int RX = 410;
    constexpr int BW = 330;
    while (!done && !quitNow_) {
        beginFrameInput();
        fb_.clear(cWindow);
        textCentered(0, 12, WIN_W, 40, "Набор для игры", cText);
        const std::string dir = tilesDir();
        uiLabel(0, 40, WIN_W, 20, dir.empty() ? "Папка набора не задана" : fitText(dir, WIN_W - 40),
                dir.empty() ? cWarn : cTextDim);
        const bool have = !dir.empty();

        uiLabelLeft(LX, 70, 24, "Выгрузка набора", cTextDim);
        if (uiButton(LX, 96, BW, 30, "Лист 8×8 для игры…", have)) exportGameSheet();
        uiLabelLeft(LX, 130, 20, "Формат игры: маркер-разделитель, тайлы до 63 точек", cTextDim);
        if (uiButton(LX, 160, BW, 30, "Атлас всего набора и JSON…", have)) exportGameAtlas();
        uiLabelLeft(LX, 194, 20, "Все картинки папки, описание в виде TexturePacker", cTextDim);
        if (uiButton(LX, 224, BW, 30, "Разрезать лист 8×8 в набор…", have)) sliceGameSheetAsk();

        uiLabelLeft(LX, 276, 24, "Карта", cTextDim);
        if (uiToggle(LX, 302, BW, 30, "Карта картинками тайлов   Shift+B", m.mapTiles)) {
            m.mapTiles = !m.mapTiles;
            gameTilesDirty_ = true;
        }
        if (uiToggle(LX, 338, BW, 30, "Стыки по соседям на карте", m.mapAutotile)) {
            m.mapAutotile = !m.mapAutotile;
            gameTilesDirty_ = true;
        }
        if (uiToggle(LX, 374, BW, 30, "Тайл в окружении карты   Shift+B", m.mapContext))
            toggleMapContext();

        uiLabelLeft(RX, 70, 24, "Стыки местности: 16 вариантов по соседям", cTextDim);
        for (int i = 0; i < 7; ++i) {
            if (uiButton(RX + (i % 2) * 168, 96 + (i / 2) * 36, 162, 30,
                         "Заготовка: " + tsetBaseName(i), have)) {
                opened = autotileTemplate(i);
                done = opened;
            }
        }
        const std::string slot = m.metaValue("autotile.slot");
        if (uiButton(RX, 246, BW, 30,
                     slot.empty() ? "Разрезать на варианты — нет заготовки"
                                  : "Разрезать на варианты «" + slot + "»",
                     have && !slot.empty() && m.docMode == DOC_TILE))
            autotileSlice();
        uiLabelLeft(RX, 286, 20, "Вариант — файл «слот_маска.png»: север 1,", cTextDim);
        uiLabelLeft(RX, 306, 20, "восток 2, юг 4, запад 8; за краем карты —", cTextDim);
        uiLabelLeft(RX, 326, 20, "та же местность. wall_5 — стена с соседями", cTextDim);
        uiLabelLeft(RX, 346, 20, "сверху и снизу. Игра пока читает только", cTextDim);
        uiLabelLeft(RX, 366, 20, "основной тайл; правило — для её перехода.", cTextDim);

        if (uiButton(WIN_W / 2 - 90, WIN_H - 52, 180, 36, "Закрыть", true)) done = true;
        redraw();
        const int key = takeKey();
        if (key == KEY_ESC || key == KEY_ENTER) done = true;
        idle();
    }
    waitMouseRelease();
    gameTilesDirty_ = true;
    m.needRepaint = true;
    return opened;
}

}  // namespace tjtm
