// Клавиатура редактора (план «Интерфейс», этап B, UX-18 и UX-09).
//
// Интерфейс 1.7+ разбирает нажатие по таблице core/shortcuts.cpp, «Классика»
// — прежней цепочкой 1.6 (handleClassicKey). Что делает каждая команда,
// написано один раз — в runCommand: обе раскладки ведут сюда, и шпаргалка
// не расходится с тем, что делает клавиша.

#include <algorithm>

#include "app/editor.h"
#include "core/imageops.h"
#include "core/text.h"
#include "core/theme.h"

namespace tjtm {

using namespace theme;

void Editor::handleEditorKey(int key) {
    if (classicUi_) {
        handleClassicKey(key);
        return;
    }
    const int mods = (lastKey_.shift ? MOD_SHIFT : 0) | (lastKey_.ctrl ? MOD_CTRL : 0) |
                     (lastKey_.alt ? MOD_ALT : 0);
    if (const Shortcut* s = findShortcut(key, mods, m.docMode == DOC_TILE))
        runCommand(s->cmd, s->arg);
}

void Editor::handleClassicKey(int key) {
    const bool tile = m.docMode == DOC_TILE;
    const bool ctrl = lastKey_.ctrl;
    const bool shift = lastKey_.shift;
    auto run = [&](Cmd c, int arg = 0) { runCommand(c, arg); };
    // Порядок — как у 1.6: сначала сочетания с Ctrl, потом клавиши этапов
    // RoadToProLevel, потом ленты кадров и только затем однобуквенные
    // команды оригинала. Ctrl, не взятый сочетанием, буква не замечает.
    if (ctrl) {
        switch (key) {
            case KEY_R: return run(Cmd::Rulers);
            case KEY_SEMICOLON: return run(Cmd::Guides);
            case KEY_G: return run(Cmd::GridScreen);
            default: break;
        }
    }
    if (tile && ctrl) {
        switch (key) {
            case KEY_T: return run(Cmd::TilePicker);
            case KEY_E: return run(Cmd::BrushFromSelection);
            case KEY_A: return run(Cmd::SelectAll);
            case KEY_D: return run(shift ? Cmd::Reselect : Cmd::Deselect);
            case KEY_I: return run(Cmd::InvertSelection);
            case KEY_C: return run(Cmd::Copy);
            case KEY_X: return run(Cmd::Cut);
            case KEY_V: return run(Cmd::PasteClipboard);
            default: break;
        }
    }
    if (tile) {
        switch (key) {
            case KEY_V: return run(Cmd::PasteHere);
            case KEY_DELETE:
                // Без выделения у 1.6 Delete очищал весь тайл — ниже.
                if (m.activeKind() == LK_VECTOR || m.hasSelection) return run(Cmd::EraseSelection);
                break;
            case KEY_Q: return run(Cmd::PixelPerfect);
            case KEY_O: return run(Cmd::ToolOptions);
            case KEY_K: return run(shift ? Cmd::Filters : Cmd::ImageScreen);
            case KEY_U: return run(Cmd::PaletteScreen);
            case KEY_LBRACKET: return run(Cmd::PatternPrev);
            case KEY_RBRACKET: return run(Cmd::PatternNext);
            case KEY_J: return run(Cmd::Timeline);
            case KEY_ENTER: return run(Cmd::Play);
            case KEY_COMMA: return run(Cmd::PrevFrame);
            case KEY_PERIOD: return run(Cmd::NextFrame);
            case KEY_HOME: return run(Cmd::FirstFrame);
            case KEY_END: return run(Cmd::LastFrame);
            case KEY_PGUP: return run(Cmd::LayerUp);
            case KEY_PGDN: return run(Cmd::LayerDown);
            case KEY_F3: return run(Cmd::Onion);
            default: break;
        }
    }
    if (!tile && ctrl && key == KEY_F) return run(Cmd::MapFind);
    switch (key) {
        case KEY_ESC:
        case KEY_M: return run(Cmd::MainMenu);
        case KEY_H: return run(Cmd::Help);
        case KEY_N: return run(Cmd::New);
        case KEY_L: return run(Cmd::Open);
        case KEY_S: return run(Cmd::Save);
        case KEY_A: return run(Cmd::SaveSelection);
        case KEY_P:
            if (tile) run(Cmd::ColorPicker);
            return;
        case KEY_Z: return run(Cmd::Undo);
        case KEY_Y: return run(Cmd::Redo);
        case KEY_X: return run(Cmd::SwapColors);
        case KEY_R:
            if (tile) m.randomizeTile();
            return;
        case KEY_DELETE:
            if (tile) m.clearTile();
            return;
        case KEY_TAB: return run(Cmd::SwitchMode);
        case KEY_V: return run(Cmd::PasteHere);
        case KEY_G: return run(Cmd::Grid);
        case KEY_C: return run(Cmd::Checker);
        case KEY_B:
            if (shift) return run(tile ? Cmd::MapContext : Cmd::MapTiles);
            return run(ctrl ? Cmd::NextMapPlace : Cmd::Seamless);
        case KEY_W: return run(Cmd::Wrap);
        case KEY_F: return run(Cmd::SymX);
        case KEY_D: return run(Cmd::SymY);
        case KEY_E: return run(Cmd::BlendMode);
        case KEY_T:
            if (tile) run(Cmd::Tileset);
            return;
        case KEY_I:
            if (tile) run(Cmd::Indexed);
            return;
        case KEY_PLUS:
        case KEY_NUMPLUS: return run(Cmd::ZoomIn);
        case KEY_MINUS:
        case KEY_NUMMINUS: return run(Cmd::ZoomOut);
        case KEY_LEFT: return run(Cmd::PanLeft);
        case KEY_RIGHT: return run(Cmd::PanRight);
        case KEY_UP: return run(Cmd::PanUp);
        case KEY_DOWN: return run(Cmd::PanDown);
        // У 1.6 пробел вписывал картинку; рука — в интерфейсе 1.7 (UX-11).
        case KEY_SPACE: return run(Cmd::ZoomFit);
        default:
            if (key >= KEY_1 && key <= KEY_9) run(Cmd::Tool, key - KEY_1);
            return;
    }
}

void Editor::runCommand(Cmd c, int arg) {
    const bool tile = m.docMode == DOC_TILE;
    const int nf = m.doc.frameCount();
    const int nl = m.doc.layerCount();
    const int step = std::max(1, m.pixelSize * 4);
    auto pan = [&](int dx, int dy) {
        m.viewX += dx;
        m.viewY += dy;
        m.clampView();
        m.needRepaint = true;
    };
    auto toCel = [&](int layer, int frame) {
        stopPlayback();
        m.setActiveCel(layer, frame);
    };
    auto flip = [&](bool& v) {
        v = !v;
        m.needRepaint = true;
    };
    switch (c) {
        case Cmd::New: commandNew(); break;
        case Cmd::Open: commandOpen(); break;
        case Cmd::Save: commandSave(); break;
        case Cmd::SaveAs: commandSaveAs(); break;
        case Cmd::SaveSelection: commandSaveSelection(); break;
        case Cmd::Undo: m.doUndo(); break;
        case Cmd::Redo: m.doRedo(); break;
        case Cmd::PasteHere:
            if (tile) {
                pasteBufferHere();
            } else if (m.bufferFilled()) {
                m.pushUndo();
                m.pasteBufferAt(m.screenToTileX(mouseX()), m.screenToTileY(mouseY()));
                m.needRepaint = true;
            }
            break;
        case Cmd::EraseSelection:
            // На векторном слое — последнюю тронутую фигуру.
            if (m.activeKind() == LK_VECTOR) {
                if (vecSel_ >= 0 && m.deleteVectorShape(vecSel_)) vecSel_ = -1;
            } else if (m.hasSelection) {
                m.eraseSelection();
            }
            break;
        case Cmd::SelectAll: m.selectAll(); break;
        case Cmd::Deselect: m.deselect(); break;
        case Cmd::Reselect: m.reselect(); break;
        case Cmd::InvertSelection: m.invertSelection(); break;
        case Cmd::Copy:
            if (m.copySelection()) exportBufferToClipboard();
            break;
        case Cmd::Cut:
            if (m.cutSelection()) exportBufferToClipboard();
            break;
        case Cmd::PasteClipboard:
            // Картинка, скопированная в другой программе, вставляется так же.
            importClipboardImage();
            pasteBufferHere();
            break;
        case Cmd::BrushFromSelection:
            // Кисть из выделения (этап 9): сразу карандашом этого вида.
            if (m.brushFromSelection()) {
                m.setTool(T_PEN);
                m.needRepaint = true;
            } else {
                notify("Кисть из выделения",
                       "Выделите кусок активного слоя — он станет кистью карандаша.");
            }
            break;
        case Cmd::ZoomIn: m.zoomAt(1, m.W / 2, m.H / 2); break;
        case Cmd::ZoomOut: m.zoomAt(-1, m.W / 2, m.H / 2); break;
        case Cmd::ZoomFit:
            m.fitScale();
            m.needRepaint = true;
            break;
        case Cmd::Hand: spaceHand(); break;
        case Cmd::PanLeft: pan(step, 0); break;
        case Cmd::PanRight: pan(-step, 0); break;
        case Cmd::PanUp: pan(0, step); break;
        case Cmd::PanDown: pan(0, -step); break;
        case Cmd::Grid:
            flip(m.gridOn);
            cfg.showGrid = m.gridOn;
            saveConfig();
            break;
        case Cmd::Checker:
            flip(m.checkerOn);
            cfg.showChecker = m.checkerOn;
            saveConfig();
            break;
        case Cmd::Seamless:
            flip(m.seamlessOn);
            cfg.showSeamless = m.seamlessOn;
            saveConfig();
            break;
        case Cmd::Wrap: flip(m.wrapOn); break;
        case Cmd::Rulers: flip(m.rulersOn); break;
        case Cmd::Guides: flip(m.guidesOn); break;
        case Cmd::GridScreen: showGridScreen(); break;
        case Cmd::MapContext:
            // Картинки набора: у тайла местности — его окружение на
            // настоящей карте (этап 7).
            toggleMapContext();
            m.needRepaint = true;
            break;
        case Cmd::NextMapPlace:
            ++mapCtxIndex_;
            m.needRepaint = true;
            break;
        case Cmd::MapTiles:
            m.mapTiles = !m.mapTiles;
            gameTilesDirty_ = true;
            m.needRepaint = true;
            break;
        case Cmd::Tool:
            // Оригинал прижимал номер к числу инструментов, и на карте 6..9
            // все выбирали выделение; лишние цифры ничего не делают.
            if (arg < m.toolCount()) m.setTool(arg);
            break;
        case Cmd::BrushSmaller:
        case Cmd::BrushBigger:
            // [ и ] — размер кисти, как в Photoshop и Krita (UX-38).
            m.brushSize = std::clamp(m.brushSize + (c == Cmd::BrushBigger ? 1 : -1), 1, 64);
            m.needRepaint = true;
            break;
        case Cmd::PatternPrev:
        case Cmd::PatternNext: {
            const int n = img::ditherPatternCount();
            m.ditherPattern = (m.ditherPattern + (c == Cmd::PatternNext ? 1 : n - 1)) % n;
            m.needRepaint = true;
            break;
        }
        case Cmd::PixelPerfect: flip(m.pixelPerfect); break;
        case Cmd::BlendMode: flip(m.brushReplace); break;
        case Cmd::SymX: flip(m.symX); break;
        case Cmd::SymY: flip(m.symY); break;
        case Cmd::ToolOptions: showToolOptions(); break;
        case Cmd::SwapColors: m.swapColors(); break;
        case Cmd::ColorPicker: showColorScreen(); break;
        case Cmd::PaletteScreen: showPaletteScreen(); break;
        case Cmd::Indexed: toggleIndexed(); break;
        case Cmd::ImageScreen: showImageScreen(); break;
        case Cmd::Filters: showFiltersScreen(); break;
        case Cmd::Tileset: showTilesetScreen(); break;
        case Cmd::TilePicker: showTilePickerScreen(); break;
        case Cmd::Timeline:
            timelineOn_ = !timelineOn_;
            if (!timelineOn_) stopPlayback();
            m.needRepaint = true;
            break;
        case Cmd::Play: togglePlayback(); break;
        case Cmd::PrevFrame:
            toCel(m.activeLayer, (m.activeFrame + nf - 1) % std::max(1, nf));
            break;
        case Cmd::NextFrame: toCel(m.activeLayer, (m.activeFrame + 1) % std::max(1, nf)); break;
        case Cmd::FirstFrame: toCel(m.activeLayer, 0); break;
        case Cmd::LastFrame: toCel(m.activeLayer, nf - 1); break;
        case Cmd::LayerUp: toCel(std::min(nl - 1, m.activeLayer + 1), m.activeFrame); break;
        case Cmd::LayerDown: toCel(std::max(0, m.activeLayer - 1), m.activeFrame); break;
        case Cmd::Onion:
            m.onionSkin = !m.onionSkin;
            m.tileBmpDirty = true;
            m.needRepaint = true;
            break;
        case Cmd::ObjectForm: {
            // Форма объекта под курсором — и без средней кнопки (UX-10).
            const int mx = m.screenToTileX(mouseX());
            const int my = m.screenToTileY(mouseY());
            const int obj = m.inDoc(mx, my) ? m.map.objectAt(mx, my) : -1;
            if (obj >= 0) showObjectForm(obj);
            break;
        }
        case Cmd::MapFind: showMapFindScreen(); break;
        case Cmd::MainMenu: showMainMenu(); break;
        case Cmd::Help: showHelpScreen(); break;
        case Cmd::Cheatsheet: showCheatsheet(); break;
        case Cmd::SwitchMode: m.switchMode(tile ? DOC_MAP : DOC_TILE); break;
    }
}

// ---------------------------------------------------------------------------
// Шпаргалка
// ---------------------------------------------------------------------------

namespace {

/// Слова строки по ширине: перенос между словами, длинное слово — целиком.
template <class Measure>
std::vector<std::string> wrapWords(const std::string& s, int width, Measure measure) {
    std::vector<std::string> out;
    std::string line;
    std::size_t i = 0;
    while (i < s.size()) {
        std::size_t j = s.find(' ', i);
        if (j == std::string::npos) j = s.size();
        const std::string word = s.substr(i, j - i);
        std::string tryLine = line;
        if (!tryLine.empty()) tryLine += ' ';
        tryLine += word;
        if (!line.empty() && measure(tryLine) > width) {
            out.push_back(line);
            line = word;
        } else {
            line = tryLine;
        }
        i = j + 1;
    }
    if (!line.empty()) out.push_back(line);
    return out;
}

}  // namespace

void Editor::showCheatsheet() {
    const auto backdrop = cardBackdrop();
    constexpr int kX0 = 8;
    constexpr int kY0 = 8;
    constexpr int kX1 = WIN_W - 8;
    constexpr int kY1 = WIN_H - 8;
    constexpr int kCols = 3;
    constexpr int kColGap = 18;
    constexpr int kMinText = 100;  // уже — клавиши строкой ниже текста
    const int colW = (kX1 - kX0 - 28 - (kCols - 1) * kColGap) / kCols;
    const int lh = textHeight();
    const int rowLh = lh + 3;
    const bool tile = m.docMode == DOC_TILE;
    const std::span<const Shortcut> all = shortcuts();
    std::string query;
    bool done = false;
    dropInput();
    pf_.input().keys.clear();

    // Строка шпаргалки, уже разложенная по строкам текста.
    struct Row {
        bool header = false;
        std::string keys;
        int keysW = 0;
        std::vector<std::string> text;
        bool keysBelow = false;  // клавиши не помещаются рядом с текстом
        int h = 0;
    };
    // Общие клавиши — строкой внизу: они нужны в любом режиме и иначе не
    // помещаются в столбцы.
    struct Foot {
        std::string text, keys;
    };
    std::vector<Foot> foot;
    auto layout = [&](const std::string& q) {
        const std::string want = netToLower(netTrim(q));
        std::vector<Row> rows;
        foot.clear();
        const char* group = nullptr;
        for (std::size_t i = 0; i < all.size(); ++i) {
            const Shortcut& s = all[i];
            if (s.text == nullptr) continue;
            if (s.where != KeyWhere::Both && (s.where == KeyWhere::Tile) != tile) continue;
            Row r;
            r.keys = shortcutKeys(all, i);
            if (!want.empty() &&
                netToLower(std::string(s.text) + " " + r.keys).find(want) == std::string::npos)
                continue;
            if (std::string_view(s.group) == kWindowGroup) {
                foot.push_back({s.text, r.keys});
                continue;
            }
            if (group == nullptr || std::string_view(group) != s.group) {
                group = s.group;
                Row hdr;
                hdr.header = true;
                hdr.text = {s.group};
                hdr.h = lh + 6;
                rows.push_back(hdr);
            }
            r.keysW = textWidth(r.keys) + 12;
            const int besideW = colW - r.keysW - 10;
            r.keysBelow = besideW < kMinText;
            r.text = wrapWords(s.text, r.keysBelow ? colW : besideW,
                               [&](const std::string& t) { return textWidth(t); });
            r.h = static_cast<int>(r.text.size() + (r.keysBelow ? 1 : 0)) * rowLh + 3;
            rows.push_back(r);
        }
        return rows;
    };
    std::vector<Row> rows = layout(query);
    std::string laidFor = query;

    while (!done && !quitNow_) {
        beginFrameInput();
        dialogBack(backdrop, kX0, kY0, kX1, kY1);
        textOut(kX0 + 14, kY0 + 15, "Клавиши", cText);
        textOut(kX0 + 14 + textWidth("Клавиши") + 10, kY0 + 15,
                tile ? "режим тайла" : "режим карты", cTextDim);
        // Поиск: и по делу («слой»), и по клавише («ctrl+d»).
        const int sx = kX1 - 14 - 110 - 8 - 230;
        uiTextLine(sx, kY0 + 8, 230, 32, query);
        const bool searching = textEditId_ == reinterpret_cast<std::uintptr_t>(&query);
        if (query.empty() && !searching) textOut(sx + 10, kY0 + 16, "Найти клавишу…", cTextDim);
        if (uiPrimary(kX1 - 14 - 110, kY0 + 8, 110, 32, "Закрыть", true)) done = true;
        // Отбор — по мере набора, не дожидаясь Enter.
        const std::string live = searching ? u32ToUtf8(textEdit_.text) : query;
        if (laidFor != live) {
            rows = layout(live);
            laidFor = live;
        }

        // Столбцы: заголовок раздела не остаётся один внизу столбца.
        const int top = kY0 + 48;
        const int bottom = kY1 - 40;
        int col = 0;
        int y = top;
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const Row& r = rows[i];
            const int need = r.h + (r.header && i + 1 < rows.size() ? rows[i + 1].h : 0);
            if (y + need > bottom && y > top) {
                ++col;
                y = top;
            }
            if (col >= kCols) break;
            const int x = kX0 + 14 + col * (colW + kColGap);
            if (r.header) {
                textOut(x, y + 5, r.text[0], cAccent);
                y += r.h;
                continue;
            }
            int ty = y;
            for (const std::string& t : r.text) {
                textOut(x, ty, t, cText);
                ty += rowLh;
            }
            const int ky = r.keysBelow ? ty : y;
            const int kx = x + colW - r.keysW;
            fb_.fillRoundRect(kx, ky - 1, kx + r.keysW, ky + lh + 2, 8, 8, cFace);
            fb_.strokeRoundRect(kx, ky - 1, kx + r.keysW, ky + lh + 2, 8, 8, cOutline, 1);
            textOut(kx + 6, ky, r.keys, cText);
            y += r.h;
        }
        if (rows.empty() && foot.empty()) textOut(kX0 + 14, top + 6, "Ничего не нашлось", cTextDim);
        int fx = kX0 + 14;
        const int fy = kY1 - 28;
        for (const Foot& f : foot) {
            textOut(fx, fy, f.text, cTextDim);
            fx += textWidth(f.text) + 8;
            const int kw = textWidth(f.keys) + 12;
            fb_.fillRoundRect(fx, fy - 1, fx + kw, fy + lh + 2, 8, 8, cFace);
            fb_.strokeRoundRect(fx, fy - 1, fx + kw, fy + lh + 2, 8, 8, cOutline, 1);
            textOut(fx + 6, fy, f.keys, cText);
            fx += kw + 22;
        }

        redraw();
        const int key = takeKey();
        if (key == KEY_ESC || key == KEY_F1 || key == KEY_ENTER) done = true;
        if (key == KEY_SLASH && lastKey_.shift) done = true;
        if (key == KEY_H) {
            endCard(backdrop);
            showHelpScreen();
            return;
        }
        idle();
    }
    waitMouseRelease();
    endCard(backdrop);
    m.needRepaint = true;
}

}  // namespace tjtm
