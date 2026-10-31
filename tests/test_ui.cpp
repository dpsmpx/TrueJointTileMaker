// План «Интерфейс» (docs/roadmap.md): то, что из него живёт в модели.

#include <gtest/gtest.h>

#include <cmath>

#include "core/model.h"
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

}  // namespace
}  // namespace tjtm
