#include "platform/platform.h"

#include <SDL3/SDL.h>

#include <atomic>
#include <cstdlib>
#include <mutex>

#include "core/fsutil.h"
#include "core/png.h"
#include "core/text.h"

namespace tjtm {

int vkFromScancode(int sc) {
    if (sc >= SDL_SCANCODE_A && sc <= SDL_SCANCODE_Z) return KEY_A + (sc - SDL_SCANCODE_A);
    if (sc >= SDL_SCANCODE_1 && sc <= SDL_SCANCODE_9) return KEY_1 + (sc - SDL_SCANCODE_1);
    if (sc >= SDL_SCANCODE_F1 && sc <= SDL_SCANCODE_F12) return KEY_F1 + (sc - SDL_SCANCODE_F1);
    if (sc >= SDL_SCANCODE_KP_1 && sc <= SDL_SCANCODE_KP_9)
        return KEY_NUM0 + 1 + (sc - SDL_SCANCODE_KP_1);
    switch (sc) {
        case SDL_SCANCODE_0: return KEY_0;
        case SDL_SCANCODE_KP_0: return KEY_NUM0;
        case SDL_SCANCODE_ESCAPE: return KEY_ESC;
        case SDL_SCANCODE_TAB: return KEY_TAB;
        case SDL_SCANCODE_RETURN:
        case SDL_SCANCODE_KP_ENTER: return KEY_ENTER;
        case SDL_SCANCODE_BACKSPACE: return KEY_BACK;
        case SDL_SCANCODE_SPACE: return KEY_SPACE;
        case SDL_SCANCODE_LEFT: return KEY_LEFT;
        case SDL_SCANCODE_UP: return KEY_UP;
        case SDL_SCANCODE_RIGHT: return KEY_RIGHT;
        case SDL_SCANCODE_DOWN: return KEY_DOWN;
        case SDL_SCANCODE_DELETE: return KEY_DELETE;
        case SDL_SCANCODE_INSERT: return KEY_INSERT;
        case SDL_SCANCODE_HOME: return KEY_HOME;
        case SDL_SCANCODE_END: return KEY_END;
        case SDL_SCANCODE_PAGEUP: return KEY_PGUP;
        case SDL_SCANCODE_PAGEDOWN: return KEY_PGDN;
        case SDL_SCANCODE_LSHIFT:
        case SDL_SCANCODE_RSHIFT: return KEY_SHIFT;
        case SDL_SCANCODE_LCTRL:
        case SDL_SCANCODE_RCTRL: return KEY_CTRL;
        case SDL_SCANCODE_LALT:
        case SDL_SCANCODE_RALT: return KEY_ALT;
        case SDL_SCANCODE_EQUALS: return KEY_PLUS;
        case SDL_SCANCODE_MINUS: return KEY_MINUS;
        case SDL_SCANCODE_KP_PLUS: return KEY_NUMPLUS;
        case SDL_SCANCODE_KP_MINUS: return KEY_NUMMINUS;
        case SDL_SCANCODE_COMMA: return KEY_COMMA;
        case SDL_SCANCODE_PERIOD: return KEY_PERIOD;
        case SDL_SCANCODE_SLASH: return KEY_SLASH;
        case SDL_SCANCODE_SEMICOLON: return KEY_SEMICOLON;
        case SDL_SCANCODE_APOSTROPHE: return KEY_QUOTE;
        case SDL_SCANCODE_LEFTBRACKET: return KEY_LBRACKET;
        case SDL_SCANCODE_RIGHTBRACKET: return KEY_RBRACKET;
        case SDL_SCANCODE_BACKSLASH: return KEY_BACKSLASH;
        case SDL_SCANCODE_GRAVE: return KEY_GRAVE;
        default: return 0;
    }
}

Platform::~Platform() {
    shutdown();
}

bool Platform::init(const std::string& title, int w, int h, std::string& error) {
    // Первый щелчок по неактивному окну — уже щелчок, как в WinForms, а не
    // только переключение фокуса.
    SDL_SetHint(SDL_HINT_MOUSE_FOCUS_CLICKTHROUGH, "1");
    if (!SDL_Init(SDL_INIT_VIDEO)) {
        error = SDL_GetError();
        return false;
    }
    w_ = w;
    h_ = h;
    window_ = SDL_CreateWindow(title.c_str(), w, h, 0);
    if (window_ == nullptr) {
        error = SDL_GetError();
        return false;
    }
    // Без экрана (драйвер offscreen, тесты) GPU не нужен: SDL всё равно
    // поднял бы EGL и Mesa, а их собственные утечки LeakSanitizer приписал бы
    // программе.
    const char* driver = SDL_GetCurrentVideoDriver();
    const bool headless = driver != nullptr && SDL_strcmp(driver, "offscreen") == 0;
    renderer_ = SDL_CreateRenderer(window_, headless ? SDL_SOFTWARE_RENDERER : nullptr);
    if (renderer_ == nullptr) {
        error = SDL_GetError();
        return false;
    }
    // Логический размер — размер окна оригинала; на экране с другой
    // плотностью кадр растягивается ближайшим соседом, без мыла.
    SDL_SetRenderLogicalPresentation(renderer_, w, h, SDL_LOGICAL_PRESENTATION_LETTERBOX);
    texture_ =
        SDL_CreateTexture(renderer_, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, w, h);
    if (texture_ == nullptr) {
        error = SDL_GetError();
        return false;
    }
    SDL_SetTextureScaleMode(texture_, SDL_SCALEMODE_NEAREST);
    float mx = 0;
    float my = 0;
    SDL_GetMouseState(&mx, &my);
    return true;
}

void Platform::shutdown() {
    if (texture_ != nullptr) SDL_DestroyTexture(texture_);
    if (renderer_ != nullptr) SDL_DestroyRenderer(renderer_);
    if (window_ != nullptr) SDL_DestroyWindow(window_);
    texture_ = nullptr;
    renderer_ = nullptr;
    if (window_ != nullptr || SDL_WasInit(SDL_INIT_VIDEO) != 0) SDL_Quit();
    window_ = nullptr;
}

void Platform::setTitle(const std::string& title) {
    if (window_ != nullptr) SDL_SetWindowTitle(window_, title.c_str());
}

void Platform::present(const gfx::Surface& frame) {
    // Снимок сценария — то, что показано, а не кадр, который, может быть,
    // как раз перерисовывается.
    if (scripted_) shown_ = frame;
    if (renderer_ == nullptr || texture_ == nullptr) return;
    SDL_UpdateTexture(texture_, nullptr, frame.data(), frame.width() * 4);
    SDL_SetRenderDrawColor(renderer_, 0, 0, 0, 255);
    SDL_RenderClear(renderer_);
    SDL_RenderTexture(renderer_, texture_, nullptr, nullptr);
    SDL_RenderPresent(renderer_);
}

void Platform::handle(const SDL_Event& e0, bool convert) {
    SDL_Event e = e0;
    if (convert && renderer_ != nullptr) SDL_ConvertEventToRenderCoordinates(renderer_, &e);
    switch (e.type) {
        case SDL_EVENT_MOUSE_MOTION:
            in_.mouseX = static_cast<int>(e.motion.x);
            in_.mouseY = static_cast<int>(e.motion.y);
            break;
        case SDL_EVENT_MOUSE_BUTTON_DOWN:
            in_.mouseX = static_cast<int>(e.button.x);
            in_.mouseY = static_cast<int>(e.button.y);
            buttons_ |= 1U << e.button.button;
            in_.mousePressed = true;
            in_.clickX = in_.mouseX;
            in_.clickY = in_.mouseY;
            in_.clickButton = e.button.button == SDL_BUTTON_LEFT    ? MB_LEFT
                              : e.button.button == SDL_BUTTON_RIGHT ? MB_RIGHT
                                                                    : MB_MIDDLE;
            in_.clickPending = true;
            break;
        case SDL_EVENT_MOUSE_BUTTON_UP:
            in_.mouseX = static_cast<int>(e.button.x);
            in_.mouseY = static_cast<int>(e.button.y);
            // Кнопка считается отпущенной, когда отпущены все: протяжка
            // одной кнопкой не обрывается щелчком другой.
            buttons_ &= ~(1U << e.button.button);
            in_.mousePressed = buttons_ != 0;
            break;
        case SDL_EVENT_MOUSE_WHEEL:
            if (e.wheel.y > 0) ++in_.wheel;
            if (e.wheel.y < 0) --in_.wheel;
            break;
        case SDL_EVENT_KEY_DOWN: {
            const int vk = vkFromScancode(e.key.scancode);
            if (vk == 0) break;
            in_.keyHeld[static_cast<std::size_t>(vk)] = true;
            const SDL_Keymod mod = e.key.mod;
            const KeyPress kp{vk, (mod & SDL_KMOD_SHIFT) != 0, (mod & SDL_KMOD_CTRL) != 0,
                              (mod & SDL_KMOD_ALT) != 0};
            in_.keys.push_back(kp);
            in_.presses.push_back(kp);
            // Очередь ограничена: если программа долго не разбирала ввод,
            // старые нажатия ей уже не нужны.
            while (in_.keys.size() > 64)
                in_.keys.pop_front();
            while (in_.presses.size() > 64)
                in_.presses.pop_front();
            break;
        }
        case SDL_EVENT_KEY_UP: {
            const int vk = vkFromScancode(e.key.scancode);
            if (vk != 0) in_.keyHeld[static_cast<std::size_t>(vk)] = false;
            break;
        }
        case SDL_EVENT_TEXT_INPUT:
            if (textInputOn_ && e.text.text != nullptr) in_.text += e.text.text;
            break;
        case SDL_EVENT_WINDOW_FOCUS_LOST:
            // Отпускание после ухода фокуса придёт уже не нам: клавиши
            // осталась бы «нажатыми» навсегда.
            in_.keyHeld.fill(false);
            break;
        case SDL_EVENT_QUIT:
        case SDL_EVENT_WINDOW_CLOSE_REQUESTED: in_.quitRequested = true; break;
        default: break;
    }
}

void Platform::pump() {
    if (scripted_) scriptStep();
    SDL_Event e;
    while (SDL_PollEvent(&e))
        handle(e);
}

void Platform::idle() {
    pump();
    // По сценарию время виртуальное, ждать нечего.
    if (!scripted_) SDL_Delay(4);
}

int Platform::milliseconds() const {
    return scripted_ ? scriptTicks_ * 16 : static_cast<int>(SDL_GetTicks());
}

bool Platform::runScript(const std::string& path, const std::string& shotDir, std::string& error) {
    if (!script_.load(path, error)) return false;
    scripted_ = true;
    scriptDone_ = false;
    shotDir_ = shotDir.empty() ? std::string(".") : shotDir;
    return true;
}

void Platform::scriptStep() {
    ++scriptTicks_;
    if (script_.empty()) {
        scriptDone_ = true;
        return;
    }
    const ScriptTick t = script_.next();
    for (const ScriptEvent& se : t.events) {
        SDL_Event e{};
        switch (se.kind) {
            case ScriptEvent::MOTION:
                e.type = SDL_EVENT_MOUSE_MOTION;
                e.motion.x = static_cast<float>(se.x);
                e.motion.y = static_cast<float>(se.y);
                break;
            case ScriptEvent::BUTTON_DOWN:
            case ScriptEvent::BUTTON_UP:
                e.type = se.kind == ScriptEvent::BUTTON_DOWN ? SDL_EVENT_MOUSE_BUTTON_DOWN
                                                             : SDL_EVENT_MOUSE_BUTTON_UP;
                e.button.button = static_cast<Uint8>(se.button);
                e.button.down = se.kind == ScriptEvent::BUTTON_DOWN;
                e.button.clicks = 1;
                e.button.x = static_cast<float>(se.x);
                e.button.y = static_cast<float>(se.y);
                break;
            case ScriptEvent::WHEEL:
                e.type = SDL_EVENT_MOUSE_WHEEL;
                e.wheel.y = static_cast<float>(se.wheel);
                e.wheel.mouse_x = static_cast<float>(se.x);
                e.wheel.mouse_y = static_cast<float>(se.y);
                break;
            case ScriptEvent::KEY_DOWN:
            case ScriptEvent::KEY_UP:
                e.type = se.kind == ScriptEvent::KEY_DOWN ? SDL_EVENT_KEY_DOWN : SDL_EVENT_KEY_UP;
                e.key.scancode = static_cast<SDL_Scancode>(se.scancode);
                e.key.mod = static_cast<SDL_Keymod>(se.mod);
                e.key.key = SDL_GetKeyFromScancode(e.key.scancode, e.key.mod, false);
                e.key.down = se.kind == ScriptEvent::KEY_DOWN;
                break;
            case ScriptEvent::TEXT:
                e.type = SDL_EVENT_TEXT_INPUT;
                e.text.text = se.text.c_str();
                break;
        }
        handle(e, false);
    }
    if (!t.shot.empty() && shown_.width() > 0) {
        std::string err;
        if (!pngSave(pathCombine(shotDir_, t.shot + ".png"), shown_.toGrid(), err))
            SDL_Log("снимок %s: %s", t.shot.c_str(), err.c_str());
    }
}

std::string Platform::baseDir() const {
    const char* p = SDL_GetBasePath();
    std::string s = p != nullptr ? p : "";
    while (!s.empty() && (s.back() == '/' || s.back() == '\\'))
        s.pop_back();
    return s;
}

namespace {

struct DialogState {
    std::mutex m;
    std::atomic<bool> done{false};
    std::string result;
};

void SDLCALL dialogCallback(void* userdata, const char* const* filelist, int /*filter*/) {
    auto* st = static_cast<DialogState*>(userdata);
    {
        const std::lock_guard<std::mutex> lock(st->m);
        st->result = (filelist != nullptr && filelist[0] != nullptr) ? filelist[0] : "";
    }
    st->done = true;
}

}  // namespace

bool Platform::scriptedDialog(std::string& answer) {
    const char* script = std::getenv("TJTM_TEST_DIALOGS");
    if (script == nullptr || *script == '\0') return false;
    // Первая строка файла — ответ этому диалогу; она снимается, остальные
    // ждут следующих. Пустая строка — отмена.
    const auto data = readFile(script);
    std::string rest = data.value_or("");
    const std::size_t nl = rest.find('\n');
    answer = rest.substr(0, nl);
    if (!answer.empty() && answer.back() == '\r') answer.pop_back();
    rest = nl == std::string::npos ? std::string() : rest.substr(nl + 1);
    writeFile(script, rest);
    return true;
}

std::string Platform::runDialog(int kind, const std::string& title,
                                const std::vector<FileFilter>& filters,
                                const std::string& location) {
    std::string scripted;
    if (scriptedDialog(scripted)) return scripted;

    std::vector<SDL_DialogFileFilter> f;
    f.reserve(filters.size());
    for (const auto& x : filters)
        f.push_back(SDL_DialogFileFilter{x.name.c_str(), x.pattern.c_str()});
    DialogState st;
    const SDL_PropertiesID props = SDL_CreateProperties();
    if (!f.empty()) {
        SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_FILTERS_POINTER, f.data());
        SDL_SetNumberProperty(props, SDL_PROP_FILE_DIALOG_NFILTERS_NUMBER,
                              static_cast<Sint64>(f.size()));
    }
    SDL_SetPointerProperty(props, SDL_PROP_FILE_DIALOG_WINDOW_POINTER, window_);
    if (!location.empty())
        SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_LOCATION_STRING, location.c_str());
    SDL_SetStringProperty(props, SDL_PROP_FILE_DIALOG_TITLE_STRING, title.c_str());
    SDL_ShowFileDialogWithProperties(static_cast<SDL_FileDialogType>(kind), dialogCallback, &st,
                                     props);
    // Ответ приходит обратным вызовом, иногда из другого потока; пока его
    // нет, окно продолжает жить.
    while (!st.done) {
        pump();
        SDL_Delay(10);
    }
    SDL_DestroyProperties(props);
    in_.quitRequested = false;
    const std::lock_guard<std::mutex> lock(st.m);
    return st.result;
}

std::string Platform::openFileDialog(const std::string& title,
                                     const std::vector<FileFilter>& filters,
                                     const std::string& folder) {
    std::string loc = folder;
    if (!loc.empty() && loc.back() != '/' && loc.back() != '\\') loc += '/';
    return runDialog(SDL_FILEDIALOG_OPENFILE, title, filters,
                     dirExists(folder) ? loc : std::string());
}

std::string Platform::saveFileDialog(const std::string& title,
                                     const std::vector<FileFilter>& filters,
                                     const std::string& folder, const std::string& suggested) {
    const std::string loc = dirExists(folder) ? pathCombine(folder, suggested) : suggested;
    return runDialog(SDL_FILEDIALOG_SAVEFILE, title, filters, loc);
}

std::string Platform::clipboardText() const {
    char* t = SDL_GetClipboardText();
    std::string s = t != nullptr ? t : "";
    SDL_free(t);
    return s;
}

void Platform::setClipboardText(const std::string& s) {
    SDL_SetClipboardText(s.c_str());
}

void Platform::startTextInput(int x, int y, int w, int h) {
    if (window_ == nullptr) return;
    const SDL_Rect r{x, y, w, h};
    SDL_SetTextInputArea(window_, &r, 0);
    SDL_StartTextInput(window_);
    textInputOn_ = true;
    in_.text.clear();
}

void Platform::stopTextInput() {
    if (window_ != nullptr) SDL_StopTextInput(window_);
    textInputOn_ = false;
    in_.text.clear();
}

}  // namespace tjtm
