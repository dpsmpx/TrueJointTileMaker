#include "gfx/font.h"

#include <SDL3/SDL.h>
#include <SDL3_ttf/SDL_ttf.h>

#include <algorithm>

#include "core/fsutil.h"

namespace tjtm::gfx {

namespace {

std::string firstExisting(const std::vector<std::string>& paths) {
    for (const auto& p : paths) {
        if (fileExists(p)) return p;
    }
    return {};
}

/// Перебор знаков строки UTF-8.
template <typename F>
void forEachChar(const std::string& s, F&& f) {
    const char* p = s.c_str();
    std::size_t len = s.size();
    while (len > 0)
        f(SDL_StepUTF8(&p, &len));
}

}  // namespace

Font::~Font() {
    if (font_ != nullptr) TTF_CloseFont(font_);
}

bool Font::open(const std::string& path, float points, int dpi) {
    if (font_ != nullptr) TTF_CloseFont(font_);
    font_ = TTF_OpenFont(path.c_str(), points);
    if (font_ == nullptr) return false;
    TTF_SetFontSizeDPI(font_, points, dpi, dpi);
    // Лёгкий хинтинг: знаки чёткие по вертикали, а их ширины — исходные,
    // округлённые до точки. Полный хинтинг сжимал подписи на 3–4 точки на
    // слово и рвал слова («тайл ов»).
    TTF_SetFontHinting(font_, TTF_HINTING_LIGHT);
    // GDI+ пар знаков не сближает: с кернингом строки выходили уже, и
    // перенос по словам расходился с оригиналом.
    TTF_SetFontKerning(font_, false);
    path_ = path;
    lineSkip_ = TTF_GetFontLineSkip(font_);
    ascent_ = TTF_GetFontAscent(font_);
    height_ = TTF_GetFontHeight(font_);
    glyphs_.clear();
    return true;
}

const Font::Glyph& Font::glyph(std::uint32_t ch) const {
    const auto it = glyphs_.find(ch);
    if (it != glyphs_.end()) return it->second;
    Glyph g;
    if (font_ != nullptr) {
        int maxx = 0;
        int miny = 0;
        TTF_GetGlyphMetrics(font_, ch, &g.minx, &maxx, &miny, &g.maxy, &g.adv);
        TTF_ImageType type = TTF_IMAGE_INVALID;
        SDL_Surface* img = TTF_GetGlyphImage(font_, ch, &type);
        if (img != nullptr) {
            // Цвет кладётся при выводе: от знака нужно только покрытие.
            SDL_Surface* conv = SDL_ConvertSurface(img, SDL_PIXELFORMAT_ARGB8888);
            SDL_DestroySurface(img);
            if (conv != nullptr) {
                g.w = conv->w;
                g.h = conv->h;
                g.a.resize(static_cast<std::size_t>(g.w) * static_cast<std::size_t>(g.h));
                for (int y = 0; y < g.h; ++y) {
                    const auto* row = reinterpret_cast<const std::uint32_t*>(
                        static_cast<const std::uint8_t*>(conv->pixels) +
                        static_cast<std::ptrdiff_t>(y) * conv->pitch);
                    for (int x = 0; x < g.w; ++x) {
                        g.a[static_cast<std::size_t>(y) * static_cast<std::size_t>(g.w) +
                            static_cast<std::size_t>(x)] = static_cast<std::uint8_t>(row[x] >> 24);
                    }
                }
                SDL_DestroySurface(conv);
            }
        }
    }
    return glyphs_.emplace(ch, std::move(g)).first->second;
}

int Font::rawWidth(const std::string& s) const {
    // Сумма целых ширин знаков — так меряет GraphABC: на выборке строк из
    // интерфейса TextWidth оригинала совпал с ней до точки.
    int w = 0;
    forEachChar(s, [&](std::uint32_t ch) { w += glyph(ch).adv; });
    return w;
}

void Font::draw(Surface& dst, int x, int y, const std::string& s, Color c) const {
    // Знаки ставятся по тем же целым ширинам, по которым строка измерена:
    // иначе центровка и перенос считались бы по одной строке, а рисовалась
    // бы другая.
    int pen = x;
    // GDI+ ставит базовую линию на точку ниже, чем FreeType: по снимкам
    // эталона TextOut(36, 60) кладёт верх заглавных на строку 65, а без
    // поправки они выходили на 64 — и так во всех подписях.
    const int base = y + ascent_ + 1;
    forEachChar(s, [&](std::uint32_t ch) {
        const Glyph& g = glyph(ch);
        const int gx = pen + g.minx;
        const int gy = base - g.maxy;
        for (int yy = 0; yy < g.h; ++yy) {
            for (int xx = 0; xx < g.w; ++xx) {
                const int cov = g.a[static_cast<std::size_t>(yy) * static_cast<std::size_t>(g.w) +
                                    static_cast<std::size_t>(xx)];
                if (cov != 0) dst.blendPixelCoverage(gx + xx, gy + yy, c, cov);
            }
        }
        pen += g.adv;
    });
}

std::vector<std::string> Font::layoutBox(const std::string& s, int maxWidth) const {
    // GDI+ (DrawString в прямоугольник) переносит не так, как экран справки:
    // строка, которая влезает, остаётся как есть, с двойными пробелами;
    // перенос — только по пробелу, и пробелы на месте переноса пропадают.
    auto rtrim = [](std::string t) {
        while (!t.empty() && t.back() == ' ')
            t.pop_back();
        return t;
    };
    auto nextChar = [](const std::string& t, std::size_t i) {
        ++i;
        while (i < t.size() && (static_cast<unsigned char>(t[i]) & 0xC0U) == 0x80U)
            ++i;
        return i;
    };
    std::vector<std::string> lines;
    std::size_t start = 0;
    while (true) {
        const std::size_t nl = s.find('\n', start);
        std::string para =
            s.substr(start, nl == std::string::npos ? std::string::npos : nl - start);
        if (!para.empty() && para.back() == '\r') para.pop_back();
        while (true) {
            const std::string whole = rtrim(para);
            if (rawWidth(whole) <= maxWidth) {
                lines.push_back(whole);
                break;
            }
            std::size_t cut = std::string::npos;
            for (std::size_t p = para.find(' '); p != std::string::npos;
                 p = para.find(' ', p + 1)) {
                const std::string head = rtrim(para.substr(0, p));
                if (head.empty()) continue;
                if (rawWidth(head) > maxWidth) break;
                cut = p;
            }
            if (cut == std::string::npos) {
                // Слово шире прямоугольника: GDI+ режет его по знакам.
                std::size_t p = nextChar(para, 0);
                while (p < para.size()) {
                    const std::size_t q = nextChar(para, p);
                    if (rawWidth(para.substr(0, q)) > maxWidth) break;
                    p = q;
                }
                lines.push_back(para.substr(0, p));
                para.erase(0, p);
            } else {
                lines.push_back(rtrim(para.substr(0, cut)));
                const std::size_t rest = para.find_first_not_of(' ', cut);
                para = rest == std::string::npos ? std::string() : para.substr(rest);
            }
            if (para.empty()) break;
        }
        if (nl == std::string::npos) break;
        start = nl + 1;
    }
    return lines;
}

void Font::drawCentered(Surface& dst, int x0, int y0, int x1, int y1, const std::string& s,
                        Color c) const {
    if (x1 <= x0 || y1 <= y0) return;
    const int w = x1 - x0;
    const int h = y1 - y0;
    // Блок строк ставится по центру; лишнее срезается краем прямоугольника.
    const std::vector<std::string> lines = layoutBox(s, w);
    const int blockH = static_cast<int>(lines.size()) * lineSkip_;
    const int top = y0 + (h - blockH) / 2;
    const Rect old = dst.clip();
    Rect r{x0, y0, x1, y1};
    if (!old.empty()) {
        r.x0 = std::max(r.x0, old.x0);
        r.y0 = std::max(r.y0, old.y0);
        r.x1 = std::min(r.x1, old.x1);
        r.y1 = std::min(r.y1, old.y1);
    }
    dst.setClip(r.empty() ? Rect{0, 0, 1, 1} : r);
    if (!r.empty()) {
        for (std::size_t i = 0; i < lines.size(); ++i) {
            const int lw = rawWidth(lines[i]);
            // Строка стоит в своей ячейке высотой lineSkip; FreeType кладёт
            // базовую линию ниже верха картинки на ascent.
            const int ly = top + static_cast<int>(i) * lineSkip_ + (lineSkip_ - height_) / 2;
            draw(dst, x0 + (w - lw) / 2, ly, lines[i], c);
        }
    }
    if (old.empty()) {
        dst.resetClip();
    } else {
        dst.setClip(old);
    }
}

std::string findUiFont(const std::string& resourcesDir) {
    return firstExisting({
#ifdef _WIN32
        "C:/Windows/Fonts/arial.ttf",
#endif
        pathCombine(resourcesDir, "Fonts/LiberationSans-Regular.ttf"),
        "/usr/share/fonts/truetype/liberation/LiberationSans-Regular.ttf",
        "/usr/share/fonts/truetype/msttcorefonts/Arial.ttf",
        "/Library/Fonts/Arial.ttf",
        "/System/Library/Fonts/Supplemental/Arial.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
    });
}

std::string findMonoFont(const std::string& resourcesDir) {
    return firstExisting({
#ifdef _WIN32
        "C:/Windows/Fonts/cour.ttf",
#endif
        pathCombine(resourcesDir, "Fonts/LiberationMono-Regular.ttf"),
        "/usr/share/fonts/truetype/liberation/LiberationMono-Regular.ttf",
        "/Library/Fonts/Courier New.ttf",
        "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
    });
}

}  // namespace tjtm::gfx
