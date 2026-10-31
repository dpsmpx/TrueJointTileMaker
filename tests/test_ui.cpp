// План «Интерфейс» (docs/roadmap.md): то, что из него живёт в модели.

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <vector>

#include "core/config.h"
#include "core/model.h"
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
        const bool found = std::any_of(all.begin(), all.end(), [&](const Shortcut& s) {
            return static_cast<int>(s.cmd) == c;
        });
        EXPECT_TRUE(found) << "команда " << c << " без клавиши";
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

}  // namespace
}  // namespace tjtm
