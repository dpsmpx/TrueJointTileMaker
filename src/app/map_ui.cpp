// Редактор карт (этап 10): форма объекта вместо хвоста строки, кисть
// смесью местностей, поиск и замена. Ядро — src/core/mapobj.* и
// src/core/model_map.cpp.

#include <algorithm>
#include <numeric>

#include "app/editor.h"
#include "app/textfield.h"
#include "core/fsutil.h"
#include "core/mapobj.h"
#include "core/pmath.h"
#include "core/text.h"
#include "core/theme.h"

namespace tjtm {

using namespace theme;

// ---------------------------------------------------------------------------
// Выбор из списка
// ---------------------------------------------------------------------------

int Editor::askChoice(const std::string& title, const std::string& hint,
                      const std::vector<std::string>& items) {
    constexpr int ROWS = 14;
    constexpr int RH = 28;
    constexpr int LX = 90;
    constexpr int LW = WIN_W - 2 * LX;
    int page = 0;
    int chosen = -1;
    bool done = false;
    dropInput();
    pf_.input().keys.clear();
    const int pages = std::max(1, (static_cast<int>(items.size()) + ROWS - 1) / ROWS);
    while (!done && !quitNow_) {
        beginFrameInput();
        focusArrows_ = true;  // список: стрелки ходят по строкам (UX-19)
        fb_.clear(cWindow);
        textCentered(0, 8, WIN_W, 36, title, cText);
        uiLabel(0, 36, WIN_W, 20, hint, cTextDim);
        if (items.empty()) uiLabel(0, 200, WIN_W, 24, "Список пуст", cTextDim);
        for (int r = 0; r < ROWS; ++r) {
            const int i = page * ROWS + r;
            if (i >= static_cast<int>(items.size())) break;
            if (uiButton(LX, 66 + r * (RH + 3), LW, RH,
                         fitText(items[static_cast<std::size_t>(i)], LW - 16), true)) {
                chosen = i;
                done = true;
            }
        }
        if (uiButton(LX, WIN_H - 52, 110, 34, "Назад", page > 0)) --page;
        uiLabel(LX + 116, WIN_H - 52, 200, 34,
                "страница " + std::to_string(page + 1) + " из " + std::to_string(pages), cTextDim);
        if (uiButton(LX + 322, WIN_H - 52, 110, 34, "Дальше", page < pages - 1)) ++page;
        if (uiButton(LX + LW - 160, WIN_H - 52, 160, 34, "Отмена", true)) done = true;
        redraw();
        const int key = takeKey();
        if (key == KEY_ESC) done = true;
        if (key == KEY_PGDN && page < pages - 1) ++page;
        if (key == KEY_PGUP && page > 0) --page;
        idle();
    }
    waitMouseRelease();
    m.needRepaint = true;
    return chosen;
}

// ---------------------------------------------------------------------------
// Форма объекта
// ---------------------------------------------------------------------------

void Editor::showObjectForm(int idx) {
    if (idx < 0 || idx >= static_cast<int>(m.map.objects.size())) return;
    const MapObject orig = m.map.objects[static_cast<std::size_t>(idx)];
    // Координаты — тоже поля формы: объект переносят, исправив число.
    std::vector<mapobj::Field> fields;
    fields.push_back({"X", std::to_string(orig.x), true, false, false, ""});
    fields.push_back({"Y", std::to_string(orig.y), true, false, false, ""});
    const std::vector<mapobj::Field> own = mapobj::fieldsOf(orig.kind, orig.rest);
    fields.insert(fields.end(), own.begin(), own.end());
    std::vector<TextField> boxes(fields.size());
    for (std::size_t k = 0; k < fields.size(); ++k) {
        boxes[k].text = utf8ToU32(fields[k].value);
        boxes[k].caret = boxes[k].anchor = boxes[k].text.size();
    }
    constexpr int LX = 40;
    constexpr int BX = 200;
    constexpr int BH = 28;
    constexpr int TOP = 64;
    const int rowH = fields.size() > 6 ? 46 : 54;
    auto boxY = [&](std::size_t k) { return TOP + static_cast<int>(k) * rowH; };
    auto boxW = [&](std::size_t k) {
        // У перехода рядом с локацией — выбор карты из папки карт.
        return orig.kind == "exit" && k == 2 ? 420 : WIN_W - BX - 40;
    };
    std::size_t focus = fields.size() > 2 ? 2 : 0;
    bool done = false;
    bool ok = false;
    bool removed = false;
    bool raw = false;
    dropInput();
    pf_.input().keys.clear();
    pf_.input().presses.clear();
    pf_.startTextInput(BX, boxY(focus), boxW(focus), BH);
    const int blinkStart = pf_.milliseconds();

    auto width = [&](const TextField& tf, std::size_t n) {
        return mono_.rawWidth(u32ToUtf8(std::u32string_view(tf.text).substr(0, n)));
    };
    auto current = [&]() {
        std::vector<mapobj::Field> f(fields.begin() + 2, fields.end());
        for (std::size_t k = 0; k < f.size(); ++k)
            f[k].value = u32ToUtf8(boxes[k + 2].text);
        return f;
    };

    while (!done && !quitNow_) {
        // Поля формы ходят по Tab сами, со своей кареткой: общий фокус
        // кнопок им бы мешал.
        beginFrameInput(false);
        Input& in = pf_.input();
        auto setFocus = [&](std::size_t k) {
            focus = k;
            pf_.startTextInput(BX, boxY(k), boxW(k), BH);
        };
        if (frameClick_ && frameClickButton_ == MB_LEFT) {
            for (std::size_t k = 0; k < fields.size(); ++k) {
                const int y = boxY(k);
                if (frameClickX_ >= BX && frameClickX_ < BX + boxW(k) && frameClickY_ >= y &&
                    frameClickY_ < y + BH) {
                    frameClick_ = false;
                    setFocus(k);
                    TextField& tf = boxes[k];
                    // Каретка — к ближайшей границе знаков под щелчком.
                    const int x = frameClickX_ - (BX + 6) + tf.scroll;
                    std::size_t best = 0;
                    int bestD = 1 << 30;
                    for (std::size_t i = 0; i <= tf.text.size(); ++i) {
                        const int d = std::abs(width(tf, i) - x);
                        if (d < bestD) {
                            bestD = d;
                            best = i;
                        }
                    }
                    tf.caret = tf.anchor = best;
                }
            }
        }
        while (!in.presses.empty()) {
            const KeyPress kp = in.presses.front();
            in.presses.pop_front();
            if (kp.vk == KEY_ESC) {
                done = true;
            } else if (kp.vk == KEY_ENTER) {
                ok = true;
            } else if (kp.vk == KEY_TAB) {
                setFocus((focus + (kp.shift ? fields.size() - 1 : 1)) % fields.size());
            } else if (kp.vk == KEY_UP && focus > 0) {
                setFocus(focus - 1);
            } else if (kp.vk == KEY_DOWN && focus + 1 < fields.size()) {
                setFocus(focus + 1);
            } else {
                editTextField(boxes[focus], kp, pf_);
            }
        }
        if (!in.text.empty()) {
            boxes[focus].insert(utf8ToU32(in.text));
            in.text.clear();
        }
        in.keys.clear();

        // Что получилось и годится ли оно игре.
        const std::vector<mapobj::Field> f = current();
        std::string error = mapobj::checkFields(orig.kind, f);
        long nx = 0;
        long ny = 0;
        const std::string sx = u32ToUtf8(boxes[0].text);
        const std::string sy = u32ToUtf8(boxes[1].text);
        char* e1 = nullptr;
        char* e2 = nullptr;
        nx = std::strtol(sx.c_str(), &e1, 10);
        ny = std::strtol(sy.c_str(), &e2, 10);
        if (sx.empty() || sy.empty() || *e1 != '\0' || *e2 != '\0') {
            error = "X и Y — целые числа";
        } else if (!m.map.inside(static_cast<int>(nx), static_cast<int>(ny))) {
            error = "Клетка (";
            error += sx;
            error += ", ";
            error += sy;
            error += ") за краем карты ";
            error += std::to_string(m.map.w);
            error += "x";
            error += std::to_string(m.map.h);
        }
        bool changed = false;
        for (std::size_t k = 2; k < fields.size(); ++k)
            changed = changed || u32ToUtf8(boxes[k].text) != fields[k].value;
        MapObject next = orig;
        next.x = static_cast<int>(nx);
        next.y = static_cast<int>(ny);
        // Ничего не правили — хвост строки прежний байт в байт.
        if (changed) next.rest = mapobj::restOf(f);

        fb_.clear(cWindow);
        textCentered(0, 8, WIN_W, 36, "Объект «" + orig.kind + "»", cText);
        uiLabel(0, 36, WIN_W, 20,
                "Поля — как их читает игра. Tab и стрелки — между полями, Enter — готово",
                cTextDim);
        for (std::size_t k = 0; k < fields.size(); ++k) {
            const int y = boxY(k);
            const int w = boxW(k);
            uiLabelLeft(LX, y, BH, fields[k].label, cText);
            fb_.fillRect(BX, y, BX + w, y + BH, k == focus ? cAccent : cBorder);
            fb_.fillRect(BX + 1, y + 1, BX + w - 1, y + BH - 1, active.field);
            TextField& tf = boxes[k];
            const int inner = w - 12;
            const int cx = width(tf, tf.caret);
            if (cx - tf.scroll > inner) tf.scroll = cx - inner;
            if (cx - tf.scroll < 0) tf.scroll = cx;
            fb_.setClip(gfx::Rect{BX + 4, y + 2, BX + w - 4, y + BH - 2});
            const int tx = BX + 6 - tf.scroll;
            const int ty = y + (BH - mono_.textHeight()) / 2;
            if (k == focus && tf.hasSel())
                fb_.fillRect(tx + width(tf, tf.selLo()), ty, tx + width(tf, tf.selHi()),
                             ty + mono_.textHeight(), active.fieldSelection);
            mono_.draw(fb_, tx, ty, u32ToUtf8(tf.text), cText);
            if (k == focus && ((pf_.milliseconds() - blinkStart) / 530) % 2 == 0)
                fb_.fillRect(tx + cx, ty, tx + cx + 1, ty + mono_.textHeight(), cText);
            fb_.resetClip();
            if (!fields[k].hint.empty() && rowH > 46)
                uiLabelLeft(BX + 4, y + BH, 20, fields[k].hint, cTextDim);
            if (orig.kind == "exit" && k == 2 &&
                uiButton(BX + w + 8, y, WIN_W - 40 - (BX + w + 8), BH, "Карта…", true)) {
                // Локация перехода — из папки карт игры.
                const std::string dir = resolvePath(cfg.mapsFolder);
                std::vector<std::string> names;
                if (!dir.empty() && dirExists(dir)) {
                    for (const std::string& f2 : listFiles(dir)) {
                        if (pathExtLower(f2) == ".map") names.push_back(pathStem(f2));
                    }
                }
                std::sort(names.begin(), names.end());
                const int pick = askChoice(
                    "Локация перехода",
                    dir.empty() ? "Папка карт не задана — в настройках" : fitText(dir, WIN_W - 40),
                    names);
                if (pick >= 0) {
                    boxes[2].text = utf8ToU32(names[static_cast<std::size_t>(pick)]);
                    boxes[2].caret = boxes[2].anchor = boxes[2].text.size();
                }
                dropInput();
            }
        }
        const int by = WIN_H - 50;
        uiLabel(0, by - 50, WIN_W, 20, fitText("Строка: " + mapObjectLine(next), WIN_W - 40),
                cTextDim);
        if (!error.empty()) uiLabel(0, by - 28, WIN_W, 20, error, cWarn);
        if (uiButton(40, by, 160, 34, "Готово", error.empty())) ok = true;
        if (uiButton(210, by, 200, 34, "Строка целиком…", true)) {
            raw = true;
            done = true;
        }
        if (uiButton(420, by, 150, 34, "Удалить", true)) {
            removed = true;
            done = true;
        }
        if (uiButton(580, by, 160, 34, "Отмена", true)) done = true;
        if (ok) {
            if (error.empty()) {
                done = true;
                if (!(next == orig)) {
                    m.pushUndo();
                    m.map.objects[static_cast<std::size_t>(idx)] = next;
                    m.overDirty = true;
                }
            } else {
                ok = false;
            }
        }
        redraw();
        idle();
    }
    pf_.stopTextInput();
    waitMouseRelease();
    if (removed) {
        m.pushUndo();
        m.map.deleteObject(idx);
        m.overDirty = true;
    }
    if (raw) editObject(idx);
    m.needRepaint = true;
}

// ---------------------------------------------------------------------------
// Кисть карты: одной местностью или смесью
// ---------------------------------------------------------------------------

void Editor::showMapBrushOptions() {
    bool done = false;
    dropInput();
    pf_.input().keys.clear();
    constexpr int LX = 60;
    while (!done && !quitNow_) {
        beginFrameInput();
        fb_.clear(cWindow);
        textCentered(0, 12, WIN_W, 40, "Кисть карты", cText);
        if (uiToggle(LX, 60, 300, 34, "Одной местностью", !m.mapMix)) m.mapMix = false;
        if (uiToggle(LX + 310, 60, 300, 34, "Смесью местностей", m.mapMix)) {
            m.mapMix = true;
            // Пустая смесь — с местности кисти: дальше добавляют к ней.
            if (std::accumulate(m.mapMixWeights.begin(), m.mapMixWeights.end(), 0) == 0)
                m.mapMixWeights[static_cast<std::size_t>(m.mapBrush)] = 3;
        }
        if (m.mapMix) {
            uiLabelLeft(LX, 104, 22,
                        "Вес каждой местности: сколько её в смеси относительно других.", cTextDim);
            for (int k = 0; k < MT_COUNT; ++k) {
                const int y = 132 + k * 38;
                fb_.fillRect(LX - 30, y + 5, LX - 10, y + 25, mapTileColor(k));
                uiStepper(LX, y, mapTileName(k), m.mapMixWeights[static_cast<std::size_t>(k)], 0,
                          10, 5);
            }
            // Образец: та же раскладка, что даст мазок.
            constexpr int CELL = 12;
            constexpr int OX = 470;
            constexpr int OY = 132;
            for (int y = 0; y < 22; ++y) {
                for (int x = 0; x < 22; ++x)
                    fb_.fillRect(OX + x * CELL, OY + y * CELL, OX + (x + 1) * CELL,
                                 OY + (y + 1) * CELL, mapTileColor(m.mapBrushAt(x, y)));
            }
            fb_.frameRect(OX - 1, OY - 1, OX + 22 * CELL + 1, OY + 22 * CELL + 1, cBorder);
            uiLabelLeft(LX, 404, 22, "Смесь кладут кисть, прямоугольник и заливка карты: каждая",
                        cTextDim);
            uiLabelLeft(LX, 424, 22, "клетка — случайная местность по весам, новый мазок — новая",
                        cTextDim);
            uiLabelLeft(LX, 444, 22, "раскладка. Местность кисти на панели остаётся для пипетки.",
                        cTextDim);
        } else {
            uiLabelLeft(LX, 110, 22, "Кисть, прямоугольник и заливка кладут местность, выбранную",
                        cTextDim);
            uiLabelLeft(LX, 130, 22, "на панели.", cTextDim);
        }
        if (uiButton(WIN_W / 2 - 90, WIN_H - 52, 180, 36, "Закрыть", true)) done = true;
        redraw();
        const int key = takeKey();
        if (key == KEY_ESC || key == KEY_ENTER) done = true;
        idle();
    }
    waitMouseRelease();
    m.needRepaint = true;
}

// ---------------------------------------------------------------------------
// Найти и заменить
// ---------------------------------------------------------------------------

void Editor::showMapFindScreen() {
    if (m.docMode != DOC_MAP) return;
    int from = m.mapBrush;
    int to = MT_FLOOR;
    bool inSel = m.hasSelection;
    std::string query;
    std::vector<int> found = m.findMapObjects(query);
    int page = 0;
    std::string note;
    bool done = false;
    dropInput();
    pf_.input().keys.clear();
    constexpr int LX = 40;
    constexpr int TW = 92;
    constexpr int ROWS = 7;
    while (!done && !quitNow_) {
        beginFrameInput();
        fb_.clear(cWindow);
        textCentered(0, 8, WIN_W, 36, "Найти и заменить", cText);

        // --- местность ---
        uiLabelLeft(LX, 44, 22, "Заменить местность", cTextDim);
        auto terrainRow = [&](int y, const std::string& caption, int& v) {
            uiLabelLeft(LX, y, 30, caption, cText);
            for (int k = 0; k < MT_COUNT; ++k) {
                const int x = LX + 60 + k * (TW + 4);
                if (uiToggle(x, y, TW, 30, mapTileName(k), v == k)) v = k;
                fb_.fillRect(x + 4, y + 24, x + TW - 4, y + 27, mapTileColor(k));
            }
        };
        terrainRow(70, "Что", from);
        terrainRow(108, "На что", to);
        if (uiToggle(LX, 148, 260, 30, "Только в выделении", inSel && m.hasSelection) &&
            m.hasSelection)
            inSel = !inSel;
        int count = 0;
        for (int y = 0; y < m.map.h; ++y) {
            for (int x = 0; x < m.map.w; ++x)
                count +=
                    m.map.at(x, y) == from && (!(inSel && m.hasSelection) || m.isSelected(x, y))
                        ? 1
                        : 0;
        }
        uiLabelLeft(LX + 280, 148, 30, "клеток: " + std::to_string(count), cTextDim);
        if (uiButton(LX + 450, 148, 250, 30, "Заменить", count > 0 && from != to)) {
            const int n = m.replaceMapTerrain(from, to, inSel);
            note = "Заменено клеток: " + std::to_string(n) + ". Отменить — Z.";
        }
        if (!note.empty()) uiLabelLeft(LX, 182, 22, note, cAccent);

        // --- объекты ---
        uiLabelLeft(LX, 214, 22, "Найти объекты", cTextDim);
        if (uiButton(LX, 240, 200, 30, "Искать…", true)) {
            std::string q = query;
            if (askText("Найти объекты", "Вид или часть строки, например chest или rat", q, q)) {
                query = netTrim(q);
                found = m.findMapObjects(query);
                page = 0;
            }
            dropInput();
        }
        uiLabelLeft(LX + 214, 240, 30,
                    (query.empty() ? std::string("все объекты") : "«" + query + "»") + ": " +
                        std::to_string(found.size()),
                    cText);
        const int pages = std::max(1, (static_cast<int>(found.size()) + ROWS - 1) / ROWS);
        page = std::clamp(page, 0, pages - 1);
        int jump = -1;
        for (int r = 0; r < ROWS; ++r) {
            const int i = page * ROWS + r;
            if (i >= static_cast<int>(found.size())) break;
            const MapObject& o =
                m.map.objects[static_cast<std::size_t>(found[static_cast<std::size_t>(i)])];
            if (uiButton(LX, 278 + r * 31, WIN_W - 2 * LX, 28,
                         fitText(mapObjectLine(o), WIN_W - 2 * LX - 16), true))
                jump = found[static_cast<std::size_t>(i)];
        }
        if (uiButton(LX, WIN_H - 50, 110, 34, "Назад", page > 0)) --page;
        uiLabel(LX + 116, WIN_H - 50, 200, 34,
                "страница " + std::to_string(page + 1) + " из " + std::to_string(pages), cTextDim);
        if (uiButton(LX + 322, WIN_H - 50, 110, 34, "Дальше", page < pages - 1)) ++page;
        if (uiButton(WIN_W - LX - 180, WIN_H - 50, 180, 34, "Закрыть", true)) done = true;
        if (jump >= 0) {
            // К объекту: он посреди холста и выделен своей клеткой.
            const MapObject& o = m.map.objects[static_cast<std::size_t>(jump)];
            m.resetSelection();
            m.selX = o.x;
            m.selY = o.y;
            m.selW = 1;
            m.selH = 1;
            m.hasSelection = true;
            m.viewX = pround((m.X0 + m.W) / 2.0 - (o.x + 0.5) * m.pixelSize);
            m.viewY = pround((m.Y0 + m.H) / 2.0 - (o.y + 0.5) * m.pixelSize);
            m.clampView();
            done = true;
        }
        redraw();
        const int key = takeKey();
        if (key == KEY_ESC) done = true;
        idle();
    }
    waitMouseRelease();
    m.overDirty = true;
    m.needRepaint = true;
}

}  // namespace tjtm
