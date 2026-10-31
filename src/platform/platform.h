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

#include "core/keys.h"
#include "gfx/surface.h"
#include "platform/script.h"

struct SDL_Window;
struct SDL_Renderer;
struct SDL_Texture;
struct SDL_Cursor;
union SDL_Event;

namespace tjtm {

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
    /// Автоповтор зажатой клавиши: холст стрелками едет плавно сам, и
    /// повторы ему только мешают.
    bool repeat = false;
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
    /// Файлы, брошенные в окно.
    std::vector<std::string> drops;
    /// Целые шаги прокрутки вбок: +1 вправо.
    int wheelX = 0;
    /// Копить дробные доли щелчка (UX-12). У оригинала каждое событие
    /// ролика было целым щелчком, а трекпад шлёт много мелких: лёгкое
    /// движение двумя пальцами удваивало масштаб несколько раз подряд.
    bool fineWheel = false;
    float wheelAccY = 0, wheelAccX = 0;
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
    /// Выбор папки системным диалогом; folder — с какой начать.
    std::string openFolderDialog(const std::string& title, const std::string& folder);

    /// Картинка в системный буфер обмена — PNG (UX-14); и обратно: пусто,
    /// если там не картинка.
    void setClipboardImage(std::vector<std::uint8_t> png);
    [[nodiscard]] std::vector<std::uint8_t> clipboardImage() const;
    /// PNG, который положили мы: своё выделение вставляется из внутреннего
    /// буфера, с номерами палитры.
    [[nodiscard]] const std::vector<std::uint8_t>& ownClipboardImage() const { return clipPng_; }
    std::string clipboardText() const;
    void setClipboardText(const std::string& s);

    /// Курсор мыши: обычная стрелка, перекрестье рисования, перенос (рука
    /// и перемещение выделения), запрет.
    enum Cursor { CURSOR_ARROW, CURSOR_CROSSHAIR, CURSOR_MOVE, CURSOR_NOT_ALLOWED, CURSOR_COUNT };
    void setCursor(Cursor c);

    void startTextInput(int x, int y, int w, int h);
    void stopTextInput();

    [[nodiscard]] SDL_Window* window() const { return window_; }

    /// Вести программу по сценарию (script.h); снимки — в shotDir.
    bool runScript(const std::string& path, const std::string& shotDir, std::string& error);
    /// Сценарий был и весь пройден: программе пора закрываться.
    [[nodiscard]] bool scriptFinished() const { return scripted_ && scriptDone_; }
    [[nodiscard]] bool scripted() const { return scripted_; }
    /// Папка настроек пользователя (SDL_GetPrefPath); пусто — нет такой.
    [[nodiscard]] static std::string prefDir();

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
    std::array<SDL_Cursor*, CURSOR_COUNT> cursors_{};
    std::vector<std::uint8_t> clipPng_;
    Cursor cursor_ = CURSOR_ARROW;
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
