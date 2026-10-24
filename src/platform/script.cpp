#include "platform/script.h"

#include <SDL3/SDL.h>

#include <cmath>
#include <sstream>

#include "core/fsutil.h"

namespace tjtm {

namespace {

constexpr double kTickSeconds = 0.016;
// После каждой клавиши и щелчка tools/gui.sh ждёт 0,15 с, внутри протяжки —
// 0,03 с на шаг. Те же паузы здесь: сценарий проходит одинаково и там,
// и тут, в том числе всё, что зависит от времени между щелчками.
constexpr double kAfterInput = 0.15;
constexpr double kDragStep = 0.03;
constexpr int kDragSteps = 12;

/// Имена xdotool, которых нет среди имён SDL.
const char* sdlKeyName(const std::string& n) {
    static const struct {
        const char* xdo;
        const char* sdl;
    } kAliases[] = {
        {"BackSpace", "Backspace"},
        {"Prior", "PageUp"},
        {"Next", "PageDown"},
        {"minus", "-"},
        {"equal", "="},
        {"plus", "="},
        {"comma", ","},
        {"period", "."},
        {"slash", "/"},
        {"semicolon", ";"},
        {"apostrophe", "'"},
        {"grave", "`"},
        {"bracketleft", "["},
        {"bracketright", "]"},
        {"backslash", "\\"},
        {"KP_Add", "Keypad +"},
        {"KP_Subtract", "Keypad -"},
        {"KP_Enter", "Keypad Enter"},
    };
    for (const auto& a : kAliases) {
        if (n == a.xdo) return a.sdl;
    }
    return nullptr;
}

}  // namespace

bool scriptKey(const std::string& name, int& scancode, int& mod) {
    unsigned m = SDL_KMOD_NONE;
    std::string rest = name;
    // Модификаторы — через «+», как у xdotool: ctrl+shift+z.
    while (true) {
        const std::size_t p = rest.find('+');
        if (p == std::string::npos || p + 1 >= rest.size()) break;
        const std::string head = rest.substr(0, p);
        if (head == "ctrl" || head == "Control_L") {
            m |= SDL_KMOD_LCTRL;
        } else if (head == "shift" || head == "Shift_L") {
            m |= SDL_KMOD_LSHIFT;
        } else if (head == "alt" || head == "Alt_L") {
            m |= SDL_KMOD_LALT;
        } else {
            return false;
        }
        rest = rest.substr(p + 1);
    }
    if (rest == "plus") m |= SDL_KMOD_LSHIFT;
    const char* alias = sdlKeyName(rest);
    const SDL_Scancode sc = SDL_GetScancodeFromName(alias != nullptr ? alias : rest.c_str());
    if (sc == SDL_SCANCODE_UNKNOWN) return false;
    scancode = static_cast<int>(sc);
    mod = static_cast<int>(m);
    return true;
}

bool Script::load(const std::string& path, std::string& error) {
    const auto text = readFile(path);
    if (!text) {
        error = "не открыть " + path;
        return false;
    }
    return parse(*text, error);
}

void Script::pause(double seconds) {
    const auto n = static_cast<int>(std::lround(seconds / kTickSeconds));
    for (int i = 0; i < n; ++i)
        ticks_.emplace_back();
}

void Script::push(ScriptEvent e) {
    if (e.kind == ScriptEvent::MOTION || e.kind == ScriptEvent::BUTTON_DOWN ||
        e.kind == ScriptEvent::BUTTON_UP) {
        mouseX_ = e.x;
        mouseY_ = e.y;
    } else {
        e.x = mouseX_;
        e.y = mouseY_;
    }
    ScriptTick t;
    t.events.push_back(std::move(e));
    ticks_.push_back(std::move(t));
}

bool Script::key(const std::string& name, std::string& error) {
    ScriptEvent e;
    if (!scriptKey(name, e.scancode, e.mod)) {
        error = "неизвестная клавиша «" + name + "»";
        return false;
    }
    e.kind = ScriptEvent::KEY_DOWN;
    push(e);
    e.kind = ScriptEvent::KEY_UP;
    push(e);
    pause(kAfterInput);
    return true;
}

bool Script::parse(const std::string& text, std::string& error) {
    std::istringstream in(text);
    std::string line;
    int lineNo = 0;
    while (std::getline(in, line)) {
        ++lineNo;
        const std::size_t hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);
        std::istringstream ls(line);
        std::string cmd;
        if (!(ls >> cmd)) continue;
        std::vector<std::string> a;
        for (std::string w; ls >> w;)
            a.push_back(w);
        auto num = [&](std::size_t i, int def) {
            return i < a.size() ? std::atoi(a[i].c_str()) : def;
        };
        auto fail = [&](const std::string& what) {
            error = "строка " + std::to_string(lineNo) + ": " + what;
            return false;
        };

        if (cmd == "sleep") {
            if (a.empty()) return fail("sleep без времени");
            pause(std::atof(a[0].c_str()));
        } else if (cmd == "shot") {
            if (a.empty()) return fail("shot без имени");
            ScriptTick t;
            t.shot = a[0];
            ticks_.push_back(std::move(t));
        } else if (cmd == "key") {
            for (const std::string& k : a) {
                if (!key(k, error)) return fail(error);
            }
        } else if (cmd == "move") {
            if (a.size() < 2) return fail("move X Y");
            ScriptEvent e;
            e.kind = ScriptEvent::MOTION;
            e.x = num(0, 0);
            e.y = num(1, 0);
            push(e);
            pause(kAfterInput);
        } else if (cmd == "click") {
            if (a.size() < 2) return fail("click X Y [КНОПКА]");
            const int btn = num(2, 1);
            ScriptEvent e;
            e.kind = ScriptEvent::MOTION;
            e.x = num(0, 0);
            e.y = num(1, 0);
            push(e);
            pause(0.05);
            if (btn == 4 || btn == 5) {
                // Ролик у xdotool — кнопки 4 и 5.
                e.kind = ScriptEvent::WHEEL;
                e.wheel = btn == 4 ? 1 : -1;
                push(e);
            } else {
                e.button = btn;
                e.kind = ScriptEvent::BUTTON_DOWN;
                push(e);
                e.kind = ScriptEvent::BUTTON_UP;
                push(e);
            }
            pause(kAfterInput);
        } else if (cmd == "drag") {
            if (a.size() < 4) return fail("drag X0 Y0 X1 Y1 [КНОПКА]");
            const int x0 = num(0, 0);
            const int y0 = num(1, 0);
            const int x1 = num(2, 0);
            const int y1 = num(3, 0);
            ScriptEvent e;
            e.button = num(4, 1);
            e.kind = ScriptEvent::MOTION;
            e.x = x0;
            e.y = y0;
            push(e);
            pause(0.05);
            e.kind = ScriptEvent::BUTTON_DOWN;
            push(e);
            for (int i = 1; i <= kDragSteps; ++i) {
                e.kind = ScriptEvent::MOTION;
                e.x = x0 + (x1 - x0) * i / kDragSteps;
                e.y = y0 + (y1 - y0) * i / kDragSteps;
                push(e);
                pause(kDragStep);
            }
            e.kind = ScriptEvent::BUTTON_UP;
            push(e);
            pause(kAfterInput);
        } else if (cmd == "type") {
            const std::size_t p = line.find("type");
            std::string t = line.substr(p + 4);
            while (!t.empty() && t.front() == ' ')
                t.erase(0, 1);
            ScriptEvent e;
            e.kind = ScriptEvent::TEXT;
            e.text = t;
            push(e);
            pause(kAfterInput);
        } else {
            return fail("неизвестная команда «" + cmd + "»");
        }
    }
    return true;
}

ScriptTick Script::next() {
    if (ticks_.empty()) return {};
    ScriptTick t = std::move(ticks_.front());
    ticks_.pop_front();
    return t;
}

}  // namespace tjtm
