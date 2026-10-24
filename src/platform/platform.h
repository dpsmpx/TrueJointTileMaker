// Окно, ввод, время, диалоги и буфер обмена на SDL3.
//
// Оригинал получал события в потоке окна WinForms и складывал их в общие
// переменные под замком; основной цикл забирал их снимком в начале кадра.
// SDL3 отдаёт события в основной поток, поэтому замок не нужен, но вид
// данных тот же: где мышь, нажата ли кнопка, накопленный щелчок с
// координатами и кнопкой, удерживаемые клавиши, очередь нажатий, отсчёты
// ролика. Код редактора читает их так же, как читал оригинал.
//
// Клавиши — коды Windows (VK_*), которыми пользовался оригинал: буквы по
// физическому положению, так что раскладка их не меняет.
#pragma once

#include <array>
#include <cstdint>
#include <deque>
#include <string>
#include <vector>

#include "gfx/surface.h"
#include "platform/script.h"

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;
union SDL_Event;

namespace tjtm {

/// Коды клавиш оригинала.
enum : int {
    KEY_BACK = 8,
    KEY_TAB = 9,
    KEY_ENTER = 13,
    KEY_SHIFT = 16,
    KEY_CTRL = 17,
    KEY_ALT = 18,
    KEY_ESC = 27,
    KEY_SPACE = 32,
    KEY_PGUP = 33,
    KEY_PGDN = 34,
    KEY_END = 35,
    KEY_HOME = 36,
    KEY_LEFT = 37,
    KEY_UP = 38,
    KEY_RIGHT = 39,
    KEY_DOWN = 40,
    KEY_INSERT = 45,
    KEY_DELETE = 46,
    KEY_0 = 48,
    KEY_1 = 49,
    KEY_2 = 50,
    KEY_3 = 51,
    KEY_4 = 52,
    KEY_5 = 53,
    KEY_6 = 54,
    KEY_7 = 55,
    KEY_8 = 56,
    KEY_9 = 57,
    KEY_A = 65,
    KEY_B,
    KEY_C,
    KEY_D,
    KEY_E,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_I,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_M,
    KEY_N,
    KEY_O,
    KEY_P,
    KEY_Q,
    KEY_R,
    KEY_S,
    KEY_T,
    KEY_U,
    KEY_V,
    KEY_W,
    KEY_X,
    KEY_Y,
    KEY_Z,
    KEY_NUM0 = 96,
    KEY_NUMPLUS = 107,
    KEY_NUMMINUS = 109,
    KEY_F1 = 112,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_F11,
    KEY_F12,
    KEY_SEMICOLON = 186,
    KEY_PLUS = 187,
    KEY_COMMA = 188,
    KEY_MINUS = 189,
    KEY_PERIOD = 190,
    KEY_SLASH = 191,
    KEY_GRAVE = 192,
    KEY_LBRACKET = 219,
    KEY_BACKSLASH = 220,
    KEY_RBRACKET = 221,
    KEY_QUOTE = 222,
    KEY_CODE_MAX = 255,
};

/// Кнопки мыши оригинала: левая 1, правая 2, всё прочее — «средняя».
inline constexpr int MB_LEFT = 1;
inline constexpr int MB_RIGHT = 2;
inline constexpr int MB_MIDDLE = 3;

/// Нажатие для поля ввода: ему нужны и сочетания, и автоповтор.
struct KeyPress {
    int vk = 0;
    bool shift = false;
    bool ctrl = false;
    bool alt = false;
};

struct Input {
    int mouseX = 0, mouseY = 0;
    bool mousePressed = false;
    /// Накопленный щелчок: снимается в начале кадра (beginFrame).
    bool clickPending = false;
    int clickX = 0, clickY = 0, clickButton = 0;
    std::array<bool, KEY_CODE_MAX + 1> keyHeld{};
    /// Очередь нажатий. У оригинала было одно место: второе нажатие за кадр
    /// затирало первое; очередь не теряет ни одного. Модификаторы — какими
    /// были в миг нажатия: разбирают клавишу кадром позже, когда Ctrl уже
    /// могли отпустить.
    std::deque<KeyPress> keys;
    /// Нажатия для поля ввода: оно разбирает их само, со своим темпом.
    std::deque<KeyPress> presses;
    std::string text;
    int wheel = 0;
    bool quitRequested = false;
};

/// Фильтр диалога файлов: подпись и расширения через «;» без точек.
struct FileFilter {
    std::string name;
    std::string pattern;
};

class Platform {
public:
    Platform() = default;
    ~Platform();
    Platform(const Platform&) = delete;
    Platform& operator=(const Platform&) = delete;

    /// Окно логического размера w x h. false и текст ошибки при неудаче.
    bool init(const std::string& title, int w, int h, std::string& error);
    void shutdown();

    void setTitle(const std::string& title);
    /// Вывести кадр в окно (Redraw оригинала).
    void present(const gfx::Surface& frame);

    /// Разобрать накопившиеся события в Input.
    void pump();
    /// Короткая пауза с разбором событий — Idle оригинала: циклы ожидания
    /// не занимают процессор вхолостую.
    void idle();

    Input& input() { return in_; }
    [[nodiscard]] bool keyHeld(int vk) const {
        return vk >= 0 && vk <= KEY_CODE_MAX && in_.keyHeld[static_cast<std::size_t>(vk)];
    }
    /// Миллисекунды с запуска программы (Milliseconds оригинала).
    [[nodiscard]] int milliseconds() const;

    /// Каталог программы (DetectBaseDir оригинала).
    [[nodiscard]] std::string baseDir() const;

    /// Диалоги файлов. Пустая строка — отмена. Пока диалог открыт, события
    /// окна продолжают разбираться.
    std::string openFileDialog(const std::string& title, const std::vector<FileFilter>& filters,
                               const std::string& folder);
    std::string saveFileDialog(const std::string& title, const std::vector<FileFilter>& filters,
                               const std::string& folder, const std::string& suggested);

    std::string clipboardText() const;
    void setClipboardText(const std::string& s);

    void startTextInput(int x, int y, int w, int h);
    void stopTextInput();

    [[nodiscard]] SDL_Window* window() const { return window_; }

    /// Вести программу по сценарию (script.h); снимки — в shotDir.
    bool runScript(const std::string& path, const std::string& shotDir, std::string& error);
    /// Сценарий был и весь пройден: программе пора закрываться.
    [[nodiscard]] bool scriptFinished() const { return scripted_ && scriptDone_; }

private:
    /// convert — пересчитать координаты окна в координаты кадра. События
    /// сценария уже в координатах кадра.
    void handle(const SDL_Event& e, bool convert = true);
    void scriptStep();
    std::string runDialog(int kind, const std::string& title,
                          const std::vector<FileFilter>& filters, const std::string& location);
    /// Ответ диалога из файла TJTM_TEST_DIALOGS — для сценарных тестов.
    bool scriptedDialog(std::string& answer);

    SDL_Window* window_ = nullptr;
    SDL_Renderer* renderer_ = nullptr;
    SDL_Texture* texture_ = nullptr;
    int w_ = 0;
    int h_ = 0;
    Input in_;
    bool textInputOn_ = false;
    /// Нажатые кнопки мыши битами: «нажата» — пока нажата хоть одна.
    unsigned buttons_ = 0;

    Script script_;
    bool scripted_ = false;
    bool scriptDone_ = false;
    int scriptTicks_ = 0;
    std::string shotDir_;
    gfx::Surface shown_;
};

/// Код клавиши оригинала по коду клавиши SDL; 0 — клавиша не нужна.
int vkFromScancode(int scancode);

}  // namespace tjtm
