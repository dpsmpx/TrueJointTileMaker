// Сценарий ввода — те же файлы tests/screens/*.txt, что гоняет tools/gui.sh
// через xdotool, только события подаются изнутри программы, без X-сервера.
// Так один сценарий служит и сверке с оригиналом на виртуальном экране, и
// тестам переноса в ctest, где экрана нет вовсе.
//
// Сценарий разворачивается в «тики»: за один разбор событий (Platform::pump)
// уходит один тик. Паузы считаются в тиках по 16 мс, и часы программы в
// этом режиме тоже идут по тикам — бег «муравьёв» и двойной щелчок от
// скорости машины не зависят.
//
// Сверх команд tools/gui.sh здесь есть «wheel DY [DX]» — прокрутка долями
// щелчка, как у трекпада, и «drop ПУТЬ» — файл, брошенный в окно (у xdotool
// их нет: сценарии с ними — только для переноса).
#pragma once

#include <deque>
#include <string>
#include <vector>

namespace tjtm {

/// Событие сценария в координатах окна оригинала.
struct ScriptEvent {
    enum Kind { MOTION, BUTTON_DOWN, BUTTON_UP, WHEEL, KEY_DOWN, KEY_UP, TEXT, DROP };
    Kind kind = MOTION;
    int x = 0, y = 0;
    int button = 0;  // 1 левая, 2 средняя, 3 правая — как у xdotool и SDL
    /// Прокрутка: y +1 от себя, −1 на себя; x +1 вправо. Дробная — как у
    /// трекпада.
    float wheelY = 0, wheelX = 0;
    int scancode = 0;  // SDL_Scancode
    int mod = 0;       // SDL_Keymod
    std::string text;
};

struct ScriptTick {
    std::vector<ScriptEvent> events;
    std::string shot;  // снять кадр под этим именем
};

class Script {
public:
    /// Прочитать и развернуть файл. false — ошибка с номером строки.
    bool load(const std::string& path, std::string& error);
    /// Развернуть текст сценария (для тестов самого разбора).
    bool parse(const std::string& text, std::string& error);

    [[nodiscard]] bool empty() const { return ticks_.empty(); }
    /// Следующий тик; сценарий укорачивается на него.
    ScriptTick next();

private:
    std::deque<ScriptTick> ticks_;
    int mouseX_ = 0, mouseY_ = 0;

    void pause(double seconds);
    void push(ScriptEvent e);
    bool key(const std::string& name, std::string& error);
    bool hold(const std::string& name, bool down, std::string& error);
};

/// Код клавиши SDL по имени xdotool (Escape, Return, g, ctrl+z) и
/// модификаторы. false — имя не распознано.
bool scriptKey(const std::string& name, int& scancode, int& mod);

}  // namespace tjtm
