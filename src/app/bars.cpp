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
#include <ranges>

#include "app/editor.h"
#include "core/i18n.h"
#include "core/imageops.h"
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
    gLayout.top = bars ? kMenuBarH + OPTIONS_BAR_H : 0;
    gLayout.bottom = bars ? kStatusH : 0;
}

bool Editor::menuShown(int i) const {
    const bool tile = m.docMode == DOC_TILE;
    const char* title = menuTitles()[static_cast<std::size_t>(i)];
    for (const MenuItem& mi : menuItems()) {
        if (std::string_view(mi.menu) != title || mi.text == nullptr) continue;
        if (mi.full && baseUi()) continue;
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
        case Cmd::Dock: return cfg.dock;
        case Cmd::UiMode: return baseUi();
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
            if (it.full && baseUi()) continue;
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
            if (it.full && baseUi()) continue;
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
            if (cmdChecked(r.item->cmd)) drawCheck(x0 + 11, r.y0 + kItemH / 2, cAccent);
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

void Editor::drawCheck(int x, int y, Color c) {
    // Две черты в две точки толщиной: короткая вниз-вправо, длинная вверх.
    for (int t = 0; t < 2; ++t) {
        fb_.line(x, y + t, x + 3, y + 3 + t, c);
        fb_.line(x + 3, y + 3 + t, x + 10, y - 4 + t, c);
    }
}

// ---------------------------------------------------------------------------
// Панель параметров инструмента
// ---------------------------------------------------------------------------

std::vector<Editor::BarItem> Editor::optionsItems() {
    std::vector<BarItem> left;
    std::vector<BarItem> right;
    const bool tile = m.docMode == DOC_TILE;
    const int tool = tile ? m.toolTile : m.toolMap;
    auto label = [&](const std::string& t) {
        BarItem b;
        b.text = t;
        b.button = false;
        left.push_back(b);
    };
    auto button = [&](const std::string& t, const std::string& tip, bool on,
                      std::function<void()> act) {
        BarItem b;
        b.text = t;
        b.tip = tip;
        b.on = on;
        b.act = std::move(act);
        left.push_back(b);
    };
    // Размер и форма пятна — у карандаша, затенения, ластика и узора (B6).
    auto sizeGroup = [&]() {
        label("Размер");
        button("−", "Кисть меньше · [", false,
               [this] { m.brushSize = std::max(1, m.brushSize - 1); });
        label(std::to_string(m.brushSize));
        button("+", "Кисть больше · ]", false,
               [this] { m.brushSize = std::min(64, m.brushSize + 1); });
        button("Круг", "Пятно кистью-кругом", m.brushRound, [this] { m.brushRound = true; });
        button("Квадрат", "Пятно кистью-квадратом", !m.brushRound,
               [this] { m.brushRound = false; });
    };
    // Название инструмента — первая строка его подсказки, без клавиши.
    std::string name = splitLines(i18n::tr(toolTip(tool))).front();
    if (const std::size_t dot = name.find(" · "); dot != std::string::npos) name.resize(dot);
    label(name);
    if (tile) {
        switch (tool) {
            case T_PEN: {
                // Аэрограф, кисть из выделения и затенение — полного режима.
                if (!baseUi()) {
                    static constexpr const char* kPen[PEN_MODES] = {"точки", "аэрограф", "кисть",
                                                                    "затенение"};
                    button(std::string("Вид: ") + kPen[std::clamp(m.penMode, 0, PEN_MODES - 1)],
                           "Вид карандаша и его параметры", false, [this] { showBrushOptions(); });
                }
                if (m.penMode == PEN_PENCIL || m.penMode == PEN_SHADE) sizeGroup();
                break;
            }
            case T_ERASER: sizeGroup(); break;
            case T_DITHER: {
                const int n = img::ditherPatternCount();
                button("‹", "Узор назад · Shift+[", false,
                       [this, n] { m.ditherPattern = (m.ditherPattern + n - 1) % n; });
                label(img::ditherPatternName(m.ditherPattern));
                button("›", "Узор вперёд · Shift+]", false,
                       [this, n] { m.ditherPattern = (m.ditherPattern + 1) % n; });
                sizeGroup();
                break;
            }
            case T_FILL:
                button("Параметры заливки…", "Допуск, градиент узором", false,
                       [this] { showFillOptions(); });
                break;
            case T_LINE:
            case T_RECT:
            case T_ELLIPSE:
                button("Параметры фигур…", "Вид линии, толщина, сглаживание", false,
                       [this] { showShapeOptions(); });
                label("Ctrl — залитая · Shift — ровная");
                break;
            case T_SELECT: {
                static constexpr const char* kSel[SS_COUNT] = {"Рамка",   "Овал",     "Лассо",
                                                               "Палочка", "По цвету", "Вершины"};
                // Базовый режим выделяет рамкой; виды — в полном.
                for (int s = 0; s < (baseUi() ? 0 : SS_COUNT); ++s)
                    button(kSel[s], "Вид выделения", m.selShape == s,
                           [this, s] { m.selShape = s; });
                break;
            }
            default: break;
        }
    } else if (tool == M_PAINT || tool == M_FILL || tool == M_RECT) {
        button("Кисть карты…", "Одна местность или смесь по весам", false,
               [this] { showMapBrushOptions(); });
    } else if (tool == M_OBJ) {
        label("Вид: " + mapKindName(m.mapObjKind));
    }

    // Режимы — справа, с видимым состоянием (UX-18): у 1.6 заворот не был
    // показан нигде, оси — полупрозрачной линией.
    struct Mode {
        const char* full;
        const char* abbr;  // коротко, когда клавиши нет или она длинная
        const char* tip;
        bool on;
        Cmd cmd;
    };
    std::vector<Mode> modes;
    modes.push_back({"Сетка", "Сет.", "Сетка между точками", m.gridOn, Cmd::Grid});
    if (baseUi()) {
        // В базовом режиме из переключателей — только сетка.
    } else if (tile) {
        modes.push_back({"Бесшовно", "Бш.", "Бесшовный предпросмотр", m.seamlessOn, Cmd::Seamless});
        modes.push_back(
            {"Заворот", "Зав.", "Мазок через край — с другой стороны", m.wrapOn, Cmd::Wrap});
        modes.push_back({"Ось ↕", "↕", "Вертикальная ось симметрии", m.symX, Cmd::SymX});
        modes.push_back({"Ось ↔", "↔", "Горизонтальная ось симметрии", m.symY, Cmd::SymY});
        modes.push_back({"ПП", "ПП", "Пиксель-перфект: без углов «Г» в мазке", m.pixelPerfect,
                         Cmd::PixelPerfect});
        modes.push_back(
            {"Смесь", "См.", "Смешивание вместо замены цвета", !m.brushReplace, Cmd::BlendMode});
        modes.push_back({"Индекс", "Инд.", "Индексная палитра", m.indexedOn, Cmd::Indexed});
    } else {
        modes.push_back(
            {"Картинками", "Карт.", "Карта картинками набора", m.mapTiles, Cmd::MapTiles});
    }

    // Раскладка: слева направо и справа налево; не помещается — режимы
    // буквой клавиши, потом без названия инструмента.
    constexpr int kGap = 4;
    auto widthOf = [&](const BarItem& b) { return textWidth(b.text) + (b.button ? 18 : 8); };
    auto leftWidth = [&](std::size_t from) {
        int w = 8;
        for (std::size_t i = from; i < left.size(); ++i)
            w += widthOf(left[i]) + kGap;
        return w;
    };
    auto buildRight = [&](bool shortLabels) {
        right.clear();
        int w = 8;
        for (const Mode& md : modes) {
            BarItem b;
            // Клавиша — из действующей таблицы (D3): её могли переназначить.
            const std::string keys = commandKeys(md.cmd, 0, tile);
            std::string key = keys.starts_with("Shift+") ? keys.substr(6) : keys;
            if (key.empty() || key.find('+') != std::string::npos || key.size() > 3) key = md.abbr;
            b.text = shortLabels ? key : md.full;
            b.tip = std::string(md.tip) + (keys.empty() ? "" : " · " + keys);
            b.on = md.on;
            b.act = [this, c = md.cmd] { runCommand(c); };
            w += widthOf(b) + kGap;
            right.push_back(b);
        }
        return w;
    };
    const int room = panelX();
    std::size_t from = 0;
    int rw = buildRight(false);
    if (leftWidth(0) + rw > room) rw = buildRight(true);
    if (leftWidth(0) + rw > room) from = 1;
    // И так тесно — убрать режимы слева направо: они есть и в меню «Вид»,
    // и на клавишах, а параметры инструмента — только здесь.
    while (!right.empty() && leftWidth(from) + rw > room) {
        rw -= widthOf(right.front()) + kGap;
        right.erase(right.begin());
    }
    std::vector<BarItem> out;
    int x = 8;
    for (std::size_t i = from; i < left.size(); ++i) {
        BarItem b = left[i];
        b.x0 = x;
        b.x1 = x + widthOf(b);
        x = b.x1 + kGap;
        out.push_back(b);
    }
    x = room - 8;
    for (BarItem b : std::views::reverse(right)) {
        b.x1 = x;
        b.x0 = x - widthOf(b);
        x = b.x0 - kGap;
        out.push_back(b);
    }
    return out;
}

void Editor::drawOptionsBar() {
    if (gLayout.top == 0) return;
    const int y0 = kMenuBarH;
    const int y1 = y0 + OPTIONS_BAR_H;
    fb_.fillRect(0, y0, panelX(), y1, cPanel);
    fb_.fillRect(0, y1 - 1, panelX(), y1, cBorder);
    const std::vector<BarItem> items = optionsItems();
    for (std::size_t i = 0; i < items.size(); ++i) {
        const BarItem& b = items[i];
        if (!b.button) {
            textCentered(b.x0, y0, b.x1, y1 - 1, b.text, cTextDim);
            continue;
        }
        const bool over = static_cast<int>(i) == optionsHover_;
        drawButtonModern(b.x0, y0 + 4, b.x1 - b.x0, OPTIONS_BAR_H - 9, b.text, true, b.on, over,
                         false, Btn::Text);
    }
}

void Editor::handleOptionsBarClick() {
    if (frameClickButton_ != MB_LEFT) return;
    for (const BarItem& b : optionsItems()) {
        if (b.button && b.act && frameClickX_ >= b.x0 && frameClickX_ < b.x1) {
            b.act();
            m.needRepaint = true;
            return;
        }
    }
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
    // Индексный режим — словом, а не только цветом рамки палитры (D7,
    // UX-51).
    if (m.docMode == DOC_TILE && m.indexedOn) s += "   ·   индексная палитра";
    // Перо: нажим виден, пока его ведут (D6).
    if (pf_.input().pen)
        s += "   ·   нажим " + std::to_string(pround(pf_.input().pressure * 100.0F)) + " %";
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
    const std::vector<std::string> lines = splitLines(i18n::tr(toolTip(tool)));
    std::string hint;
    for (std::size_t i = 1; i < lines.size(); ++i) {
        if (!hint.empty()) hint += "  ·  ";
        hint += lines[i];
    }
    const int room = panelX() - 10 - iw - 24;
    if (room > 40) textOut(10, ty, fitTextEnd(hint, room), cTextDim);
}

}  // namespace tjtm
