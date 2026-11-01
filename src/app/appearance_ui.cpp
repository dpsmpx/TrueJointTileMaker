// Экран «Оформление» (план «Интерфейс», D8, ДС-5). У 1.6 облик был один, у
// 1.9 — три темы переключателем в настройках. Здесь — встроенные темы,
// включая контрастные и «как в системе», темы из файлов и акцент: девять
// готовых или свой цвет. Выбор применяется сразу, и сам экран — его
// предпросмотр; «Отмена» возвращает прежнее.

#include <algorithm>

#include "app/editor.h"
#include "core/text.h"
#include "core/theme.h"

namespace tjtm {

using namespace theme;

void Editor::showAppearance() {
    const std::string oldTheme = cfg.theme;
    const std::string oldAccent = cfg.themeAccent;
    std::string custom = cfg.themeAccent.empty() ? hexColor(kDefaultSeed) : cfg.themeAccent;
    bool done = false;
    bool ok = false;
    dropInput();
    pf_.input().keys.clear();
    auto seedNow = [&]() {
        Color c = kDefaultSeed;
        if (!cfg.themeAccent.empty()) parseHexColor(cfg.themeAccent, c);
        return c;
    };
    // Имя для людей: «Светлая контрастная», имя из файла — как есть.
    auto title = [](const std::string& n) {
        std::u32string u = utf8ToU32(n);
        if (!u.empty() && u[0] >= U'а' && u[0] <= U'я') u[0] = u[0] - U'а' + U'А';
        return u32ToUtf8(u);
    };
    constexpr int LX = 40;
    constexpr int RX = 340;
    while (!done && !quitNow_) {
        beginFrameInput();
        fb_.clear(cWindow);
        uiTitle(0, 10, WIN_W, 42, "Оформление");
        uiHelpButton(WIN_W - 44, 11, "НАСТРОЙКИ");

        // --- тема ---
        uiLabelLeft(LX, 52, 26, "Тема", cTextDim);
        std::vector<std::string> names;
        for (const char* n : builtinThemeNames())
            names.emplace_back(n);
        for (const ThemeFile& f : themeFiles_)
            names.push_back(f.name);
        std::string pick;
        int y = 80;
        for (const std::string& n : names) {
            if (y > WIN_H - 100) break;
            if (uiToggle(LX, y, 260, 30, title(n), netToLower(cfg.theme) == netToLower(n)))
                pick = n;
            y += 34;
        }
        if (uiButton(LX, y + 4, 260, 30, "Перечитать темы из файлов", true)) {
            loadThemeFiles();
            applyTheme();
        }

        // --- акцент ---
        const bool classic = cfg.theme == "классика";
        uiLabelLeft(RX, 52, 26, classic ? "Акцент — у новых тем" : "Акцент", cTextDim);
        const Color seed = seedNow();
        int k = 0;
        for (const Accent& a : accents()) {
            const int ax = RX + (k % 3) * 136;
            const int ay = 80 + (k / 3) * 40;
            const bool on = seed == a.seed;
            // Образец — цвет главной кнопки этой схемы, а не сам акцент:
            // так он и будет выглядеть.
            const Color sw =
                schemeTheme(a.seed, active.window.r < 128 ? Variant::Dark : Variant::Light).accent;
            if (uiToggle(ax, ay, 130, 34, "", on) && !classic) {
                cfg.themeAccent = a.seed == kDefaultSeed ? "" : hexColor(a.seed);
                custom = hexColor(a.seed);
                applyTheme();
            }
            fb_.fillRoundRect(ax + 8, ay + 9, ax + 26, ay + 25, 8, 8, sw);
            textOut(ax + 34, ay + 9, a.name, on ? active.onAccentContainer : cText);
            ++k;
        }
        const int cy = 80 + 3 * 40 + 8;
        uiLabelLeft(RX, cy, 30, "Свой", cText);
        uiTextLine(RX + 60, cy, 120, 30, custom);
        Color mine;
        const bool valid = parseHexColor(custom, mine);
        if (uiButton(RX + 186, cy, 100, 30, "Применить", valid && !classic)) {
            cfg.themeAccent = mine == kDefaultSeed ? "" : hexColor(mine);
            applyTheme();
        }
        if (uiButton(RX + 292, cy, 120, 30, "Первый цвет", !classic)) {
            custom = hexColor(m.firstColor);
            cfg.themeAccent = hexColor(m.firstColor);
            applyTheme();
        }

        // --- предпросмотр: элементы в выбранной теме ---
        const int py = cy + 50;
        fb_.fillRoundRect(RX, py, RX + 412, py + 150, active.radius * 2, active.radius * 2, cPanel);
        textOut(RX + 14, py + 10, "Подпись и текст", cText);
        textOut(RX + 14, py + 30, "Подсказка — тише текста", cTextDim);
        uiPrimary(RX + 14, py + 58, 120, 32, "Главная", true);
        uiButton(RX + 142, py + 58, 120, 32, "Обычная", true);
        uiToggle(RX + 270, py + 58, 128, 32, "Выбрано", true);
        uiButton(RX + 14, py + 102, 120, 32, "Недоступна", false);
        textOut(RX + 146, py + 110, "Предупреждение", cWarn);
        // Контраст подписи к фону — числом: тема из файла его не обещает.
        const double cr = contrastRatio(cText, cWindow);
        textOut(RX + 270, py + 110,
                "контраст " + std::to_string(static_cast<int>(cr * 10) / 10) + "," +
                    std::to_string(static_cast<int>(cr * 10) % 10) + ":1",
                cr >= 4.5 ? cTextDim : cWarn);

        if (uiButton(WIN_W - 24 - 284, WIN_H - 52, 140, 36, "Отмена", true)) done = true;
        if (uiPrimary(WIN_W - 24 - 140, WIN_H - 52, 140, 36, "Готово", true)) {
            ok = true;
            done = true;
        }
        redraw();
        if (!pick.empty() && pick != cfg.theme) {
            cfg.theme = netToLower(pick);
            applyTheme();
        }
        const int key = takeKey();
        if (key == KEY_ESC) done = true;
        idle();
    }
    waitMouseRelease();
    if (ok) {
        saveConfig();
    } else {
        cfg.theme = oldTheme;
        cfg.themeAccent = oldAccent;
        applyTheme();
    }
    m.needRepaint = true;
}

}  // namespace tjtm
