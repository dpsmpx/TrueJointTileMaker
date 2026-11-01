// План «Интерфейс» (docs/roadmap.md): то, что из него живёт в модели.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <vector>

#include "core/config.h"
#include "core/i18n.h"
#include "core/mapdoc.h"
#include "core/model.h"
#include "core/png.h"
#include "core/scheme.h"
#include "core/shortcuts.h"
#include "core/theme.h"

namespace tjtm {
namespace {

Model tileModel(int w = 16, int h = 16) {
    Model m;
    m.setDefaultPalette();
    m.newTileOfSize(w, h);
    m.map.reset(20, 12);
    return m;
}

void selectRect(Model& m, int x, int y, int w, int h) {
    m.selX = x;
    m.selY = y;
    m.selW = w;
    m.selH = h;
    m.hasSelection = true;
}

// A2: Tab больше не снимает выделение — оно остаётся за документом.
TEST(UiPlan, SelectionStaysWithItsDocument) {
    Model m = tileModel();
    m.keepSelectionPerDoc = true;
    selectRect(m, 2, 3, 4, 5);
    m.switchMode(DOC_MAP);
    EXPECT_FALSE(m.hasSelection) << "у карты своего выделения ещё нет";
    selectRect(m, 1, 1, 6, 2);
    m.switchMode(DOC_TILE);
    ASSERT_TRUE(m.hasSelection);
    EXPECT_EQ(m.selX, 2);
    EXPECT_EQ(m.selH, 5);
    m.switchMode(DOC_MAP);
    ASSERT_TRUE(m.hasSelection);
    EXPECT_EQ(m.selW, 6);
}

TEST(UiPlan, OriginalDropsSelectionOnModeSwitch) {
    Model m = tileModel();
    selectRect(m, 2, 3, 4, 5);
    m.switchMode(DOC_MAP);
    m.switchMode(DOC_TILE);
    EXPECT_FALSE(m.hasSelection);
}

TEST(UiPlan, ReplacedDocumentForgetsItsSelection) {
    Model m = tileModel();
    m.keepSelectionPerDoc = true;
    selectRect(m, 2, 3, 4, 5);
    m.switchMode(DOC_MAP);
    // Тайл с карты открыт заново: прежнее выделение к нему не относится.
    m.setTile(Grid<Color>(16, 16, argb(255, 1, 2, 3)), "other.png");
    m.switchMode(DOC_MAP);
    m.switchMode(DOC_TILE);
    EXPECT_FALSE(m.hasSelection);
}

TEST(UiPlan, SavedSelectionMustFitTheDocument) {
    Model m = tileModel();
    m.keepSelectionPerDoc = true;
    m.switchMode(DOC_MAP);
    selectRect(m, 10, 5, 8, 6);
    m.switchMode(DOC_TILE);
    // Карта стала меньше, пока её не было на экране.
    m.map.reset(12, 8);
    m.switchMode(DOC_MAP);
    EXPECT_FALSE(m.hasSelection);
}

// A1: правая кнопка по ячейке берёт второй цвет с точной привязкой.
TEST(UiPlan, SecondColorFromPaletteCell) {
    Model m = tileModel();
    m.palette[3] = argb(255, 10, 20, 30);
    m.palette[5] = argb(255, 10, 20, 30);
    m.pickSecondFromPalette(5);
    EXPECT_EQ(m.secondColor.argb(), argb(255, 10, 20, 30).argb());
    EXPECT_EQ(m.secondIdx, 5) << "цвет стоит дважды — привязка к той ячейке, где щёлкнули";
}

// A1: запись в ячейку — шаг истории и вне индексного режима.
TEST(UiPlan, PaletteCellWriteIsUndone) {
    Model m = tileModel();
    const Color before = m.palette[1];
    m.pushUndo();
    m.palette[1] = argb(255, 0, 0, 0);
    m.doUndo();
    EXPECT_EQ(m.palette[1].argb(), before.argb());
}

// A11: история по реальному объёму — у тайла 512×512 не 6 шагов, а сотни.
TEST(UiPlan, UndoKeepsHundredsOfSteps) {
    Model m = tileModel(512, 512);
    EXPECT_EQ(m.undoDepth(), 6) << "бюджет оригинала";
    m.undoBudget = std::int64_t{256} * 1024 * 1024;
    m.undoBytesPerPx = 6;
    m.undoMinSteps = 100;
    m.undoMaxSteps = 1000;
    EXPECT_GE(m.undoDepth(), 100);
    for (int k = 0; k < 120; ++k) {
        m.pushUndo();
        m.tile.at(k % 512, 0) = argb(255, k, 0, 0);
    }
    int undone = 0;
    while (!m.undoStack.empty()) {
        m.doUndo();
        ++undone;
    }
    EXPECT_EQ(undone, 120);
    EXPECT_EQ(m.tile.at(0, 0).argb(), tileModel(512, 512).tile.at(0, 0).argb());
    m = tileModel(32, 32);
    m.undoBudget = std::int64_t{256} * 1024 * 1024;
    m.undoMaxSteps = 1000;
    EXPECT_EQ(m.undoDepth(), 1000) << "мелкий тайл — до верхнего предела";
}

// B6: пятно кисти — квадрат или круг; по умолчанию одна точка, как у
// оригинала.
TEST(UiPlan, BrushFootprint) {
    Model m = tileModel();
    auto count = [&](int size, bool round) {
        m.brushSize = size;
        m.brushRound = round;
        int n = 0;
        m.forEachBrushCell(5, 5, [&](int, int) { ++n; });
        return n;
    };
    EXPECT_EQ(count(1, true), 1);
    EXPECT_EQ(count(2, true), 4);
    EXPECT_EQ(count(3, true), 5) << "у тройки круг — «плюс»";
    EXPECT_EQ(count(3, false), 9);
    EXPECT_EQ(count(4, true), 12) << "квадрат без углов";
    EXPECT_EQ(count(5, false), 25);
}

TEST(UiPlan, SizedEraserStrokeIsWide) {
    Model m = tileModel(16, 16);
    m.transparentBlank = true;
    for (Color& c : m.tile.d)
        c = argb(255, 10, 20, 30);
    m.setTool(T_ERASER);
    m.brushSize = 3;
    m.brushRound = false;
    EXPECT_TRUE(m.brushSized());
    m.pushUndo();
    m.beginStroke();
    m.emitPixel(4, 8);
    m.shapeLine(4, 8, 11, 8);
    m.endStroke();
    for (int x = 3; x <= 12; ++x) {
        for (int y = 7; y <= 9; ++y)
            EXPECT_EQ(m.tile.at(x, y).a, 0) << x << "," << y;
    }
    EXPECT_EQ(m.tile.at(8, 6).a, 255) << "выше пятна не стёрто";
    EXPECT_EQ(m.tile.at(8, 10).a, 255);
    m.brushSize = 1;
    EXPECT_FALSE(m.brushSized()) << "одна точка — путь оригинала";
}

// B8: недавние файлы — последний первым, без повторов, не больше десяти;
// в файле — только когда есть, чтобы файл по умолчанию был как у оригинала.
TEST(UiPlan, RecentFilesInConfig) {
    Config c;
    EXPECT_EQ(c.serialize().find("RecentFiles"), std::string::npos);
    c.addRecent("/a/one.png");
    c.addRecent("/b/два.map");
    c.addRecent("/a/one.png");
    ASSERT_EQ(c.recentFiles.size(), 2U);
    EXPECT_EQ(c.recentFiles[0], "/a/one.png");
    for (int i = 0; i < 12; ++i)
        c.addRecent("/x/" + std::to_string(i) + ".png");
    EXPECT_EQ(c.recentFiles.size(), Config::kRecentMax);
    EXPECT_EQ(c.recentFiles[0], "/x/11.png");
    Config back;
    std::vector<std::string> lines;
    const std::string text = c.serialize();
    std::size_t a = 0;
    while (a < text.size()) {
        const std::size_t b = text.find('\n', a);
        lines.push_back(text.substr(a, b == std::string::npos ? std::string::npos : b - a));
        if (b == std::string::npos) break;
        a = b + 1;
    }
    back.applyLines(lines);
    EXPECT_EQ(back.recentFiles, c.recentFiles);
}

// A6: контраст по WCAG 2.1 — относительная яркость и отношение (L1 + 0,05) /
// (L2 + 0,05).
double luminance(Color c) {
    auto lin = [](int v) {
        const double s = v / 255.0;
        return s <= 0.04045 ? s / 12.92 : std::pow((s + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * lin(c.r) + 0.7152 * lin(c.g) + 0.0722 * lin(c.b);
}

double contrast(Color a, Color b) {
    const double la = luminance(a);
    const double lb = luminance(b);
    return (std::max(la, lb) + 0.05) / (std::min(la, lb) + 0.05);
}

void expectWcag(const theme::Theme& t) {
    SCOPED_TRACE(t.name);
    EXPECT_GE(contrast(t.text, t.back), 4.5);
    EXPECT_GE(contrast(t.text, t.window), 4.5);
    EXPECT_GE(contrast(t.text, t.panel), 4.5);
    EXPECT_GE(contrast(t.textDim, t.window), 4.5) << "подсказки — мелкий текст";
    EXPECT_GE(contrast(t.textDim, t.face), 4.5);
    EXPECT_GE(contrast(t.outline, t.window), 3.0) << "граница нажимаемого";
    EXPECT_GE(contrast(t.onAccent, t.accent), 4.5) << "главная кнопка";
    EXPECT_GE(contrast(t.accent, t.window), 4.5) << "акцентом пишут «режим тайла» и вкладки";
    EXPECT_GE(contrast(t.onAccentContainer, t.accentContainer), 4.5) << "выбранное";
    EXPECT_GE(contrast(t.inverseOnSurface, t.inverseSurface), 4.5) << "подсказки поверх окна";
    EXPECT_GE(contrast(t.warn, t.window), 4.5);
}

// B1: светлая и тёмная схемы ui-design-system.md держат контраст WCAG.
TEST(UiTheme, ModernThemesMeetWcag) {
    expectWcag(theme::lightTheme());
    expectWcag(theme::darkTheme());
    EXPECT_TRUE(theme::lightTheme().modern);
    EXPECT_TRUE(theme::darkTheme().modern);
    EXPECT_FALSE(theme::classicTheme().modern);
}

TEST(UiTheme, ThemeByName) {
    EXPECT_STREQ(theme::themeByName("классика").name, "классика");
    EXPECT_STREQ(theme::themeByName("тёмная").name, "тёмная");
    EXPECT_STREQ(theme::themeByName("что-то").name, "светлая");
}

TEST(UiTheme, LightThemeMeetsWcag) {
    const theme::Theme t = theme::lightTheme();
    EXPECT_GE(contrast(t.text, t.back), 4.5);
    EXPECT_GE(contrast(t.text, t.window), 4.5);
    EXPECT_GE(contrast(t.textDim, t.window), 4.5) << "подсказки — мелкий текст";
    EXPECT_GE(contrast(t.textDim, t.back), 4.5);
    EXPECT_GE(contrast(t.outline, t.window), 3.0) << "граница нажимаемого";
    EXPECT_GE(contrast(t.outline, t.back), 3.0);
    EXPECT_GE(contrast(t.onAccent, t.accent), 4.5);
    EXPECT_GE(contrast(t.accent, t.window), 4.5) << "акцентом пишут «режим тайла»";
}

TEST(UiTheme, ClassicKeepsTheLookOf16) {
    // На «Классике» сверяются сценарии оригинала: её цвета не меняются.
    const theme::Theme t = theme::classicTheme();
    EXPECT_EQ(t.window.argb(), rgb(240, 242, 246).argb());
    EXPECT_EQ(t.border.argb(), rgb(205, 210, 218).argb());
    EXPECT_EQ(t.outline.argb(), t.border.argb());
    EXPECT_EQ(t.textDim.argb(), rgb(122, 130, 141).argb());
    EXPECT_EQ(t.checkA.argb(), rgb(214, 214, 218).argb());
    EXPECT_EQ(t.radius, 7);
    EXPECT_LT(contrast(t.textDim, t.window), 4.5) << "в 1.6 подсказки были бледнее нормы";
}

// --- B11: сочетания одной таблицей -----------------------------------------

TEST(UiShortcuts, NoConflicts) {
    // Два сочетания на одно нажатие — второе никогда не сработает, а
    // шпаргалка его покажет.
    const auto all = shortcuts();
    for (std::size_t i = 0; i < all.size(); ++i) {
        for (std::size_t j = i + 1; j < all.size(); ++j) {
            const Shortcut& a = all[i];
            const Shortcut& b = all[j];
            const bool overlap =
                a.where == KeyWhere::Both || b.where == KeyWhere::Both || a.where == b.where;
            EXPECT_FALSE(a.key == b.key && a.mods == b.mods && overlap)
                << keyName(a.key, a.mods) << " — дважды";
        }
    }
}

TEST(UiShortcuts, EveryCommandHasKeyAndName) {
    const auto all = shortcuts();
    for (int c = 0; c <= static_cast<int>(Cmd::SwitchMode); ++c) {
        const bool key = std::any_of(all.begin(), all.end(), [&](const Shortcut& s) {
            return static_cast<int>(s.cmd) == c;
        });
        const auto menu = menuItems();
        const bool item = std::any_of(menu.begin(), menu.end(), [&](const MenuItem& mi) {
            return mi.text != nullptr && static_cast<int>(mi.cmd) == c;
        });
        EXPECT_TRUE(key || item) << "команда " << c << " — ни клавиши, ни пункта меню";
    }
    for (const Shortcut& s : all)
        EXPECT_NE(keyName(s.key, s.mods)[0], '#') << "клавиша без имени: " << s.key;
}

TEST(UiShortcuts, HiddenRowsFollowTheirLine) {
    // Сочетание без текста дописывается к строке над ним: та же команда
    // или своя подпись у строки (цифры, стрелки, [ ]).
    const auto all = shortcuts();
    const Shortcut* line = nullptr;
    for (const Shortcut& s : all) {
        if (s.text != nullptr) {
            line = &s;
            EXPECT_STRNE(s.group, "");
            continue;
        }
        ASSERT_NE(line, nullptr);
        EXPECT_TRUE(line->cmd == s.cmd || line->label != nullptr) << keyName(s.key, s.mods);
        EXPECT_EQ(line->where, s.where) << keyName(s.key, s.mods);
    }
}

TEST(UiShortcuts, ShiftFallsBackAndModesSplit) {
    auto cmd = [](int key, int mods, bool tile) {
        const Shortcut* s = findShortcut(key, mods, tile);
        return s == nullptr ? -1 : static_cast<int>(s->cmd);
    };
    auto is = [](Cmd c) { return static_cast<int>(c); };
    // Shift без своего сочетания — как без Shift, как в 1.6.
    EXPECT_EQ(cmd(KEY_G, MOD_SHIFT, true), is(Cmd::Grid));
    EXPECT_EQ(cmd(KEY_A, MOD_CTRL | MOD_SHIFT, true), is(Cmd::SelectAll));
    // Своё сочетание с Shift — важнее.
    EXPECT_EQ(cmd(KEY_S, MOD_CTRL | MOD_SHIFT, true), is(Cmd::SaveAs));
    EXPECT_EQ(cmd(KEY_Z, MOD_CTRL | MOD_SHIFT, false), is(Cmd::Redo));
    EXPECT_EQ(cmd(KEY_SLASH, MOD_SHIFT, true), is(Cmd::Cheatsheet));
    // Ctrl с буквой без сочетания и Alt ничего не делают (UX-16).
    EXPECT_EQ(cmd(KEY_Q, MOD_CTRL, true), -1);
    EXPECT_EQ(cmd(KEY_G, MOD_ALT, true), -1);
    EXPECT_EQ(cmd(KEY_A, MOD_CTRL, false), -1);
    // Одна клавиша — разное в тайле и на карте.
    EXPECT_EQ(cmd(KEY_ENTER, 0, true), is(Cmd::Play));
    EXPECT_EQ(cmd(KEY_ENTER, 0, false), is(Cmd::ObjectForm));
    EXPECT_EQ(cmd(KEY_B, MOD_SHIFT, true), is(Cmd::MapContext));
    EXPECT_EQ(cmd(KEY_B, MOD_SHIFT, false), is(Cmd::MapTiles));
    // Стрелки: без Ctrl — сдвиг выделения (или холст), с Ctrl — холст.
    EXPECT_EQ(cmd(KEY_LEFT, 0, true), is(Cmd::Nudge));
    EXPECT_EQ(cmd(KEY_LEFT, MOD_SHIFT, false), is(Cmd::Nudge));
    EXPECT_EQ(findShortcut(KEY_DOWN, MOD_SHIFT, true)->arg, 7);
    EXPECT_EQ(cmd(KEY_RIGHT, MOD_CTRL, true), is(Cmd::Pan));
    EXPECT_EQ(cmd(KEY_RIGHT, MOD_CTRL | MOD_SHIFT, true), is(Cmd::Pan));
    // Однобуквенные команды файла — только с Ctrl (UX-17).
    EXPECT_EQ(cmd(KEY_N, 0, true), -1);
    EXPECT_EQ(cmd(KEY_S, 0, true), -1);
    EXPECT_EQ(cmd(KEY_R, 0, true), -1);
}

TEST(UiShortcuts, MenuItemsAreKnown) {
    // У каждого меню есть пункты, и пункт стоит в меню из списка.
    const auto titles = menuTitles();
    for (const char* t : titles) {
        const auto items = menuItems();
        EXPECT_TRUE(std::any_of(items.begin(), items.end(), [&](const MenuItem& mi) {
            return std::string_view(mi.menu) == t;
        })) << t;
    }
    for (const MenuItem& mi : menuItems()) {
        EXPECT_TRUE(std::any_of(titles.begin(), titles.end(), [&](const char* t) {
            return std::string_view(mi.menu) == t;
        })) << mi.menu;
    }
    // Клавиша пункта — из таблицы сочетаний.
    EXPECT_EQ(commandKeys(Cmd::Save, 0, true), "Ctrl+S");
    EXPECT_EQ(commandKeys(Cmd::Settings, 0, true), "");
    EXPECT_EQ(commandKeys(Cmd::ObjectForm, 0, false), "Enter");
}

TEST(UiShortcuts, SheetLabels) {
    const auto all = shortcuts();
    auto row = [&](Cmd c) {
        for (std::size_t i = 0; i < all.size(); ++i) {
            if (all[i].cmd == c && all[i].text != nullptr) return shortcutKeys(all, i);
        }
        return std::string();
    };
    EXPECT_EQ(row(Cmd::Undo), "Ctrl+Z, Z");
    EXPECT_EQ(row(Cmd::Redo), "Ctrl+Shift+Z, Ctrl+Y, Y");
    EXPECT_EQ(row(Cmd::Cheatsheet), "F1, ?");
    EXPECT_EQ(row(Cmd::MainMenu), "Esc, M");
    EXPECT_EQ(row(Cmd::Tool), "1 … 9");
    EXPECT_EQ(keyName(KEY_SEMICOLON, MOD_CTRL), "Ctrl+;");
}

// --- B12: масштаб ступенями и меньше 100 % ---------------------------------

TEST(UiZoom, OriginalDoubles) {
    Model m = tileModel();
    m.pixelSize = 8;
    m.zoomAt(1, 100, 100);
    EXPECT_EQ(m.pixelSize, 16);
    m.pixelSize = 1;
    m.zoomAt(-1, 100, 100);
    EXPECT_EQ(m.pixelSize, 1);
    EXPECT_EQ(m.zoomOut, 1);
}

TEST(UiZoom, StepsUpAndDown) {
    Model m = tileModel();
    m.zoomSteps = true;
    m.pixelSize = 8;
    std::vector<int> up;
    for (int i = 0; i < 7; ++i) {
        m.zoomAt(1, 100, 100);
        up.push_back(m.zoomPercent());
    }
    EXPECT_EQ(up, (std::vector<int>{1200, 1600, 2400, 3200, 4800, 6400, 6400}));
    m.pixelSize = 8;
    std::vector<int> down;
    for (int i = 0; i < 8; ++i) {
        m.zoomAt(-1, 100, 100);
        down.push_back(m.zoomPercent());
    }
    EXPECT_EQ(down, (std::vector<int>{600, 400, 300, 200, 100, 50, 25, 25}));
    // Вписанный масштаб между ступенями: шаг — к соседней ступени.
    m.setZoom(5, 1, 100, 100);
    m.zoomAt(1, 100, 100);
    EXPECT_EQ(m.pixelSize, 6);
    m.setZoom(5, 1, 100, 100);
    m.zoomAt(-1, 100, 100);
    EXPECT_EQ(m.pixelSize, 4);
}

TEST(UiZoom, MapStopsAtHundred) {
    Model m = tileModel();
    m.zoomSteps = true;
    m.switchMode(DOC_MAP);
    m.setZoom(1, 1, 100, 100);
    m.zoomAt(-1, 100, 100);
    EXPECT_EQ(m.zoomOut, 1);
    m.setZoom(1, 2, 100, 100);
    EXPECT_EQ(m.zoomOut, 1);
}

TEST(UiZoom, ZoomOutCoordinates) {
    Model m = tileModel(200, 200);
    m.zoomSteps = true;
    m.setZoom(1, 2, 0, 0);
    EXPECT_EQ(m.scr(200), 100);
    // Точка экрана — первая из двух точек документа под ней, и обратно.
    for (int tx : {0, 1, 2, 57, 199}) {
        const int sx = m.tileToScreenX(tx);
        EXPECT_EQ(m.screenToTileX(sx), tx - tx % 2) << tx;
    }
    // Документ меньше холста — по середине.
    EXPECT_EQ(m.tileToScreenX(0), m.X0 + (m.W - m.X0 - 100) / 2);
}

TEST(UiZoom, AnchorStays) {
    Model m = tileModel(300, 300);
    m.zoomSteps = true;
    m.setZoom(8, 1, 0, 0);
    m.viewX = -600;
    m.viewY = -600;
    m.clampView();
    const int ax = 300;
    const int ay = 250;
    const int tx = m.screenToTileX(ax);
    const int ty = m.screenToTileY(ay);
    m.zoomAt(1, ax, ay);
    EXPECT_NEAR(m.screenToTileX(ax), tx, 1);
    EXPECT_NEAR(m.screenToTileY(ay), ty, 1);
}

TEST(UiZoom, FitShrinksBigSheet) {
    // Лист 512×512 в холсте 300×300 (линейки, лента, а с этапа C — окно
    // любого размера).
    Model m = tileModel(512, 512);
    m.W = m.X0 + 300;
    m.H = m.Y0 + 300;
    m.fitScale();
    EXPECT_EQ(m.zoomOut, 1);  // без флага — как у оригинала: 100 %, не влезает
    m.zoomSteps = true;
    m.fitScale();
    EXPECT_EQ(m.zoomOut, 2);
    EXPECT_LE(m.scr(m.docW()), m.W - m.X0);
    EXPECT_LE(m.scr(m.docH()), m.H - m.Y0);
    m.W = m.X0 + 120;
    m.fitScale();
    EXPECT_EQ(m.zoomOut, 4);  // мельче 25 % не бывает
    Model small = tileModel(16, 16);
    small.zoomSteps = true;
    small.fitScale();
    EXPECT_EQ(small.zoomOut, 1);
    EXPECT_GT(small.pixelSize, 1);
}

// --- B13: стрелки двигают выделенное ---------------------------------------

TEST(UiNudge, TileMovesPixelsAndKeepsUnder) {
    Model m = tileModel(8, 8);
    m.tile = Grid<Color>(8, 8, kWhite);
    m.tile.at(2, 2) = kBlack;
    m.selX = 2;
    m.selY = 2;
    m.selW = 1;
    m.selH = 1;
    m.hasSelection = true;
    m.nudgeSelection(1, 0);
    EXPECT_EQ(m.selX, 3);
    EXPECT_EQ(m.tile.at(3, 2), kBlack);
    EXPECT_EQ(m.tile.at(2, 2), kClear);
    // Кусок ездит по картинке, ничего под собой не стирая.
    m.nudgeSelection(1, 0);
    EXPECT_EQ(m.tile.at(4, 2), kBlack);
    EXPECT_EQ(m.tile.at(3, 2), kWhite);
    m.nudgeSelection(0, 3);
    EXPECT_EQ(m.tile.at(4, 5), kBlack);
    EXPECT_EQ(m.tile.at(4, 2), kWhite);
}

TEST(UiNudge, MapMovesFrameOnly) {
    Model m = tileModel();
    m.switchMode(DOC_MAP);
    const auto before = m.map.at(1, 1);
    m.selX = 1;
    m.selY = 1;
    m.selW = 3;
    m.selH = 2;
    m.hasSelection = true;
    m.nudgeSelection(-5, 1);
    EXPECT_EQ(m.selX, 0);  // у края документа рамка встаёт
    EXPECT_EQ(m.selY, 2);
    EXPECT_EQ(m.map.at(1, 1), before);
}

TEST(UiColors, ResetToBlackAndWhite) {
    Model m = tileModel();
    m.firstColor = argb(255, 10, 20, 30);
    m.secondColor = argb(128, 1, 2, 3);
    m.resetColors();
    EXPECT_EQ(m.firstColor, kBlack);
    EXPECT_EQ(m.secondColor, kWhite);
}

// --- C1: окно с изменяемым размером -----------------------------------------

TEST(UiWindow, ConfigKeys) {
    // Не запоминалось — в файле ни строки: файл по умолчанию как у
    // оригинала.
    Config c;
    EXPECT_EQ(c.serialize().find("WindowSize"), std::string::npos);
    EXPECT_EQ(c.serialize().find("UiScale"), std::string::npos);
    Config back;
    back.applyLines({"WindowSize = 1280x800", "WindowMaximized = да", "UiScale = 150"});
    EXPECT_EQ(back.windowW, 1280);
    EXPECT_EQ(back.windowH, 800);
    EXPECT_TRUE(back.windowMaximized);
    EXPECT_EQ(back.uiScale, 150);
    const std::string text = back.serialize();
    EXPECT_NE(text.find("WindowSize = 1280x800"), std::string::npos);
    EXPECT_NE(text.find("UiScale = 150"), std::string::npos);
    // Масштаб не из ступеней — по экрану.
    Config odd;
    odd.applyLines({"UiScale = 133"});
    EXPECT_EQ(odd.uiScale, 0);
}

TEST(UiWindow, PanelFollowsWindow) {
    const WindowLayout saved = gLayout;
    gLayout = {1100, 700};
    EXPECT_EQ(panelX(), 1100 - PANEL_W);
    Model m = tileModel();
    int idx = -1;
    // Инструменты — у правого края окна, а не на месте 1.6.
    EXPECT_TRUE(m.toolHit(panelX() + 10, TOOLS_Y0 + 10, idx));
    EXPECT_FALSE(m.toolHit(CANVAS_W + 10, TOOLS_Y0 + 10, idx));
    gLayout = saved;
}

// --- C3: панель истории ------------------------------------------------------

TEST(UiHistory, LabelsFollowUndoAndRedo) {
    Model m = tileModel(8, 8);
    m.actionName = "Карандаш";
    m.pushUndo();
    m.tile.at(0, 0) = kBlack;
    m.actionName = "Заливка";
    m.pushUndo();
    m.tile.at(1, 1) = kBlack;
    int done = 0;
    EXPECT_EQ(m.historyLabels(done), (std::vector<std::string>{"Карандаш", "Заливка"}));
    EXPECT_EQ(done, 2);
    m.doUndo();
    EXPECT_EQ(m.historyLabels(done), (std::vector<std::string>{"Карандаш", "Заливка"}));
    EXPECT_EQ(done, 1);
    // Переход к началу и обратно — отменой и повтором.
    m.historyGoTo(0);
    EXPECT_EQ(m.tile.at(0, 0), Model().blankColor());
    m.historyGoTo(2);
    EXPECT_EQ(m.tile.at(1, 1), kBlack);
    EXPECT_FALSE(m.historyLabels(done).empty());
    EXPECT_EQ(done, 2);
}

TEST(UiHistory, MapLabels) {
    Model m = tileModel();
    m.switchMode(DOC_MAP);
    m.actionName = "Кисть";
    m.pushUndo();
    m.actionName = "Объект";
    m.pushUndo();
    m.doUndo();
    int done = 0;
    EXPECT_EQ(m.historyLabels(done), (std::vector<std::string>{"Кисть", "Объект"}));
    EXPECT_EQ(done, 1);
    m.doRedo();
    EXPECT_FALSE(m.historyLabels(done).empty());
    EXPECT_EQ(done, 2);
}

// --- C4: базовый и полный режимы -------------------------------------------

TEST(UiModes, ConfigAndMenu) {
    Config c;
    EXPECT_EQ(c.uiMode, "полный");
    EXPECT_EQ(c.serialize().find("UiMode"), std::string::npos);
    c.applyLines({"UiMode = базовый"});
    EXPECT_EQ(c.uiMode, "базовый");
    EXPECT_NE(c.serialize().find("UiMode = базовый"), std::string::npos);
    // Анимация — только полного режима, файл и отмена — обоих.
    for (const MenuItem& mi : menuItems()) {
        if (mi.text == nullptr) continue;
        if (std::string_view(mi.menu) == "Анимация") {
            EXPECT_TRUE(mi.full) << mi.text;
        }
        if (mi.cmd == Cmd::Save || mi.cmd == Cmd::Undo || mi.cmd == Cmd::UiMode) {
            EXPECT_FALSE(mi.full) << mi.text;
        }
    }
    // Переключатель — Shift+Tab, а Tab остаётся тайлом и картой.
    EXPECT_EQ(findShortcut(KEY_TAB, MOD_SHIFT, true)->cmd, Cmd::UiMode);
    EXPECT_EQ(findShortcut(KEY_TAB, 0, true)->cmd, Cmd::SwitchMode);
}

// --- C10: карта ----------------------------------------------------------------

TEST(UiMap, KindTitlesAndProblemCells) {
    for (int k = 0; k < mapKindCount(); ++k)
        EXPECT_FALSE(mapKindTitle(mapKindName(k)).empty()) << mapKindName(k);
    EXPECT_EQ(mapKindTitle("npc"), "Житель");
    MapDoc d;
    d.reset(4, 3);
    d.at(1, 1) = static_cast<std::uint8_t>(mapCharTile(U'#'));
    d.addObject("sign", 1, 1, "Стена");
    d.addObject("npc", 2, 2, "elder");
    const auto cells = d.problemCells();
    ASSERT_EQ(cells.size(), 1U);
    EXPECT_EQ(cells[0], (std::pair{1, 1}));
}

}  // namespace

// --- D3: правка клавиш и набор «как в Aseprite» ------------------------------

namespace {

/// Таблица клавиш — одна на процесс: тест возвращает стандартную.
struct KeymapGuard {
    KeymapGuard() = default;
    KeymapGuard(const KeymapGuard&) = delete;
    KeymapGuard& operator=(const KeymapGuard&) = delete;
    ~KeymapGuard() { applyKeymap(kKeysStandard, {}); }
};

void expectNoConflicts() {
    const auto all = shortcuts();
    for (std::size_t i = 0; i < all.size(); ++i) {
        for (std::size_t j = i + 1; j < all.size(); ++j) {
            const Shortcut& a = all[i];
            const Shortcut& b = all[j];
            if (a.key == 0) continue;
            const bool overlap =
                a.where == KeyWhere::Both || b.where == KeyWhere::Both || a.where == b.where;
            EXPECT_FALSE(a.key == b.key && a.mods == b.mods && overlap)
                << keyName(a.key, a.mods) << " — дважды";
        }
    }
}

}  // namespace

TEST(UiKeymap, KeyNamesRoundTrip) {
    for (const Shortcut& s : defaultShortcuts()) {
        KeyCombo k;
        ASSERT_TRUE(parseKeyName(keyName(s.key, s.mods), k)) << keyName(s.key, s.mods);
        EXPECT_EQ(k, (KeyCombo{s.key, s.mods})) << keyName(s.key, s.mods);
    }
    // Как пишут руками.
    KeyCombo k;
    ASSERT_TRUE(parseKeyName("ctrl+space", k));
    EXPECT_EQ(k, (KeyCombo{KEY_SPACE, MOD_CTRL}));
    ASSERT_TRUE(parseKeyName("Ctrl+-", k));
    EXPECT_EQ(k, (KeyCombo{KEY_MINUS, MOD_CTRL}));
    ASSERT_TRUE(parseKeyName("shift+u", k));
    EXPECT_EQ(k, (KeyCombo{KEY_U, MOD_SHIFT}));
    EXPECT_FALSE(parseKeyName("Ctrl+Что-то", k));
    EXPECT_EQ(parseKeysText("Ctrl+Y | Ctrl+Shift+Z | мусор").size(), 2u);
    EXPECT_EQ(keysText({}), "нет");
}

TEST(UiKeymap, ActionsHaveIdsAndTitles) {
    std::vector<std::string> ids;
    for (const KeyAction& a : keyActions()) {
        const std::string id = actionId(a);
        EXPECT_FALSE(a.title.empty()) << id;
        EXPECT_EQ(findAction(id), &a) << id;
        EXPECT_EQ(std::count(ids.begin(), ids.end(), id), 0) << id;
        ids.push_back(id);
    }
    EXPECT_EQ(findAction("tool.3")->arg, 3);
    EXPECT_EQ(findAction("SAVE")->cmd, Cmd::Save);
    EXPECT_EQ(findAction("Нет такого"), nullptr);
    // Команды меню без клавиш тоже можно назначить.
    ASSERT_NE(findAction("Settings"), nullptr);
    EXPECT_EQ(findAction("Settings")->group, "Файл");
}

TEST(UiKeymap, AsepritePreset) {
    const KeymapGuard guard;
    applyKeymap(kKeysAseprite, {});
    expectNoConflicts();
    const auto tool = [](int key, int mods) {
        const Shortcut* s = findShortcut(key, mods, true);
        return s != nullptr && s->cmd == Cmd::Tool ? s->arg : -1;
    };
    EXPECT_EQ(tool(KEY_B, 0), 0);  // карандаш
    EXPECT_EQ(tool(KEY_E, 0), 1);  // ластик
    EXPECT_EQ(tool(KEY_G, 0), 2);  // заливка
    EXPECT_EQ(tool(KEY_I, 0), 3);  // пипетка
    EXPECT_EQ(tool(KEY_M, 0), 8);  // выделение
    EXPECT_EQ(findShortcut(KEY_1, 0, true)->cmd, Cmd::Zoom100);
    EXPECT_EQ(findShortcut(KEY_PLUS, 0, true)->cmd, Cmd::BrushBigger);
    EXPECT_EQ(findShortcut(KEY_TAB, 0, true)->cmd, Cmd::Timeline);
    EXPECT_EQ(findShortcut(KEY_TAB, MOD_CTRL, true)->cmd, Cmd::SwitchMode);
    EXPECT_EQ(findShortcut(KEY_SPACE, MOD_CTRL, true)->cmd, Cmd::CommandSearch);
    EXPECT_EQ(findShortcut(KEY_K, MOD_CTRL, true)->cmd, Cmd::Settings);
    // Занятое буквой осталось без клавиши, но строкой — её можно задать.
    EXPECT_TRUE(keysIn(shortcuts(), Cmd::Seamless, 0).empty());
    EXPECT_EQ(commandKeys(Cmd::Seamless, 0, true), "");
    // Подпись строки «1 … 9» снята: клавиши — как есть.
    const auto all = shortcuts();
    for (std::size_t i = 0; i < all.size(); ++i) {
        if (all[i].cmd == Cmd::Tool && all[i].text != nullptr) {
            EXPECT_EQ(shortcutKeys(all, i), "B, E, G, I, L, U, Shift+U, 8, M");
        }
    }
    // Стандартный набор — снова таблица 1.9.
    applyKeymap(kKeysStandard, {});
    ASSERT_EQ(shortcuts().size(), defaultShortcuts().size());
    for (std::size_t i = 0; i < shortcuts().size(); ++i) {
        EXPECT_EQ(shortcuts()[i].key, defaultShortcuts()[i].key);
        EXPECT_EQ(shortcuts()[i].cmd, defaultShortcuts()[i].cmd);
    }
}

TEST(UiKeymap, CustomKeysStealAndRoundTrip) {
    const KeymapGuard guard;
    applyKeymap(kKeysStandard, {{"Save", "F2"}});
    EXPECT_EQ(findShortcut(KEY_F2, 0, true)->cmd, Cmd::Save);
    EXPECT_EQ(findShortcut(KEY_S, MOD_CTRL, true), nullptr);
    ASSERT_EQ(keymapDiff(kKeysStandard).size(), 1u);
    EXPECT_EQ(keymapDiff(kKeysStandard)[0], (std::pair<std::string, std::string>{"Save", "F2"}));

    // Сочетание другого действия у того снимается.
    const auto robbed = setActionKeys(*findAction("Open"), {{KEY_N, MOD_CTRL}});
    ASSERT_EQ(robbed.size(), 1u);
    EXPECT_EQ(robbed[0]->cmd, Cmd::New);
    EXPECT_EQ(findShortcut(KEY_N, MOD_CTRL, true)->cmd, Cmd::Open);
    EXPECT_TRUE(keysIn(shortcuts(), Cmd::New, 0).empty());
    expectNoConflicts();

    // Записанное и прочитанное снова — та же таблица.
    const auto diff = keymapDiff(kKeysStandard);
    EXPECT_EQ(diff.size(), 3u);
    const std::vector<Shortcut> before(shortcuts().begin(), shortcuts().end());
    applyKeymap(kKeysStandard, {});
    applyKeymap(kKeysStandard, diff);
    ASSERT_EQ(shortcuts().size(), before.size());
    for (std::size_t i = 0; i < before.size(); ++i) {
        EXPECT_EQ(shortcuts()[i].key, before[i].key) << i;
        EXPECT_EQ(shortcuts()[i].mods, before[i].mods) << i;
        EXPECT_EQ(shortcuts()[i].cmd, before[i].cmd) << i;
    }
    // Команда меню без клавиши получает строку в своём разделе.
    setActionKeys(*findAction("Settings"), {{KEY_K, MOD_CTRL | MOD_SHIFT}});
    EXPECT_EQ(findShortcut(KEY_K, MOD_CTRL | MOD_SHIFT, true)->cmd, Cmd::Settings);
    EXPECT_EQ(commandKeys(Cmd::Settings, 0, true), "Ctrl+Shift+K");
}

TEST(UiKeymap, ConfigKeys) {
    Config c;
    c.applyLines({"KeyPreset = как в Aseprite", "Key.Save = F2 | Ctrl+S", "key.tool.0 = нет"});
    EXPECT_EQ(c.keyPreset, kKeysAseprite);
    ASSERT_EQ(c.keys.size(), 2u);
    EXPECT_EQ(c.keys[1].first, "tool.0");
    const std::string text = c.serialize();
    EXPECT_NE(text.find("KeyPreset = как в Aseprite"), std::string::npos);
    EXPECT_NE(text.find("Key.Save = F2 | Ctrl+S"), std::string::npos);
    // По умолчанию — ни строки: файл как у оригинала.
    EXPECT_EQ(Config{}.serialize().find("Key"), std::string::npos);
}

// --- D4: английский интерфейс ---------------------------------------------

namespace {

/// Язык и таблица — одни на процесс: тест возвращает русский.
struct LangGuard {
    LangGuard() = default;
    LangGuard(const LangGuard&) = delete;
    LangGuard& operator=(const LangGuard&) = delete;
    ~LangGuard() {
        i18n::setLanguage("ru");
        i18n::loadCatalogText({});
    }
};

std::string readSource(const std::string& rel) {
    std::ifstream f(std::string(TJTM_SOURCE_DIR) + "/" + rel, std::ios::binary);
    std::ostringstream ss;
    ss << f.rdbuf();
    return ss.str();
}

}  // namespace

TEST(UiI18n, CatalogRules) {
    const LangGuard guard;
    i18n::loadCatalogText(
        "# комментарий\n"
        "Сохранить\tSave\n"
        "тайл\ttile\n"
        "строки {}–{} из {}\tlines {}–{} of {}\n"
        "страница {} из {}\tpage {} of {}\n"
        "страница {} из {},  тайлов {}\tpage {} of {},  {} tiles\n"
        "Вид: {}\tKind: {}\n"
        "~Пробел\tSpace\n"
        "Две строки\\nвторая\tTwo lines\\nsecond\n");
    // По-русски — как в коде.
    EXPECT_EQ(i18n::text("Сохранить"), "Сохранить");
    i18n::setLanguage("en");
    EXPECT_EQ(i18n::text("Сохранить"), "Save");
    EXPECT_EQ(i18n::text("Нет в таблице"), "Нет в таблице");
    EXPECT_EQ(i18n::text("Already English"), "Already English");
    EXPECT_EQ(i18n::text("строки 1–23 из 399"), "lines 1–23 of 399");
    // Точный шаблон — раньше общего.
    EXPECT_EQ(i18n::text("страница 1 из 2,  тайлов 17"), "page 1 of 2,  17 tiles");
    EXPECT_EQ(i18n::text("страница 1 из 2"), "page 1 of 2");
    // Подставленное переводится тоже.
    EXPECT_EQ(i18n::text("Вид: тайл"), "Kind: tile");
    EXPECT_EQ(i18n::text("Ctrl+Пробел"), "Ctrl+Space");
    EXPECT_EQ(i18n::text("Две строки\nвторая"), "Two lines\nsecond");
    {
        const i18n::Raw raw;
        EXPECT_EQ(i18n::text("Сохранить"), "Сохранить");
    }
    EXPECT_TRUE(i18n::has("Сохранить"));
    EXPECT_FALSE(i18n::has("Нет в таблице"));
}

TEST(UiI18n, ShippedCatalogIsClean) {
    const std::string text = readSource("Resources/Lang/en.txt");
    ASSERT_FALSE(text.empty());
    const auto bad = i18n::catalogProblems(text);
    EXPECT_TRUE(bad.empty()) << bad.size() << " строк, первая: " << (bad.empty() ? "" : bad[0]);
    const LangGuard guard;
    EXPECT_GT(i18n::loadCatalogText(text), 1000);
    i18n::setLanguage("en");
    EXPECT_EQ(i18n::text("Файл"), "File");
    EXPECT_EQ(i18n::text("Настроить клавиши…"), "Customize keys…");
}

TEST(UiI18n, HelpSectionsExistInEnglishHelp) {
    // Кнопка «?» экрана открывает справку на разделе по его русскому
    // заголовку; по-английски раздел ищется по переводу заголовка.
    const LangGuard guard;
    i18n::loadCatalogText(readSource("Resources/Lang/en.txt"));
    i18n::setLanguage("en");
    const std::string help = readSource("Resources/Lang/help.en.txt");
    ASSERT_FALSE(help.empty());
    for (const char* section :
         {"НАБОР ДЛЯ ИГРЫ", "ЛИНЕЙКИ, НАПРАВЛЯЮЩИЕ, СЕТКА", "НАСТРОЙКИ", "НАБОРЫ ТАЙЛОВ",
          "СЛОИ, КАДРЫ И АНИМАЦИЯ", "ВНЕ ПИКСЕЛЬ-АРТА", "ИНСТРУМЕНТЫ РЕЖИМА ТАЙЛА",
          "ВЫДЕЛЕНИЕ И ПРЕОБРАЗОВАНИЯ", "РЕДАКТОР КАРТ", "ИНСТРУМЕНТЫ РЕЖИМА КАРТЫ", "ПАЛИТРА",
          "КЛАВИШИ"}) {
        const std::string en = i18n::tr(section);
        EXPECT_NE(en, section);
        EXPECT_NE(help.find("\n" + en + "\n"), std::string::npos) << en;
    }
    EXPECT_NE(help.find("{KEYS}"), std::string::npos);
    EXPECT_NE(help.find("{VERSION}"), std::string::npos);
}

TEST(UiI18n, ConfigLanguage) {
    Config c;
    EXPECT_EQ(c.language, "русский");
    c.applyLines({"Language = English"});
    EXPECT_EQ(c.language, "english");
    EXPECT_NE(c.serialize().find("Language = english"), std::string::npos);
    EXPECT_EQ(Config{}.serialize().find("Language"), std::string::npos);
}

// --- D6: нажим пера --------------------------------------------------------

TEST(UiPen, PressureScalesStrength) {
    Model m;
    m.brushOpacity = 255;
    m.penPressure = 0.3F;
    // У модели по умолчанию — как у оригинала: нажим не действует.
    EXPECT_EQ(m.strokeOpacity(), 255);
    m.pressureStrength = true;
    EXPECT_EQ(m.strokeOpacity(), 77);
    m.penPressure = 0.0F;
    EXPECT_EQ(m.strokeOpacity(), 1);  // касание всё же кладёт цвет
    m.penPressure = 1.0F;
    m.brushOpacity = 128;
    EXPECT_EQ(m.strokeOpacity(), 128);

    Config c;
    EXPECT_FALSE(c.penPressureBrush);
    EXPECT_TRUE(c.penPressureAir);
    c.applyLines({"PenPressureBrush = да", "PenPressureAir = нет"});
    EXPECT_TRUE(c.penPressureBrush);
    EXPECT_FALSE(c.penPressureAir);
    EXPECT_NE(c.serialize().find("PenPressureBrush = да"), std::string::npos);
    EXPECT_EQ(Config{}.serialize().find("PenPressure"), std::string::npos);
}

// --- D7: доступность -------------------------------------------------------

TEST(UiAccess, OnionPalettesAndMenuKey) {
    // Пары цветов луковой кожи различимы и без красного с зелёным: у каждой
    // пары прошлое и будущее разнятся по яркости или по синему каналу.
    for (int k = 0; k < Model::kOnionPalettes; ++k) {
        const Color p = Model::onionColor(k, true);
        const Color f = Model::onionColor(k, false);
        EXPECT_NE(p, f) << k;
        EXPECT_GT(std::abs(static_cast<int>(p.b) - static_cast<int>(f.b)) +
                      std::abs((p.r + p.g) - (f.r + f.g)),
                  100)
            << k;
    }
    // Нулевая пара — та, что была до выбора: эталоны анимации не меняются.
    EXPECT_EQ(Model::onionColor(0, true), rgb(220, 70, 70));
    EXPECT_EQ(Model::onionColor(0, false), rgb(70, 110, 230));
    Config c;
    c.applyLines({"OnionColors = 1"});
    EXPECT_EQ(c.onionColors, 1);
    EXPECT_NE(c.serialize().find("OnionColors = 1"), std::string::npos);
    EXPECT_EQ(Config{}.serialize().find("OnionColors"), std::string::npos);
    // Строка меню — с клавиатуры.
    EXPECT_EQ(findShortcut(KEY_F10, 0, true)->cmd, Cmd::MenuBar);
}

// --- D8: схема из акцента, контрастные темы, файлы тем ----------------------

TEST(UiScheme, GeneratorMatchesTable) {
    // Генератор повторяет таблицу приложения А (docs/ui-design-system.md) с
    // точностью до единицы-двух в канале: тон L* CIELAB, оттенок OKLCH.
    using namespace theme;
    auto near = [](Color a, Color b) {
        return std::abs(a.r - b.r) <= 2 && std::abs(a.g - b.g) <= 2 && std::abs(a.b - b.b) <= 2;
    };
    const Theme l = lightTheme();
    const Theme d = darkTheme();
    EXPECT_TRUE(near(neutralTone(kDefaultSeed, 98, 0.006), l.window));
    EXPECT_TRUE(near(neutralTone(kDefaultSeed, 10, 0.006), l.text));
    EXPECT_TRUE(near(neutralTone(kDefaultSeed, 30, 0.012), l.textDim));
    EXPECT_TRUE(near(neutralTone(kDefaultSeed, 50, 0.012), l.outline));
    EXPECT_TRUE(near(toneOf(kDefaultSeed, 40), l.accent));
    EXPECT_TRUE(near(toneOf(kDefaultSeed, 90), l.accentContainer));
    EXPECT_TRUE(near(toneOf(kDefaultSeed, 80), d.accent));
    EXPECT_TRUE(near(toneOf(kDefaultSeed, 30), d.accentContainer));
    // При акценте 1.9 светлая и тёмная — прежние таблицы точка в точку.
    EXPECT_EQ(schemeTheme(kDefaultSeed, Variant::Light).accent, l.accent);
    EXPECT_EQ(schemeTheme(kDefaultSeed, Variant::Dark).window, d.window);
}

TEST(UiScheme, ContrastForEveryAccentAndVariant) {
    // Нормы WCAG 2.1 из ui-design-system.md: текст 4,5:1, границы 3:1.
    using namespace theme;
    std::vector<Color> seeds;
    for (const Accent& a : accents())
        seeds.push_back(a.seed);
    seeds.push_back(rgb(0xFF, 0xE0, 0x00));  // свой: жёлтый, самый трудный
    seeds.push_back(rgb(0x80, 0x80, 0x80));  // свой: серый без цветности
    for (const Color seed : seeds) {
        for (const Variant v :
             {Variant::Light, Variant::Dark, Variant::LightContrast, Variant::DarkContrast}) {
            const Theme t = schemeTheme(seed, v);
            const std::string what = hexColor(seed) + " " + t.name;
            EXPECT_GE(contrastRatio(t.text, t.window), 4.5) << what;
            EXPECT_GE(contrastRatio(t.text, t.back), 4.5) << what;
            EXPECT_GE(contrastRatio(t.textDim, t.back), 4.5) << what;
            EXPECT_GE(contrastRatio(t.textDim, t.face), 4.5) << what;
            EXPECT_GE(contrastRatio(t.outline, t.window), 3.0) << what;
            EXPECT_GE(contrastRatio(t.onAccent, t.accent), 4.5) << what;
            EXPECT_GE(contrastRatio(t.onAccentContainer, t.accentContainer), 4.5) << what;
            EXPECT_GE(contrastRatio(t.accent, t.window), 3.0) << what;
            EXPECT_GE(contrastRatio(t.warn, t.window), 3.0) << what;
        }
    }
    // Контрастные — заметно контрастнее обычных.
    const Theme l = schemeTheme(kDefaultSeed, Variant::Light);
    const Theme lc = schemeTheme(kDefaultSeed, Variant::LightContrast);
    EXPECT_GT(contrastRatio(lc.outline, lc.window), contrastRatio(l.outline, l.window) + 3);
}

TEST(UiScheme, ThemeFiles) {
    using namespace theme;
    ThemeFile f;
    std::vector<std::string> problems;
    ASSERT_TRUE(
        parseThemeFile("name = Проба\nbase = тёмная   # комментарий\nseed = #7445B4\n"
                       "shape.corners = square\ncolor.canvas.backdrop = #2B2B2B\n"
                       "color.нет_такой = #000000\nмусор\n",
                       f, problems));
    EXPECT_EQ(f.name, "Проба");
    EXPECT_EQ(problems.size(), 2u);
    const Theme t = buildTheme("проба", kDefaultSeed, false, {f});
    EXPECT_STREQ(t.name, "Проба");
    EXPECT_EQ(t.canvas, rgb(0x2B, 0x2B, 0x2B));
    EXPECT_EQ(t.radius, 0);
    EXPECT_LT(t.window.r, 40);  // тёмная база
    EXPECT_EQ(t.accent, schemeTheme(rgb(0x74, 0x45, 0xB4), Variant::Dark).accent);
    // Без name — не тема.
    ThemeFile g;
    EXPECT_FALSE(parseThemeFile("seed = #123456\n", g, problems));
    // Встроенные файлы тем разбираются без замечаний.
    for (const char* file : {"Resources/Themes/dusk.txt", "Resources/Themes/pine.txt"}) {
        ThemeFile h;
        std::vector<std::string> bad;
        EXPECT_TRUE(parseThemeFile(readSource(file), h, bad)) << file;
        EXPECT_TRUE(bad.empty()) << file << ": " << (bad.empty() ? "" : bad[0]);
    }
    // «Как в системе» следует системе.
    EXPECT_LT(buildTheme("как в системе", kDefaultSeed, true, {}).window.r, 40);
    EXPECT_GT(buildTheme("как в системе", kDefaultSeed, false, {}).window.r, 200);
    Color c;
    EXPECT_TRUE(parseHexColor("#7445b4", c));
    EXPECT_EQ(hexColor(c), "#7445B4");
    EXPECT_FALSE(parseHexColor("7445", c));

    Config cfg;
    cfg.applyLines({"Theme = dark contrast", "ThemeAccent = #7445B4"});
    EXPECT_EQ(cfg.theme, "тёмная контрастная");
    EXPECT_EQ(cfg.themeAccent, "#7445B4");
    EXPECT_NE(cfg.serialize().find("ThemeAccent = #7445B4"), std::string::npos);
    EXPECT_EQ(Config{}.serialize().find("ThemeAccent"), std::string::npos);
}

// --- C7: значки одного стиля -----------------------------------------------

TEST(UiIcons, ModernSetIsComplete) {
    // Набор 2.0 (tools/icons.py): у каждого инструмента — значок 16×16,
    // показ ровно ×2; две маски — форма и акцент, остальное прозрачно.
    for (int i = 0; i < iconCount(); ++i) {
        const std::string path =
            std::string(TJTM_SOURCE_DIR) + "/Resources/Icons/modern/" + iconFile(i) + ".png";
        Grid<Color> g;
        std::string error;
        ASSERT_TRUE(pngLoad(path, g, error)) << iconFile(i) << ": " << error;
        EXPECT_EQ(g.w, 16) << iconFile(i);
        EXPECT_EQ(g.h, 16) << iconFile(i);
        std::vector<Color> colors;
        int opaque = 0;
        for (const Color c : g.d) {
            if (c.a == 0) continue;
            ++opaque;
            if (std::find(colors.begin(), colors.end(), c) == colors.end()) colors.push_back(c);
        }
        EXPECT_LE(colors.size(), 2u) << iconFile(i) << ": форма и акцент";
        EXPECT_GT(opaque, 16) << iconFile(i);
    }
    // Значки 1.6 на месте: на них стоит «Классика» и сверка с оригиналом.
    for (int i = 0; i < iconCount(); ++i) {
        Grid<Color> g;
        std::string error;
        EXPECT_TRUE(pngLoad(
            std::string(TJTM_SOURCE_DIR) + "/Resources/Icons/" + iconFile(i) + ".png", g, error))
            << iconFile(i);
    }
}

}  // namespace tjtm
