// Строка меню и строка состояния (план «Интерфейс», C2, UX-05, UX-08,
// UX-40). У 1.6 команды жили в меню во всё окно и на экранах со стенами
// кнопок, а что делают кнопки мыши у инструмента, можно было узнать только
// из справки. Теперь над холстом — строка меню с выпадающими списками, под
// ним — строка состояния: подсказка по кнопкам мыши для инструмента и
// сведения о точке под курсором, документе, масштабе, слое и кадре.
//
// Полосы — только в новых темах: «Классика» сохраняет раскладку 1.6, на
// ней стоят эталоны сценариев.

#include <algorithm>
#include <cstdio>

#include "app/editor.h"
#include "core/text.h"
#include "core/theme.h"

namespace tjtm {

using namespace theme;

namespace {

constexpr int kMenuBarH = MENU_BAR_H;
constexpr int kStatusH = STATUS_BAR_H;
constexpr int kMenuPad = 10;  // поле слева и справа от названия меню
constexpr int kItemH = 26;
constexpr int kSepH = 9;

}  // namespace

void Editor::applyBars() const {
    const bool bars = active.modern && !classicUi_;
    gLayout.top = bars ? kMenuBarH : 0;
    gLayout.bottom = bars ? kStatusH : 0;
}

bool Editor::menuShown(int i) const {
    const bool tile = m.docMode == DOC_TILE;
    const char* title = menuTitles()[static_cast<std::size_t>(i)];
    for (const MenuItem& mi : menuItems()) {
        if (std::string_view(mi.menu) != title || mi.text == nullptr) continue;
        if (mi.where == KeyWhere::Both || (mi.where == KeyWhere::Tile) == tile) return true;
    }
    return false;
}

std::vector<Editor::MenuSlot> Editor::menuSlots() const {
    std::vector<MenuSlot> out;
    int x = 4;
    const auto titles = menuTitles();
    for (std::size_t i = 0; i < titles.size(); ++i) {
        if (!menuShown(static_cast<int>(i))) continue;
        const int w = textWidth(titles[i]) + 2 * kMenuPad;
        out.push_back({static_cast<int>(i), x, x + w});
        x += w;
    }
    return out;
}

int Editor::menuSlotAt(int x, int y) const {
    if (gLayout.top == 0 || y < 0 || y >= kMenuBarH || x >= panelX()) return -1;
    for (const MenuSlot& s : menuSlots()) {
        if (x >= s.x0 && x < s.x1) return s.menu;
    }
    return -1;
}

void Editor::drawMenuBar(int open) {
    if (gLayout.top == 0) return;
    fb_.fillRect(0, 0, panelX(), kMenuBarH, cPanel);
    fb_.fillRect(0, kMenuBarH - 1, panelX(), kMenuBarH, cBorder);
    const int hover = menuSlotAt(mouseX(), mouseY());
    for (const MenuSlot& s : menuSlots()) {
        const bool lit = s.menu == open || (open < 0 && s.menu == hover);
        if (lit) {
            fb_.fillRoundRect(s.x0, 3, s.x1, kMenuBarH - 3, active.radius * 2, active.radius * 2,
                              s.menu == open ? active.accentContainer : cHover);
        }
        textCentered(s.x0, 0, s.x1, kMenuBarH - 1, menuTitles()[static_cast<std::size_t>(s.menu)],
                     s.menu == open ? active.onAccentContainer : cText);
    }
}

bool Editor::cmdChecked(Cmd c) const {
    switch (c) {
        case Cmd::Grid: return m.gridOn;
        case Cmd::Checker: return m.checkerOn;
        case Cmd::Seamless: return m.seamlessOn;
        case Cmd::Rulers: return m.rulersOn;
        case Cmd::Guides: return m.guidesOn;
        case Cmd::Timeline: return timelineOn_;
        case Cmd::Onion: return m.onionSkin;
        case Cmd::Indexed: return m.indexedOn;
        case Cmd::Play: return playing_;
        case Cmd::MapContext: return m.mapContext;
        case Cmd::MapTiles: return m.mapTiles;
        default: return false;
    }
}

void Editor::handleMenuBarClick() {
    const int i = menuSlotAt(frameClickX_, frameClickY_);
    if (i >= 0) openMenu(i);
}

void Editor::openMenu(int menu) {
    // Список рисуется поверх кадра главного окна, как всплывающее меню: без
    // притушения и без страницы по середине.
    const gfx::Surface under = fb_;
    const bool tile = m.docMode == DOC_TILE;
    int open = menu;
    int hot = -1;  // пункт под курсором или выбранный стрелками
    Cmd run = Cmd::MainMenu;
    int runArg = 0;
    bool chosen = false;
    bool done = false;
    waitMouseRelease();
    dropInput();
    pf_.input().keys.clear();

    struct Row {
        const MenuItem* item;  // nullptr — разделитель
        int y0, y1;
    };
    auto rowsOf = [&](int mi, int& x0, int& x1) {
        std::vector<Row> rows;
        const char* title = menuTitles()[static_cast<std::size_t>(mi)];
        int textW = 0;
        int keysW = 0;
        for (const MenuItem& it : menuItems()) {
            if (std::string_view(it.menu) != title || it.text == nullptr) continue;
            if (it.where != KeyWhere::Both && (it.where == KeyWhere::Tile) != tile) continue;
            textW = std::max(textW, textWidth(it.text));
            keysW = std::max(keysW, textWidth(commandKeys(it.cmd, it.arg, tile)));
        }
        x0 = 4;
        for (const MenuSlot& s : menuSlots()) {
            if (s.menu == mi) x0 = s.x0;
        }
        x1 = x0 + 30 + textW + 40 + keysW + 14;
        int y = kMenuBarH + 4;
        bool lastSep = true;  // разделитель в начале и двойной не нужен
        for (const MenuItem& it : menuItems()) {
            if (std::string_view(it.menu) != title) continue;
            if (it.text == nullptr) {
                if (!lastSep) {
                    rows.push_back({nullptr, y, y + kSepH});
                    y += kSepH;
                }
                lastSep = true;
                continue;
            }
            if (it.where != KeyWhere::Both && (it.where == KeyWhere::Tile) != tile) continue;
            rows.push_back({&it, y, y + kItemH});
            y += kItemH;
            lastSep = false;
        }
        if (!rows.empty() && rows.back().item == nullptr) rows.pop_back();
        return rows;
    };
    auto stepHot = [&](const std::vector<Row>& rows, int d) {
        const int n = static_cast<int>(rows.size());
        if (n == 0) return;
        int k = hot;
        for (int t = 0; t < n; ++t) {
            k = (k + d + n) % n;
            if (k < 0) k = 0;
            if (rows[static_cast<std::size_t>(k)].item != nullptr) break;
        }
        hot = k;
    };
    auto switchMenu = [&](int d) {
        const std::vector<MenuSlot> slots = menuSlots();
        const int n = static_cast<int>(slots.size());
        for (int k = 0; k < n; ++k) {
            if (slots[static_cast<std::size_t>(k)].menu == open) {
                open = slots[static_cast<std::size_t>((k + d + n) % n)].menu;
                hot = -1;
                return;
            }
        }
    };

    while (!done && !quitNow_) {
        beginFrameInput(false);
        int x0 = 0;
        int x1 = 0;
        const std::vector<Row> rows = rowsOf(open, x0, x1);
        const int y1 = rows.empty() ? kMenuBarH + 8 : rows.back().y1 + 4;
        // Наведение на другое меню строки переключает список, как в
        // настольных программах.
        const int overTitle = menuSlotAt(mouseX(), mouseY());
        if (overTitle >= 0 && overTitle != open) {
            open = overTitle;
            hot = -1;
            continue;
        }
        const bool inList =
            mouseX() >= x0 && mouseX() < x1 && mouseY() >= kMenuBarH && mouseY() < y1;
        if (inList) {
            for (std::size_t k = 0; k < rows.size(); ++k) {
                if (rows[k].item != nullptr && mouseY() >= rows[k].y0 && mouseY() < rows[k].y1)
                    hot = static_cast<int>(k);
            }
        }

        fb_ = under;
        drawMenuBar(open);
        fb_.fillRoundRect(x0 + 2, kMenuBarH + 2, x1 + 2, y1 + 3, 10, 10, active.shadow);
        fb_.fillRoundRect(x0, kMenuBarH, x1, y1, 10, 10, cBorder);
        fb_.fillRoundRect(x0 + 1, kMenuBarH + 1, x1 - 1, y1 - 1, 9, 9, cFace);
        for (std::size_t k = 0; k < rows.size(); ++k) {
            const Row& r = rows[k];
            if (r.item == nullptr) {
                fb_.fillRect(x0 + 10, r.y0 + kSepH / 2, x1 - 10, r.y0 + kSepH / 2 + 1, cLine);
                continue;
            }
            if (static_cast<int>(k) == hot)
                fb_.fillRoundRect(x0 + 4, r.y0, x1 - 4, r.y1, active.radius * 2, active.radius * 2,
                                  cHover);
            const int ty = r.y0 + (kItemH - textHeight()) / 2;
            if (cmdChecked(r.item->cmd)) textOut(x0 + 12, ty, "✓", cAccent);
            textOut(x0 + 30, ty, r.item->text, cText);
            const std::string keys = commandKeys(r.item->cmd, r.item->arg, tile);
            if (!keys.empty()) textOut(x1 - 14 - textWidth(keys), ty, keys, cTextDim);
        }
        redraw();

        if (frameClick_) {
            frameClick_ = false;
            const int k = hot;
            if (inList && k >= 0 && rows[static_cast<std::size_t>(k)].item != nullptr &&
                frameClickButton_ == MB_LEFT) {
                run = rows[static_cast<std::size_t>(k)].item->cmd;
                runArg = rows[static_cast<std::size_t>(k)].item->arg;
                chosen = true;
            }
            // Щелчок мимо списка закрывает его и больше ничего не делает.
            done = true;
        }
        switch (takeKey()) {
            case KEY_ESC: done = true; break;
            case KEY_DOWN: stepHot(rows, 1); break;
            case KEY_UP: stepHot(rows, -1); break;
            case KEY_LEFT: switchMenu(-1); break;
            case KEY_RIGHT: switchMenu(1); break;
            case KEY_ENTER:
            case KEY_SPACE:
                if (hot >= 0 && rows[static_cast<std::size_t>(hot)].item != nullptr) {
                    run = rows[static_cast<std::size_t>(hot)].item->cmd;
                    runArg = rows[static_cast<std::size_t>(hot)].item->arg;
                    chosen = true;
                    done = true;
                }
                break;
            default: break;
        }
        idle();
    }
    waitMouseRelease();
    fb_ = under;
    dropInput();
    m.needRepaint = true;
    if (chosen) runCommand(run, runArg);
}

std::string Editor::statusInfo() {
    std::string s;
    const int tx = m.screenToTileX(mouseX());
    const int ty = m.screenToTileY(mouseY());
    if (overCanvas(mouseX(), mouseY()) && m.inDoc(tx, ty)) {
        s = std::to_string(tx - m.originX) + " : " + std::to_string(ty - m.originY);
        if (m.docMode == DOC_MAP) {
            s += "   " + mapTileName(m.map.at(tx, ty));
        } else {
            const Color c = m.tile.at(tx, ty);
            char hex[24];
            std::snprintf(hex, sizeof hex, "   #%02X%02X%02X", c.r, c.g, c.b);
            s += hex;
            if (c.a < 255) s += "  α" + std::to_string(c.a);
        }
        s += "   ·   ";
    }
    s += std::to_string(m.docW()) + "×" + std::to_string(m.docH()) + "   ·   " +
         std::to_string(m.zoomPercent()) + " %";
    // Какой слой и кадр правишь — видно и без ленты (UX-40).
    if (m.docMode == DOC_TILE && m.multiCel()) {
        s += "   ·   слой " + std::to_string(m.activeLayer + 1) + "/" +
             std::to_string(m.doc.layerCount()) + ", кадр " + std::to_string(m.activeFrame + 1) +
             "/" + std::to_string(m.doc.frameCount());
    }
    return s;
}

void Editor::drawStatusBar() {
    if (gLayout.bottom == 0) return;
    const int y0 = gLayout.h - gLayout.bottom;
    fb_.fillRect(0, y0, panelX(), gLayout.h, cPanel);
    fb_.fillRect(0, y0, panelX(), y0 + 1, cBorder);
    const int ty = y0 + (gLayout.bottom - textHeight()) / 2;
    const std::string info = statusInfo();
    const int iw = textWidth(info);
    textOut(panelX() - 10 - iw, ty, info, cText);
    // Что делают кнопки мыши у инструмента (UX-08): строки подсказки
    // инструмента после названия — одной строкой.
    const int tool = m.docMode == DOC_TILE ? m.toolTile : m.toolMap;
    const std::vector<std::string> lines = splitLines(toolTip(tool));
    std::string hint;
    for (std::size_t i = 1; i < lines.size(); ++i) {
        if (!hint.empty()) hint += "  ·  ";
        hint += lines[i];
    }
    const int room = panelX() - 10 - iw - 24;
    if (room > 40) textOut(10, ty, fitTextEnd(hint, room), cTextDim);
}

}  // namespace tjtm
