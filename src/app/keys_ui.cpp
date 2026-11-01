// Правка клавиш (план «Интерфейс», D3, UX-18). У 1.6 клавиши были зашиты в
// код, а инструменты — цифрами, хотя в других редакторах это буквы: B —
// кисть, E — ластик, G — заливка. Здесь любое действие таблицы сочетаний и
// любая команда меню получает свои клавиши, а набор «как в Aseprite»
// ставит привычные по нему разом. Таблица одна, так что шпаргалка, меню и
// справка сразу показывают новые клавиши.

#include <algorithm>

#include "app/editor.h"
#include "core/text.h"
#include "core/theme.h"

namespace tjtm {

using namespace theme;

std::string Editor::keyActionTitle(const KeyAction& a) const {
    if (a.cmd != Cmd::Tool) return a.title;
    // Цифра выбирает инструмент панели своего режима: у тайла и у карты
    // под одним номером разные.
    static constexpr const char* kTile[] = {"карандаш", "ластик", "заливка",
                                            "пипетка",  "линия",  "прямоугольник",
                                            "овал",     "узор",   "выделение"};
    static constexpr const char* kMap[] = {"местность", "заливка местностью",
                                           "прямоугольник местности", "объекты", "выделение"};
    const auto i = static_cast<std::size_t>(a.arg);
    std::string t = "Инструмент " + std::to_string(a.arg + 1);
    if (i < std::size(kTile)) t += ": " + std::string(kTile[i]);
    if (i < std::size(kMap)) t += " / " + std::string(kMap[i]);
    return t;
}

void Editor::showKeysScreen() {
    // Правки идут сразу в действующую таблицу; «Отмена» собирает прежнюю.
    const std::string oldPreset = cfg.keyPreset;
    const auto oldKeys = cfg.keys;
    std::string preset = cfg.keyPreset;
    std::vector<Shortcut> base = presetTable(preset);
    const std::vector<KeyAction>& acts = keyActions();
    constexpr int LX = 24;
    constexpr int LY = 90;
    constexpr int RH = 28;
    constexpr int KX = 330;  // столбец клавиш
    const int rowsVis = (WIN_H - 62 - LY) / RH;
    std::string query;
    int top = 0;
    int capture = -1;  // номер действия, которому ждём сочетание
    bool captureAdd = false;
    bool done = false;
    bool ok = false;
    dropInput();
    pf_.input().keys.clear();

    // Разделы — в порядке шпаргалки; команды меню без клавиш — в своих.
    std::vector<std::string> groups;
    for (const KeyAction& a : acts) {
        if (std::find(groups.begin(), groups.end(), a.group) == groups.end())
            groups.push_back(a.group);
    }
    struct Item {
        int act = -1;  // -1 — заголовок раздела
        std::string group;
    };

    while (!done && !quitNow_) {
        keyCapture_ = capture >= 0;
        beginFrameInput();
        fb_.clear(cWindow);
        uiTitle(0, 8, WIN_W, 40, "Клавиши");
        uiHelpButton(WIN_W - 44, 9, "КЛАВИШИ");
        const bool ase = preset == kKeysAseprite;
        uiLabelLeft(LX, 48, 30, "Набор", cText);
        // Набор меняется после кадра: вопрос встаёт над целым экраном.
        const char* choose = nullptr;
        if (uiToggle(LX + 64, 48, 140, 30, "Стандартный", !ase)) choose = kKeysStandard;
        if (uiToggle(LX + 208, 48, 150, 30, "Как в Aseprite", ase)) choose = kKeysAseprite;
        const int sx = WIN_W - 24 - 250;
        uiTextLine(sx, 48, 250, 30, query);
        const bool typing = textEditId_ == reinterpret_cast<std::uintptr_t>(&query);
        const std::string live = typing ? u32ToUtf8(textEdit_.text) : query;
        if (live.empty() && !typing) textOut(sx + 10, 54, "Найти действие или клавишу", cTextDim);
        const std::string want = netToLower(netTrim(live));

        // Список: заголовки разделов и действия, отобранные поиском.
        std::vector<Item> items;
        for (const std::string& g : groups) {
            bool head = false;
            for (std::size_t i = 0; i < acts.size(); ++i) {
                const KeyAction& a = acts[i];
                if (a.group != g) continue;
                const std::string keys = keysText(keysIn(shortcuts(), a.cmd, a.arg));
                if (!want.empty() &&
                    netToLower(keyActionTitle(a) + " " + keys).find(want) == std::string::npos)
                    continue;
                if (!head) items.push_back({-1, g});
                head = true;
                items.push_back({static_cast<int>(i), g});
            }
        }
        const int maxTop = std::max(0, static_cast<int>(items.size()) - rowsVis);
        top = clampI(top, 0, maxTop);

        for (int r = 0; r < rowsVis && top + r < static_cast<int>(items.size()); ++r) {
            const Item& it = items[static_cast<std::size_t>(top + r)];
            const int y = LY + r * RH;
            if (it.act < 0) {
                headFont_.draw(fb_, LX, y + 4, it.group, cAccent);
                continue;
            }
            const KeyAction& a = acts[static_cast<std::size_t>(it.act)];
            const std::vector<KeyCombo> now = keysIn(shortcuts(), a.cmd, a.arg);
            const bool own = now != keysIn(base, a.cmd, a.arg);
            const bool here = capture == it.act;
            if (here) fb_.fillRect(LX - 6, y, WIN_W - LX + 6, y + RH - 2, active.fieldSelection);
            textOut(LX + 8, y + 5, fitTextEnd(keyActionTitle(a), KX - LX - 16), cText);
            std::string keys;
            for (const KeyCombo& k : now)
                keys += (keys.empty() ? "" : ", ") + keyName(k.key, k.mods);
            if (here) {
                textOut(KX, y + 5, captureAdd ? "Ещё одно сочетание…" : "Нажмите сочетание…",
                        cAccent);
            } else {
                // Свои — акцентом и точкой: не только цветом (D7).
                textOut(KX, y + 5,
                        fitTextEnd(own ? "• " + (keys.empty() ? "нет" : keys)
                                       : (keys.empty() ? "—" : keys),
                                   236),
                        own            ? cAccent
                        : keys.empty() ? cTextDim
                                       : cText);
            }
            const int bx = WIN_W - 24 - 196;
            if (a.fixed) {
                uiLabelLeft(bx, y, RH - 2, "не меняется", cTextDim);
                continue;
            }
            if (here) {
                if (uiButton(bx, y + 1, 110, RH - 4, "Без клавиши", !now.empty())) {
                    setActionKeys(a, {});
                    capture = -1;
                }
                if (uiButton(bx + 114, y + 1, 82, RH - 4, "Отмена", true)) capture = -1;
                continue;
            }
            uiTip(bx, y + 1, 30, RH - 4, "Добавить ещё одно сочетание");
            if (uiButton(bx, y + 1, 30, RH - 4, "+", true)) {
                capture = it.act;
                captureAdd = true;
            }
            if (uiButton(bx + 34, y + 1, 80, RH - 4, "Задать", true)) {
                capture = it.act;
                captureAdd = false;
            }
            if (uiButton(bx + 118, y + 1, 78, RH - 4, "Сброс", own))
                setActionKeys(a, keysIn(base, a.cmd, a.arg));
        }
        if (items.empty()) textOut(LX + 8, LY + 6, "Ничего не нашлось", cTextDim);

        const int by = WIN_H - 48;
        if (uiButton(LX, by, 150, 34, "Сбросить всё", !keymapDiff(preset).empty())) {
            applyKeymap(preset, {});
            capture = -1;
        }
        if (maxTop > 0)
            uiLabelLeft(
                LX + 164, by, 34,
                "строки " + std::to_string(top + 1) + "–" +
                    std::to_string(std::min(static_cast<int>(items.size()), top + rowsVis)) +
                    " из " + std::to_string(items.size()),
                cTextDim);
        if (uiButton(WIN_W - 24 - 284, by, 140, 34, "Отмена", true)) done = true;
        if (uiPrimary(WIN_W - 24 - 140, by, 140, 34, "Готово", true)) {
            ok = true;
            done = true;
        }
        redraw();

        if (choose != nullptr && preset != choose) {
            const std::size_t own = keymapDiff(preset).size();
            if (own == 0 || askYesNo("Сменить набор клавиш?",
                                     "Свои сочетания (" + std::to_string(own) +
                                         ") сбросятся: набор ставит клавиши заново.",
                                     "Сменить", "Оставить")) {
                preset = choose;
                applyKeymap(preset, {});
                base = presetTable(preset);
                capture = -1;
            }
        }
        if (capture >= 0) {
            // Ждём сочетание: Shift, Ctrl и Alt сами по себе — не клавиша.
            Input& in = pf_.input();
            while (!in.keys.empty() && capture >= 0) {
                const KeyPress k = in.keys.front();
                in.keys.pop_front();
                if (k.vk == 0 || k.vk == KEY_SHIFT || k.vk == KEY_CTRL || k.vk == KEY_ALT) continue;
                const int mods =
                    (k.shift ? MOD_SHIFT : 0) | (k.ctrl ? MOD_CTRL : 0) | (k.alt ? MOD_ALT : 0);
                if (k.vk == KEY_ESC && mods == 0) {
                    capture = -1;
                    break;
                }
                const KeyAction& a = acts[static_cast<std::size_t>(capture)];
                std::vector<KeyCombo> keys;
                if (captureAdd) keys = keysIn(shortcuts(), a.cmd, a.arg);
                const KeyCombo c{k.vk, mods};
                if (std::find(keys.begin(), keys.end(), c) == keys.end()) keys.push_back(c);
                const auto robbed = setActionKeys(a, keys);
                if (!robbed.empty()) {
                    std::string who;
                    for (const KeyAction* b : robbed)
                        who += (who.empty() ? "«" : ", «") + keyActionTitle(*b) + "»";
                    notify(keyName(c.key, c.mods) + " — теперь «" + keyActionTitle(a) + "»",
                           "У " + who + " это сочетание снято.");
                }
                capture = -1;
            }
        } else {
            const int key = takeKey();
            if (key == KEY_ESC) done = true;
            if (key == KEY_DOWN) ++top;
            if (key == KEY_UP) --top;
            if (key == KEY_PGDN) top += rowsVis;
            if (key == KEY_PGUP) top -= rowsVis;
        }
        top -= takeWheel() * 3;
        idle();
    }
    keyCapture_ = false;
    waitMouseRelease();
    if (ok) {
        cfg.keyPreset = preset;
        cfg.keys = keymapDiff(preset);
        saveConfig();
    } else {
        applyKeymap(oldPreset, oldKeys);
    }
    m.needRepaint = true;
}

}  // namespace tjtm
