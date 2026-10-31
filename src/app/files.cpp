// Файлы и команды (TJTM.pas: «Файловые операции», «Палитра и тайлсет»,
// «Карты локаций», «Команды»). Каждый вход в файловую систему проверяет
// ошибку: испорченный или чужой файл не должен ронять редактор.

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>

#include "app/editor.h"
#include "core/anim.h"
#include "core/fsutil.h"
#include "core/gif.h"
#include "core/pmath.h"
#include "core/png.h"
#include "core/project.h"
#include "core/rasterfmt.h"
#include "core/text.h"
#include "core/theme.h"

namespace tjtm {

using namespace theme;

namespace {

// Проект .tjt открывается тем же «Загрузить»: это тоже PNG, только с
// куском проекта внутри.
const std::vector<FileFilter> kPngFilter = {
    {"Картинки, анимации и проекты", "png;gif;tjt;bmp;tif;tiff;jpg;jpeg"},
    {"PNG и APNG", "png"},
    {"Проекты редактора", "tjt"},
    {"GIF", "gif"},
    {"BMP", "bmp"},
    {"TIFF", "tif;tiff"},
    {"Все файлы", "*"}};
const std::vector<FileFilter> kMapFilter = {{"Карты локаций", "map"}, {"Все файлы", "*"}};

/// Расширение по умолчанию, если в диалоге сохранения его не написали —
/// AddExtension оригинала.
std::string withExtension(const std::string& path, const std::string& ext) {
    if (path.empty() || !pathExtLower(path).empty()) return path;
    return path + "." + ext;
}

std::string replaceExtension(const std::string& path, const std::string& ext) {
    const std::string old = pathExtLower(path);
    return path.substr(0, path.size() - old.size()) + ext;
}

/// Проект ли это: по расширению или, у переименованного, по содержимому.
bool looksLikeProject(const std::string& path) {
    if (pathExtLower(path) == ".tjt") return true;
    const auto data = readFile(path);
    if (!data) return false;
    return isProjectFile(std::vector<std::uint8_t>(data->begin(), data->end()));
}

}  // namespace

std::string Editor::askOpenFile(const std::string& title) {
    const std::string r = pf_.openFileDialog(title, kPngFilter, resolvePath(cfg.lastFolder));
    waitMouseRelease();
    m.needRepaint = true;
    return r;
}

std::string Editor::askFolder(const std::string& title, const std::string& current) {
    const std::string start = resolvePath(current.empty() ? cfg.lastFolder : current);
    std::string r = pf_.openFolderDialog(title, start);
    waitMouseRelease();
    m.needRepaint = true;
    // Ответ-файл — его папка: так отвечают диалоги сценариев 1.6, где папку
    // выбирали через любой файл в ней.
    if (!r.empty() && !dirExists(r) && fileExists(r)) r = pathDirName(r);
    return r;
}

std::string Editor::askSaveFile(const std::string& title, const std::string& suggested) {
    const std::string r =
        pf_.saveFileDialog(title, kPngFilter, resolvePath(cfg.lastFolder), suggested);
    waitMouseRelease();
    m.needRepaint = true;
    // Без расширения: документ со слоями — проектом, картинка — PNG.
    return withExtension(r, m.multiCel() ? "tjt" : "png");
}

/// Папка карт для диалогов: сначала настройка, потом папка последней карты.
std::string Editor::mapsDir() const {
    std::string r = resolvePath(cfg.mapsFolder);
    if (!r.empty() && dirExists(r)) return r;
    if (!cfg.lastMapFile.empty()) return pathDirName(resolvePath(cfg.lastMapFile));
    return {};
}

std::string Editor::askOpenMap(const std::string& title) {
    const std::string r = pf_.openFileDialog(title, kMapFilter, mapsDir());
    waitMouseRelease();
    m.needRepaint = true;
    return r;
}

std::string Editor::askSaveMap(const std::string& title, const std::string& suggested) {
    const std::string r = pf_.saveFileDialog(title, kMapFilter, mapsDir(), suggested);
    waitMouseRelease();
    m.needRepaint = true;
    return withExtension(r, "map");
}

/// Читает картинку в сетку цветов. PNG — своим чтением, остальное, что
/// открывал GDI+ (BMP, GIF, JPEG), — через SDL3_image.
bool Editor::readImageGrid(const std::string& path, Grid<Color>& g, bool quiet) {
    if (!fileExists(path)) {
        if (!quiet) errorOpen("Файл не найден", path);
        return false;
    }
    const auto data = readFile(path);
    if (!data) {
        if (!quiet) errorOpen("Не удалось открыть файл", path);
        return false;
    }
    std::string err;
    Grid<Color> img;
    const std::vector<std::uint8_t> bytes(data->begin(), data->end());
    bool ok = false;
    if (bytes.size() >= 8 && bytes[0] == 137 && bytes[1] == 'P' && bytes[2] == 'N' &&
        bytes[3] == 'G') {
        ok = pngDecode(bytes, img, err);
    } else if (isTiff(bytes)) {
        // TIFF — своим чтением: SDL_image собран без libtiff.
        std::vector<Grid<Color>> pages;
        ok = tiffDecode(bytes, pages, err);
        if (ok) img = std::move(pages.front());
    } else {
        SDL_IOStream* io = SDL_IOFromConstMem(bytes.data(), bytes.size());
        SDL_Surface* s = io != nullptr ? IMG_Load_IO(io, true) : nullptr;
        if (s != nullptr) {
            SDL_Surface* c = SDL_ConvertSurface(s, SDL_PIXELFORMAT_ARGB8888);
            SDL_DestroySurface(s);
            if (c != nullptr) {
                img.resize(c->w, c->h);
                for (int y = 0; y < c->h; ++y) {
                    const auto* row = reinterpret_cast<const std::uint32_t*>(
                        static_cast<const std::uint8_t*>(c->pixels) +
                        static_cast<std::ptrdiff_t>(y) * c->pitch);
                    for (int x = 0; x < c->w; ++x)
                        img.at(x, y) = Color::fromArgb(row[x]);
                }
                SDL_DestroySurface(c);
                ok = true;
            }
        }
        if (!ok) err = SDL_GetError();
    }
    if (!ok) {
        if (!quiet) errorOpen("Не удалось открыть файл", err);
        return false;
    }
    if (img.w > MAX_TILE || img.h > MAX_TILE) {
        if (!quiet) {
            showMessage("Слишком большое изображение",
                        "Максимальный размер тайла — " + std::to_string(MAX_TILE) + " на " +
                            std::to_string(MAX_TILE) + " пикселей. В файле " +
                            std::to_string(img.w) + " на " + std::to_string(img.h) + ".");
        }
        return false;
    }
    g = std::move(img);
    return true;
}

/// Загружает тайл; размер документа берётся из самого изображения.
/// remember = false — для автозагрузки при старте: стартовая картинка не
/// должна подменять собой последний файл пользователя.
bool Editor::loadTileFrom(const std::string& path, bool quiet, bool remember) {
    stopPlayback();
    playTag_ = -1;
    if (looksLikeProject(path)) {
        std::string err;
        // Модель меняется, только если проект прочитан целиком.
        if (!projectLoad(path, m, err)) {
            if (!quiet) errorOpen("Проект не открыт", path + ": " + err);
            return false;
        }
        m.currentFile = path;
        m.tileModified = false;
        if (m.multiCel()) timelineOn_ = true;
        if (remember) rememberFile(path, false);
        return true;
    }
    // GIF и APNG из нескольких кадров открываются кадрами; одиночная
    // картинка — прежним путём.
    const int anim = tryLoadAnimation(path, quiet);
    if (anim < 0) return false;
    if (anim > 0) {
        if (remember) rememberFile(path, false);
        return true;
    }
    Grid<Color> g;
    if (!readImageGrid(path, g, quiet)) return false;
    m.setTile(std::move(g), path);
    if (remember) rememberFile(path, false);
    return true;
}

bool Editor::saveProjectTo(const std::string& path) {
    std::string err;
    if (!projectSave(path, m, err)) {
        errorSave("Не удалось сохранить проект", path + ": " + err);
        return false;
    }
    m.currentFile = path;
    m.tileModified = false;
    rememberFile(path, false);
    return true;
}

bool Editor::saveTileTo(const std::string& path) {
    if (path.empty()) return false;
    // Записанное могло лечь в набор игры: картинки карты перечитаются.
    gameTilesDirty_ = true;
    const int nf = m.doc.frameCount();
    if (pathExtLower(path) == ".tjt") return saveProjectTo(path);
    const int fmt = imageFormatOf(path);
    if (!m.multiCel()) {
        // Одна картинка — сама картинка тайла, как у оригинала, в формате
        // по расширению.
        if (!writeImageFile(path, {AnimFrame{m.tile, m.doc.frames.front().durationMs}}, 0))
            return false;
        m.currentFile = path;
        m.tileModified = false;
        rememberFile(path, false);
        return true;
    }
    // Кадры помещаются в GIF, APNG и TIFF страницами, в BMP — нет.
    const bool framesFit = fmt != FMT_BMP;
    // Открытая анимация из одного слоя сохраняется в том же виде, в
    // каком её открыли.
    if (m.doc.layerCount() == 1 && framesFit) return saveAnimationTo(path, 0, nf - 1, 1, 0, true);
    const std::string name = fmt == FMT_GIF    ? "GIF"
                             : fmt == FMT_TIFF ? "TIFF"
                             : fmt == FMT_BMP  ? "BMP"
                                               : "PNG";
    const std::string what = m.doc.layerCount() > 1
                                 ? name + " хранит картинки, а не слои: слои сведутся"
                                 : name + " хранит одну картинку";
    const std::string frames =
        nf > 1 ? (framesFit ? ", кадры сохранятся по одному" : ", сохранится только текущий кадр")
               : "";
    const int a =
        ask3("Слои и кадры", what + frames + ". Слои, кадры и метки целиком сохраняет проект .tjt.",
             "Сохранить проект", "Сохранить картинку", "Отмена");
    if (a < 0) return false;
    if (a == 1) return saveProjectTo(replaceExtension(path, ".tjt"));
    if (framesFit) return saveAnimationTo(path, 0, nf - 1, 1, 0, false);
    return saveAnimationTo(path, m.activeFrame, m.activeFrame, 1, 0, false);
}

int Editor::imageFormatOf(const std::string& path) {
    const std::string ext = pathExtLower(path);
    if (ext == ".gif") return FMT_GIF;
    if (ext == ".bmp") return FMT_BMP;
    if (ext == ".tif" || ext == ".tiff") return FMT_TIFF;
    return FMT_PNG;
}

bool Editor::writeImageFile(const std::string& path, const std::vector<AnimFrame>& frames,
                            int plays) {
    if (frames.empty()) return false;
    std::vector<std::uint8_t> bytes;
    switch (imageFormatOf(path)) {
        case FMT_GIF:
            // Цвета палитры идут в GIF первыми и в её порядке.
            bytes = gifEncode(frames, plays, m.palette);
            break;
        case FMT_BMP: bytes = bmpEncode(frames.front().image); break;
        case FMT_TIFF: {
            std::vector<Grid<Color>> pages;
            pages.reserve(frames.size());
            for (const AnimFrame& f : frames)
                pages.push_back(f.image);
            bytes = tiffEncode(pages);
            break;
        }
        default:
            bytes = frames.size() > 1 ? apngEncode(frames, plays) : pngEncode(frames.front().image);
            break;
    }
    if (!writeFile(path,
                   std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()))) {
        errorSave("Не удалось сохранить файл",
                  path + ": не удалось записать файл. Проверьте права на запись.");
        return false;
    }
    return true;
}

// ---------------------------------------------------------------------------
// Анимация: открытие кадрами и выгрузка (этап 4)
// ---------------------------------------------------------------------------

int Editor::tryLoadAnimation(const std::string& path, bool quiet) {
    const auto data = readFile(path);
    if (!data) return 0;
    const std::vector<std::uint8_t> bytes(data->begin(), data->end());
    std::vector<AnimFrame> frames;
    std::string err;
    std::vector<std::uint8_t> actl;
    if (isGif(bytes)) {
        if (!gifDecode(bytes, frames, err)) {
            if (!quiet) errorOpen("Не удалось открыть GIF", path + ": " + err);
            return -1;
        }
    } else if (isTiff(bytes)) {
        // Страницы TIFF — кадры; длительностей в TIFF нет.
        std::vector<Grid<Color>> pages;
        if (!tiffDecode(bytes, pages, err)) {
            if (!quiet) errorOpen("Не удалось открыть TIFF", path + ": " + err);
            return -1;
        }
        for (Grid<Color>& g : pages)
            frames.push_back(AnimFrame{std::move(g), 100});
    } else if (pngFindChunk(bytes, "acTL", actl)) {
        if (!apngDecode(bytes, frames, err)) {
            if (!quiet) errorOpen("Не удалось открыть APNG", path + ": " + err);
            return -1;
        }
    } else {
        return 0;
    }
    if (frames.size() < 2) return 0;
    const Grid<Color>& first = frames.front().image;
    if (first.w > MAX_TILE || first.h > MAX_TILE || first.w < 1 || first.h < 1) {
        if (!quiet) {
            showMessage("Слишком большое изображение",
                        "Максимальный размер тайла — " + std::to_string(MAX_TILE) + " на " +
                            std::to_string(MAX_TILE) + " пикселей. В файле " +
                            std::to_string(first.w) + " на " + std::to_string(first.h) + ".");
        }
        return -1;
    }
    m.setAnimation(frames, path);
    timelineOn_ = true;
    return 1;
}

bool Editor::saveAnimationTo(const std::string& path, int from, int to, int scale, int plays,
                             bool asDocument) {
    if (path.empty()) return false;
    const std::vector<AnimFrame> frames = scaleFrames(m.animationFrames(from, to), scale);
    if (frames.empty() || !writeImageFile(path, frames, plays)) return false;
    // Документом файл становится, только если в нём ничего не потеряно:
    // один слой, все кадры, без увеличения.
    if (asDocument && m.doc.layerCount() == 1 && from == 0 && to >= m.doc.frameCount() - 1 &&
        scale == 1) {
        m.currentFile = path;
        m.tileModified = false;
        rememberFile(path, false);
    }
    return true;
}

std::string Editor::askExportFile(const std::string& title, const std::string& suggested,
                                  const std::string& ext, const std::string& filterName) {
    const std::vector<FileFilter> filter = {{filterName, ext}, {"Все файлы", "*"}};
    const std::string r = pf_.saveFileDialog(title, filter, resolvePath(cfg.lastFolder), suggested);
    waitMouseRelease();
    m.needRepaint = true;
    return withExtension(r, ext);
}

void Editor::exportRange(int& from, int& to) const {
    from = 0;
    to = m.doc.frameCount() - 1;
    if (playTag_ >= 0 && playTag_ < static_cast<int>(m.doc.tags.size())) {
        from = m.doc.tags[static_cast<std::size_t>(playTag_)].from;
        to = m.doc.tags[static_cast<std::size_t>(playTag_)].to;
    }
}

namespace {

/// Основа имён выгрузки: имя документа без расширения или «sprite».
std::string exportBase(const std::string& currentFile) {
    const std::string stem = pathStem(currentFile);
    return stem.empty() ? "sprite" : stem;
}

}  // namespace

void Editor::exportSheetAsk() {
    const int nf = m.doc.frameCount();
    int from = 0;
    int to = 0;
    exportRange(from, to);
    int first = from + 1;
    int last = to + 1;
    int& cols = sheetCols_;
    int& margin = sheetMargin_;
    int& spacing = sheetSpacing_;
    int& padding = sheetPadding_;
    int& scale = sheetScale_;
    bool done = false;
    bool ok = false;
    dropInput();
    pf_.input().keys.clear();
    while (!done && !quitNow_) {
        beginFrameInput();
        fb_.clear(cWindow);
        textCentered(0, 16, WIN_W, 48, "Лист спрайтов", cText);
        uiLabel(0, 46, WIN_W, 22, "Рядом ляжет описание .json в виде Aseprite — его читают движки",
                cTextDim);
        uiStepper(24, 90, "С кадра", first, 1, nf, 10);
        uiStepper(24, 128, "По кадр", last, 1, nf, 10);
        uiLabelLeft(24, 176, 30, "Раскладка", cText);
        if (uiToggle(154, 176, 104, 30, "Строкой", cols == 0)) cols = 0;
        if (uiToggle(262, 176, 104, 30, "Столбцом", cols == 1)) cols = 1;
        if (cols >= 2 || cols < 0) {
            uiStepper(24, 214, "Столбцов", cols, 2, std::max(2, nf), 4);
        } else if (uiButton(154, 214, 212, 30, "Сеткой…", nf >= 2)) {
            cols = std::max(2, static_cast<int>(std::ceil(std::sqrt(static_cast<double>(nf)))));
        }
        uiStepper(400, 90, "Поле вокруг", margin, 0, 64, 8);
        uiStepper(400, 128, "Промежуток", spacing, 0, 64, 8);
        uiStepper(400, 166, "Отступ в клетке", padding, 0, 32, 4);
        uiStepper(400, 204, "Увеличение", scale, 1, 8, 2, "×");
        uiLabelLeft(24, 262, 30, "Фон", cText);
        static const char* kBg[3] = {"Прозрачный", "Второй цвет", "Первый цвет"};
        for (int k = 0; k < 3; ++k) {
            if (uiToggle(154 + k * 150, 262, 144, 30, kBg[k], sheetBg_ == k)) sheetBg_ = k;
        }
        // Размер листа — сразу, чтобы не гадать, влезет ли он в атлас движка.
        {
            SheetOptions o;
            o.layout = cols == 0 ? SHEET_ROW : (cols == 1 ? SHEET_COLUMN : SHEET_GRID);
            o.columns = cols;
            int gc = 1;
            int gr = 1;
            sheetGrid(std::abs(last - first) + 1, o, gc, gr);
            const int cw = m.tile.w * scale + 2 * padding;
            const int ch = m.tile.h * scale + 2 * padding;
            uiLabel(0, 310, WIN_W, 24,
                    "Лист " + std::to_string(margin * 2 + gc * cw + (gc - 1) * spacing) + " × " +
                        std::to_string(margin * 2 + gr * ch + (gr - 1) * spacing) + " точек",
                    cTextDim);
        }
        if (uiButton(WIN_W / 2 - 180, 360, 170, 38, "Дальше…", true)) {
            ok = true;
            done = true;
        }
        if (uiButton(WIN_W / 2 + 10, 360, 170, 38, "Отмена", true)) done = true;
        redraw();
        const int key = takeKey();
        if (key == KEY_ESC) done = true;
        if (key == KEY_ENTER) {
            ok = true;
            done = true;
        }
        idle();
    }
    waitMouseRelease();
    m.needRepaint = true;
    if (!ok) return;
    if (first > last) std::swap(first, last);
    const std::string path = askExportFile("Сохранить лист спрайтов",
                                           exportBase(m.currentFile) + "_sheet.png", "png", "PNG");
    if (path.empty()) return;
    SheetOptions o;
    o.layout = cols == 0 ? SHEET_ROW : (cols == 1 ? SHEET_COLUMN : SHEET_GRID);
    o.columns = cols;
    o.margin = margin;
    o.spacing = spacing;
    o.padding = padding;
    o.scale = scale;
    o.background = sheetBg_ == 1 ? m.secondColor : (sheetBg_ == 2 ? m.firstColor : kClear);
    const SpriteSheet sheet = buildSheet(m.animationFrames(first - 1, last - 1), o);
    std::string err;
    if (!pngSave(path, sheet.image, err)) {
        errorSave("Не удалось сохранить лист", path + ": " + err);
        return;
    }
    // Метки — в номерах кадров листа: отрезок, обрезанный по выгрузке.
    std::vector<AnimTag> tags;
    for (const AnimTag& t : m.doc.tags) {
        const int a = std::max(t.from, first - 1);
        const int b = std::min(t.to, last - 1);
        if (a <= b) tags.push_back(AnimTag{t.name, a - (first - 1), b - (first - 1), t.direction});
    }
    const std::string json = sheetJson(sheet, pathFileName(path), pathStem(path), tags);
    const std::string jsonPath = replaceExtension(path, ".json");
    if (!writeFile(jsonPath, json))
        errorSave("Не удалось сохранить описание", jsonPath + ": не удалось записать файл.");
}

void Editor::exportAnimationAsk(bool gif) {
    const int nf = m.doc.frameCount();
    int from = 0;
    int to = 0;
    exportRange(from, to);
    int first = from + 1;
    int last = to + 1;
    int scale = 1;
    int plays = 0;
    if (!askNumbers(gif ? "Анимация GIF" : "Анимация APNG",
                    gif ? "До 256 цветов, прозрачность только полная; повторов 0 — бесконечно"
                        : "Все цвета и полупрозрачность; повторов 0 — бесконечно",
                    {{"С кадра", &first, 1, nf},
                     {"По кадр", &last, 1, nf},
                     {"Увеличение", &scale, 1, 8},
                     {"Проигрываний", &plays, 0, 100}},
                    "Дальше…", nullptr))
        return;
    if (first > last) std::swap(first, last);
    const std::string path = askExportFile(gif ? "Сохранить GIF" : "Сохранить APNG",
                                           exportBase(m.currentFile) + (gif ? ".gif" : ".png"),
                                           gif ? "gif" : "png", gif ? "GIF" : "APNG");
    if (!path.empty()) saveAnimationTo(path, first - 1, last - 1, scale, plays, false);
}

void Editor::exportFramesAsk() {
    const int nf = m.doc.frameCount();
    int from = 0;
    int to = 0;
    exportRange(from, to);
    int first = from + 1;
    int last = to + 1;
    int scale = 1;
    if (!askNumbers(
            "Кадры в PNG", "Каждый кадр — свой файл: имя_001.png, имя_002.png…",
            {{"С кадра", &first, 1, nf}, {"По кадр", &last, 1, nf}, {"Увеличение", &scale, 1, 8}},
            "Дальше…", nullptr))
        return;
    if (first > last) std::swap(first, last);
    const std::string path =
        askExportFile("Основа имён кадров", exportBase(m.currentFile) + ".png", "png", "PNG");
    if (path.empty()) return;
    const std::string stem = path.substr(0, path.size() - pathExtLower(path).size());
    const auto frames = scaleFrames(m.animationFrames(first - 1, last - 1), scale);
    std::string err;
    std::string failed;
    for (std::size_t k = 0; k < frames.size() && failed.empty(); ++k) {
        char num[16];
        std::snprintf(num, sizeof num, "_%03d.png", first + static_cast<int>(k));
        std::string name = stem;
        name += num;
        if (!pngSave(name, frames[k].image, err)) failed = name;
    }
    if (!failed.empty()) errorSave("Не удалось сохранить кадр", failed + ": " + err);
}

void Editor::exportToTilesetSlot() {
    const std::string slot = m.metaValue("tileset.slot");
    const int kind = std::atoi(m.metaValue("tileset.kind").c_str());
    if (slot.empty() || kind < 0 || kind >= TS_KINDS) return;
    const std::string path = tsetPathOf(kind, slot);
    if (path.empty()) {
        showMessage("Папка набора не задана", "Укажите её на экране «Наборы тайлов».");
        return;
    }
    std::string err;
    if (!pngSave(path, m.compositeFrame(0), err)) {
        errorSave("Не удалось записать тайл", path + ": " + err);
        return;
    }
    if (kind == TS_ICON) iconsDirty_ = true;
    notify("Тайл в наборе", "Первый кадр записан в " + path + ".");
}

bool Editor::saveGridTo(const Grid<Color>& g, const std::string& path) {
    if (path.empty() || g.empty()) return false;
    std::string err;
    if (!pngSave(path, g, err)) {
        errorSave("Не удалось сохранить файл", path + ": " + err);
        return false;
    }
    return true;
}

/// Палитра из картинки, растянутой до 8x8.
/// Палитра из файла настроек. Оригинал растягивал любую картинку до 8x8;
/// этап 2 берёт каждую точку небольшой картинки ячейкой, а у большой —
/// её цвета (loadPaletteFile). Для Palitra.png 8x8 это одно и то же.
bool Editor::loadPaletteFrom(const std::string& path, bool quiet) {
    return loadPaletteFile(path, quiet, false);
}

void Editor::paletteIO() {
    showPaletteScreen();
}

bool Editor::loadMapFrom(const std::string& path, bool quiet, bool remember) {
    if (!fileExists(path)) {
        if (!quiet) errorOpen("Файл не найден", path);
        return false;
    }
    const MapParseResult r = mapLoad(path, m.map);
    if (!r.ok) {
        if (!quiet) errorOpen("Карта не прочитана", r.error);
        return false;
    }
    m.currentMapFile = path;
    m.mapModified = false;
    m.docMode = DOC_MAP;
    m.resetSelection();
    m.forgetSelection(DOC_MAP);
    m.clearMapHistory();
    m.overDirty = true;
    m.fitScale();
    m.needRepaint = true;
    if (remember) rememberFile(path, true);
    if (!r.warning.empty() && !quiet) showMessage("Карта открыта с замечанием", r.warning);
    return true;
}

bool Editor::saveMapTo(const std::string& path) {
    if (path.empty()) return false;
    if (!mapSave(path, m.map)) {
        errorSave("Карта не сохранена",
                  "Не удалось записать файл " + path + ". Проверьте права на запись.");
        return false;
    }
    m.currentMapFile = path;
    m.mapModified = false;
    rememberFile(path, true);
    return true;
}

/// Правка строки объекта. Хвост строки правится как есть: так ничего не
/// теряется и доступны все возможности формата.
void Editor::editObject(int idx) {
    if (idx < 0 || idx >= static_cast<int>(m.map.objects.size())) return;
    const auto k = static_cast<std::size_t>(idx);
    std::string rest = m.map.objects[k].rest;
    if (askText("Объект " + m.map.objects[k].kind,
                "Строка целиком: " + mapObjectLine(m.map.objects[k]), rest, rest)) {
        m.pushUndo();
        m.map.objects[k].rest = netTrim(rest);
        m.needRepaint = true;
    }
}

// ---------------------------------------------------------------------------
// Команды
// ---------------------------------------------------------------------------

bool Editor::commandSaveAs() {
    if (m.docMode == DOC_MAP) {
        std::string suggest = fileNameOnly(m.currentMapFile);
        if (suggest.empty()) suggest = "location.map";
        const std::string path = askSaveMap("Сохранить карту", suggest);
        return !path.empty() && saveMapTo(path);
    }
    std::string suggest = fileNameOnly(m.currentFile);
    if (suggest.empty()) suggest = m.multiCel() ? "sprite.tjt" : "tile.png";
    const std::string path = askSaveFile("Сохранить тайл", suggest);
    return !path.empty() && saveTileTo(path);
}

bool Editor::commandSave() {
    if (m.docMode == DOC_MAP)
        return m.currentMapFile.empty() ? commandSaveAs() : saveMapTo(m.currentMapFile);
    return m.currentFile.empty() ? commandSaveAs() : saveTileTo(m.currentFile);
}

std::string Editor::docName(int mode) const {
    const bool map = mode == DOC_MAP;
    const std::string& file = map ? m.currentMapFile : m.currentFile;
    if (file.empty()) return map ? "новая карта" : "новый тайл";
    return "«" + fileNameOnly(file) + "» (" + (map ? "карта" : "тайл") + ")";
}

std::string Editor::unsavedText(int mode) const {
    // У оригинала было «в документе», и при выходе с тайлом и картой вопрос
    // звучал дважды одними словами (UX-22).
    if (classicUi_) return "В документе есть изменения, которых нет в файле.";
    const bool map = mode == DOC_MAP;
    const std::string& file = map ? m.currentMapFile : m.currentFile;
    const std::string where = file.empty() ? (map ? "новой карте" : "новом тайле") : docName(mode);
    return "В " + where + " есть изменения, которых нет в файле.";
}

/// Вопрос о несохранённых изменениях. false — операцию нужно отменить.
bool Editor::confirmDiscard(const std::string& what) {
    if (!m.isModified()) return true;
    const int answer = ask3("Несохранённые изменения", unsavedText(m.docMode) + " " + what,
                            "Сохранить", "Не сохранять", "Отмена");
    if (answer == 1) return commandSave();
    return answer == 0;
}

bool Editor::saveDocument(int mode) {
    const int was = m.docMode;
    m.docMode = mode;
    const bool ok = commandSave();
    m.docMode = was;
    return ok;
}

void Editor::commandNew() {
    if (m.docMode == DOC_MAP) {
        if (!confirmDiscard("Создать новую карту?")) return;
        int nw = cfg.defaultMapWidth;
        int nh = cfg.defaultMapHeight;
        if (askNewSize("Создать новую карту", nw, nh, kMapMinSide, kMapMaxSide))
            m.newMapOfSize(nw, nh);
    } else {
        if (!confirmDiscard("Создать новый тайл?")) return;
        int nw = cfg.defaultWidth;
        int nh = cfg.defaultHeight;
        int bg = m.blankColor().a == 0 ? 0 : 1;
        if (askNewSize("Создать новый тайл", nw, nh, MIN_TILE, MAX_TILE, &bg)) {
            // Фон, выбранный при создании, — часть нового документа, а не
            // правка: истории и пометки «изменён» он не даёт.
            const Color c = bg == 0 ? kClear : bg == 1 ? kWhite : m.secondColor;
            m.setTile(
                Grid<Color>(clampI(nw, MIN_TILE, MAX_TILE), clampI(nh, MIN_TILE, MAX_TILE), c), "");
        }
    }
}

void Editor::commandOpen() {
    if (m.docMode == DOC_MAP) {
        if (!confirmDiscard("Открыть другую карту?")) return;
        const std::string path = askOpenMap("Открыть карту локации");
        if (!path.empty()) loadMapFrom(path, false, true);
    } else {
        if (!confirmDiscard("Открыть другой файл?")) return;
        const std::string path = askOpenFile("Открыть тайл");
        if (!path.empty()) loadTileFrom(path, false, true);
    }
}

void Editor::commandOpenLast() {
    const std::string path = resolvePath(m.docMode == DOC_MAP ? cfg.lastMapFile : cfg.lastFile);
    if (!fileExists(path)) {
        showMessage("Файл недоступен", "Последний файл не найден: " + path);
        return;
    }
    if (!confirmDiscard("Открыть последний файл?")) return;
    if (m.docMode == DOC_MAP) {
        loadMapFrom(path, false, true);
    } else {
        loadTileFrom(path, false, true);
    }
}

void Editor::commandSaveSelection() {
    // Оригинал в режиме карты копировал выделение в буфер клеток, а в файл
    // писал буфер тайла — никак не связанную с картой картинку.
    if (m.docMode == DOC_MAP) {
        showMessage(
            "Только для тайла",
            "Отдельным файлом сохраняется выделение тайла. Выделение карты копирует клетки внутри "
            "редактора: вставьте их щелчком инструмента выделения или клавишей V.");
        return;
    }
    if (m.hasSelection && m.selW > 0 && m.selH > 0) m.copySelectionToBuffer();
    if (m.buffer.empty()) {
        showMessage(
            "Нечего сохранять",
            "Сначала выделите область инструментом выделения или возьмите тайл из тайлсета.");
        return;
    }
    const std::string path = askSaveFile("Сохранить выделение", "selection.png");
    if (!path.empty()) saveGridTo(m.buffer, path);
}

void Editor::quitApp() {
    saveConfig();
    quitNow_ = true;
}

void Editor::commandExit() {
    if (quitNow_) return;
    if (!classicUi_) {
        // Один вопрос на все несохранённые документы, с их именами.
        std::vector<int> dirty;
        if (m.tileModified) dirty.push_back(DOC_TILE);
        if (m.mapModified) dirty.push_back(DOC_MAP);
        if (!dirty.empty()) {
            const bool one = dirty.size() == 1;
            const std::string text =
                one ? unsavedText(dirty[0]) + " Выйти из программы?"
                    : "Изменения, которых нет в файлах, есть в двух документах: " +
                          docName(DOC_TILE) + " и " + docName(DOC_MAP) + ". Выйти из программы?";
            const int answer = ask3("Несохранённые изменения", text,
                                    one ? "Сохранить" : "Сохранить всё", "Не сохранять", "Отмена");
            if (answer < 0) return;
            if (answer == 1) {
                for (const int d : dirty) {
                    if (!saveDocument(d)) return;
                }
            }
        }
        quitApp();
        return;
    }
    if (!confirmDiscard("Выйти из программы?")) return;
    // Второй документ тоже мог остаться несохранённым — спрашиваем о нём.
    const int mode = m.docMode;
    const bool other = mode == DOC_MAP ? m.tileModified : m.mapModified;
    if (other) {
        m.docMode = mode == DOC_MAP ? DOC_TILE : DOC_MAP;
        const bool ok = confirmDiscard("Выйти из программы?");
        m.docMode = mode;
        if (!ok) return;
    }
    quitApp();
}

}  // namespace tjtm
