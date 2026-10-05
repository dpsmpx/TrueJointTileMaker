// Проверка инструментов переноса: окно, текст, картинка и снимок кадра.
//
// Это ещё не редактор. Программа проверяет, что цепочка, на которой будет
// стоять перенос, собирается и работает: SDL2 открывает окно размером с окно
// оригинала, SDL_ttf пишет кириллицу шрифтом с метриками Arial 10 pt — тем,
// которым GraphABC рисует весь интерфейс, — SDL_image читает палитру из
// Resources и записывает снимок кадра в PNG.
//
// Кадр рисуется в программный буфер и только потом выводится в окно, как
// у GraphABC с LockDrawing и Redraw. Снимок поэтому берётся из буфера, а не
// с экрана, и одинаков что в окне, что без него.
//
//   tjtm [--frames N] [--screenshot FILE.png] [--font FILE.ttf]
//
// --frames N — закрыться после N кадров; вместе с SDL_VIDEODRIVER=offscreen
// программа проходит тест без экрана.

#include <SDL.h>
#include <SDL_image.h>
#include <SDL_ttf.h>

#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace {

// Размеры окна оригинала: холст 600x560 и панель 180 справа.
constexpr int kWinW = 780;
constexpr int kWinH = 560;

struct Rgb {
    Uint8 r, g, b;
};

// Цвета интерфейса оригинала (cWindow, cText, cTextDim, cBorder).
constexpr Rgb kWindow{240, 242, 246};
constexpr Rgb kText{28, 32, 38};
constexpr Rgb kTextDim{122, 130, 141};
constexpr Rgb kBorder{205, 210, 218};

struct Options {
    int frames = -1;
    std::string screenshot;
    std::string font;
};

bool parseArgs(int argc, char** argv, Options& opt) {
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        const bool hasValue = i + 1 < argc;
        if (a == "--frames" && hasValue) {
            opt.frames = std::atoi(argv[++i]);
        } else if (a == "--screenshot" && hasValue) {
            opt.screenshot = argv[++i];
        } else if (a == "--font" && hasValue) {
            opt.font = argv[++i];
        } else {
            std::fprintf(stderr, "неизвестный ключ: %s\n", a.c_str());
            return false;
        }
    }
    return true;
}

bool fileExists(const std::string& path) {
    SDL_RWops* f = SDL_RWFromFile(path.c_str(), "rb");
    if (f == nullptr) return false;
    SDL_RWclose(f);
    return true;
}

// Шрифт с метриками Arial. На Windows это сам Arial, в Linux — Liberation
// Sans: он сделан метрически совместимым, и строки занимают ту же ширину.
std::string findFont(const Options& opt) {
    std::vector<std::string> candidates;
    if (!opt.font.empty()) candidates.push_back(opt.font);
    if (const char* env = std::getenv("TJTM_FONT")) candidates.emplace_back(env);
    candidates.emplace_back("C:/Windows/Fonts/arial.ttf");
    candidates.emplace_back("/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf");
    candidates.emplace_back("/usr/share/fonts/truetype/msttcorefonts/Arial.ttf");
    candidates.emplace_back("/Library/Fonts/Arial.ttf");
    candidates.emplace_back("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf");
    for (const auto& c : candidates) {
        if (fileExists(c)) return c;
    }
    return {};
}

// Ресурс ищется рядом с программой, а если его там нет — в исходниках.
std::string findResource(const std::string& rel) {
    if (char* base = SDL_GetBasePath()) {
        std::string p = std::string(base) + rel;
        SDL_free(base);
        if (fileExists(p)) return p;
    }
    const std::string p = std::string(TJTM_SOURCE_DIR) + "/" + rel;
    return fileExists(p) ? p : std::string();
}

void fillRect(SDL_Surface* s, int x0, int y0, int x1, int y1, Rgb c) {
    const SDL_Rect r{x0, y0, x1 - x0, y1 - y0};
    SDL_FillRect(s, &r, SDL_MapRGB(s->format, c.r, c.g, c.b));
}

// Текст, отцентрованный в прямоугольнике, — как DrawTextCentered в GraphABC.
void textCentered(SDL_Surface* s, TTF_Font* font, int x0, int y0, int x1, int y1,
                  const std::string& text, Rgb c) {
    SDL_Surface* t = TTF_RenderUTF8_Blended(font, text.c_str(), SDL_Color{c.r, c.g, c.b, 255});
    if (t == nullptr) return;
    SDL_Rect dst{x0 + (x1 - x0 - t->w) / 2, y0 + (y1 - y0 - t->h) / 2, t->w, t->h};
    SDL_BlitSurface(t, nullptr, s, &dst);
    SDL_FreeSurface(t);
}

std::string versionLine() {
    SDL_version sdl;
    SDL_GetVersion(&sdl);
    const SDL_version* ttf = TTF_Linked_Version();
    const SDL_version* img = IMG_Linked_Version();
    auto v = [](const SDL_version& x) {
        return std::to_string(x.major) + "." + std::to_string(x.minor) + "." +
               std::to_string(x.patch);
    };
    return "SDL " + v(sdl) + " · SDL_ttf " + v(*ttf) + " · SDL_image " + v(*img);
}

void drawFrame(SDL_Surface* back, TTF_Font* font, SDL_Surface* palette,
               const std::string& fontPath) {
    fillRect(back, 0, 0, kWinW, kWinH, kWindow);
    textCentered(back, font, 0, 16, kWinW, 52, "True Joint Tile Maker", kText);
    textCentered(back, font, 0, 52, kWinW, 74, "перенос на C++ · проверка инструментов", kTextDim);
    textCentered(back, font, 0, 74, kWinW, 96, versionLine(), kTextDim);
    textCentered(back, font, 0, 96, kWinW, 118, "шрифт: " + fontPath, kTextDim);

    // Палитра по умолчанию, ячейки по 16 точек, как PAL_CELL в оригинале.
    // Растягивается ближайшим соседом: пиксель-арт не сглаживают.
    if (palette != nullptr) {
        const int cell = 16;
        const int x0 = (kWinW - palette->w * cell) / 2;
        const int y0 = 140;
        fillRect(back, x0 - 1, y0 - 1, x0 + palette->w * cell + 1, y0 + palette->h * cell + 1,
                 kBorder);
        SDL_Rect dst{x0, y0, palette->w * cell, palette->h * cell};
        SDL_BlitScaled(palette, nullptr, back, &dst);
    }
    textCentered(back, font, 0, kWinH - 40, kWinW, kWinH - 18, "Escape закрывает окно", kTextDim);
}

}  // namespace

int main(int argc, char** argv) {
    Options opt;
    if (!parseArgs(argc, argv, opt)) return 2;

    if (SDL_Init(SDL_INIT_VIDEO) != 0) {
        std::fprintf(stderr, "SDL_Init: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() != 0 || (IMG_Init(IMG_INIT_PNG) & IMG_INIT_PNG) == 0) {
        std::fprintf(stderr, "TTF/IMG: %s\n", SDL_GetError());
        SDL_Quit();
        return 1;
    }

    int rc = 0;
    SDL_Window* win = nullptr;
    SDL_Surface* back = nullptr;
    SDL_Surface* palette = nullptr;
    TTF_Font* font = nullptr;

    const std::string fontPath = findFont(opt);
    const std::string palettePath = findResource("Resources/Palitra.png");

    // Размер в пунктах при 96 точках на дюйм — так GDI считает шрифт
    // Arial 10, которым пишет GraphABC: те же 13 с третью точек.
    if (!fontPath.empty()) font = TTF_OpenFontDPI(fontPath.c_str(), 10, 96, 96);
    if (fontPath.empty()) {
        std::fprintf(stderr, "не найден шрифт: укажите --font или TJTM_FONT\n");
        rc = 1;
    } else if (font == nullptr) {
        std::fprintf(stderr, "TTF_OpenFont: %s\n", TTF_GetError());
        rc = 1;
    }
    if (rc == 0 && !palettePath.empty()) {
        palette = IMG_Load(palettePath.c_str());
        if (palette == nullptr) std::fprintf(stderr, "IMG_Load: %s\n", IMG_GetError());
    }
    if (rc == 0) {
        win = SDL_CreateWindow("True Joint Tile Maker — C++", SDL_WINDOWPOS_CENTERED,
                               SDL_WINDOWPOS_CENTERED, kWinW, kWinH, 0);
        back = SDL_CreateRGBSurfaceWithFormat(0, kWinW, kWinH, 32, SDL_PIXELFORMAT_ARGB8888);
        if (win == nullptr || back == nullptr) {
            std::fprintf(stderr, "окно: %s\n", SDL_GetError());
            rc = 1;
        }
    }

    for (int frame = 0; rc == 0; ++frame) {
        bool quit = false;
        SDL_Event e;
        while (SDL_PollEvent(&e) != 0) {
            if (e.type == SDL_QUIT) quit = true;
            if (e.type == SDL_KEYDOWN && e.key.keysym.sym == SDLK_ESCAPE) quit = true;
        }

        drawFrame(back, font, palette, fontPath);
        if (SDL_Surface* ws = SDL_GetWindowSurface(win)) {
            SDL_BlitSurface(back, nullptr, ws, nullptr);
            SDL_UpdateWindowSurface(win);
        }

        if (opt.frames >= 0 && frame + 1 >= opt.frames) quit = true;
        if (quit) {
            if (!opt.screenshot.empty() && IMG_SavePNG(back, opt.screenshot.c_str()) != 0) {
                std::fprintf(stderr, "IMG_SavePNG: %s\n", IMG_GetError());
                rc = 1;
            }
            break;
        }
        SDL_Delay(16);
    }

    if (palette != nullptr) SDL_FreeSurface(palette);
    if (back != nullptr) SDL_FreeSurface(back);
    if (font != nullptr) TTF_CloseFont(font);
    if (win != nullptr) SDL_DestroyWindow(win);
    IMG_Quit();
    TTF_Quit();
    SDL_Quit();
    return rc;
}
