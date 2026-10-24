// Файлы и команды (TJTM.pas: «Файловые операции», «Палитра и тайлсет»,
// «Карты локаций», «Команды»). Каждый вход в файловую систему проверяет
// ошибку: испорченный или чужой файл не должен ронять редактор.

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include <algorithm>
#include <cstdio>

#include "app/editor.h"
#include "core/anim.h"
#include "core/fsutil.h"
#include "core/gif.h"
#include "core/png.h"
#include "core/project.h"
#include "core/text.h"
#include "core/theme.h"

namespace tjtm {

namespace {

// Проект .tjt открывается тем же «Загрузить»: это тоже PNG, только с
// куском проекта внутри.
const std::vector<FileFilter> kPngFilter = {
    {"Картинки, анимации и проекты (PNG, GIF, TJT)", "png;gif;tjt"},
    {"Проекты редактора", "tjt"},
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
        if (!quiet) showMessage("Файл не найден", path);
        return false;
    }
    const auto data = readFile(path);
    if (!data) {
        if (!quiet) showMessage("Не удалось открыть файл", path);
        return false;
    }
    std::string err;
    Grid<Color> img;
    const std::vector<std::uint8_t> bytes(data->begin(), data->end());
    bool ok = false;
    if (bytes.size() >= 8 && bytes[0] == 137 && bytes[1] == 'P' && bytes[2] == 'N' &&
        bytes[3] == 'G') {
        ok = pngDecode(bytes, img, err);
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
        if (!quiet) showMessage("Не удалось открыть файл", err);
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
            if (!quiet) showMessage("Проект не открыт", path + ": " + err);
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
        showMessage("Не удалось сохранить проект", path + ": " + err);
        return false;
    }
    m.currentFile = path;
    m.tileModified = false;
    rememberFile(path, false);
    return true;
}

bool Editor::saveTileTo(const std::string& path) {
    if (path.empty()) return false;
    const int nf = m.doc.frameCount();
    if (pathExtLower(path) == ".tjt") return saveProjectTo(path);
    // Открытый GIF сохраняется в GIF, анимация из одного слоя в PNG — в
    // APNG: так «Сохранить» возвращает файл в том виде, в каком его открыли.
    if (pathExtLower(path) == ".gif") return saveAnimationTo(path, true, 0, nf - 1, 1, 0, true);
    std::string err;
    if (m.multiCel() && m.doc.layerCount() == 1)
        return saveAnimationTo(path, false, 0, nf - 1, 1, 0, true);
    if (m.multiCel()) {
        // Слои в PNG не помещаются: либо проект, либо сведённая картинка
        // (у анимации — APNG). Тогда документ остаётся несохранённым.
        const int a = ask3("Слои и кадры",
                           nf > 1 ? "PNG хранит картинки, а не слои: кадры сведутся в анимацию "
                                    "APNG. Слои, кадры и метки сохраняет проект .tjt."
                                  : "PNG хранит одну картинку: слои сведутся. Слои, кадры и метки "
                                    "сохраняет проект .tjt.",
                           "Сохранить проект", "Сохранить картинку", "Отмена");
        if (a < 0) return false;
        if (a == 1) return saveProjectTo(replaceExtension(path, ".tjt"));
        return saveAnimationTo(path, false, 0, nf - 1, 1, 0, false);
    }
    if (!pngSave(path, m.tile, err)) {
        showMessage("Не удалось сохранить файл", path + ": " + err);
        return false;
    }
    m.currentFile = path;
    m.tileModified = false;
    rememberFile(path, false);
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
            if (!quiet) showMessage("Не удалось открыть GIF", path + ": " + err);
            return -1;
        }
    } else if (pngFindChunk(bytes, "acTL", actl)) {
        if (!apngDecode(bytes, frames, err)) {
            if (!quiet) showMessage("Не удалось открыть APNG", path + ": " + err);
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

bool Editor::saveAnimationTo(const std::string& path, bool gif, int from, int to, int scale,
                             int plays, bool asDocument) {
    if (path.empty()) return false;
    const std::vector<AnimFrame> frames = scaleFrames(m.animationFrames(from, to), scale);
    if (frames.empty()) return false;
    std::vector<std::uint8_t> bytes;
    if (gif) {
        // Цвета палитры идут в GIF первыми и в её порядке.
        bytes = gifEncode(frames, plays, m.palette);
    } else if (frames.size() > 1) {
        bytes = apngEncode(frames, plays);
    } else {
        bytes = pngEncode(frames.front().image);
    }
    if (!writeFile(path,
                   std::string_view(reinterpret_cast<const char*>(bytes.data()), bytes.size()))) {
        showMessage("Не удалось сохранить файл",
                    path + ": не удалось записать файл. Проверьте права на запись.");
        return false;
    }
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
    int cols = 0;
    int margin = 0;
    int spacing = 0;
    int scale = 1;
    if (!askNumbers("Лист спрайтов",
                    "Столбцов 0 — все кадры в строку. Рядом ляжет описание .json для движка",
                    {{"С кадра", &first, 1, nf},
                     {"По кадр", &last, 1, nf},
                     {"Столбцов", &cols, 0, nf},
                     {"Поле вокруг", &margin, 0, 64},
                     {"Промежуток", &spacing, 0, 64},
                     {"Увеличение", &scale, 1, 8}},
                    "Дальше…", nullptr))
        return;
    if (first > last) std::swap(first, last);
    const std::string path = askExportFile("Сохранить лист спрайтов",
                                           exportBase(m.currentFile) + "_sheet.png", "png", "PNG");
    if (path.empty()) return;
    SheetOptions o;
    o.layout = cols == 0 ? SHEET_ROW : SHEET_GRID;
    o.columns = cols;
    o.margin = margin;
    o.spacing = spacing;
    o.scale = scale;
    const SpriteSheet sheet = buildSheet(m.animationFrames(first - 1, last - 1), o);
    std::string err;
    if (!pngSave(path, sheet.image, err)) {
        showMessage("Не удалось сохранить лист", path + ": " + err);
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
        showMessage("Не удалось сохранить описание", jsonPath + ": не удалось записать файл.");
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
    if (!path.empty()) saveAnimationTo(path, gif, first - 1, last - 1, scale, plays, false);
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
    if (!failed.empty()) showMessage("Не удалось сохранить кадр", failed + ": " + err);
}

bool Editor::saveGridTo(const Grid<Color>& g, const std::string& path) {
    if (path.empty() || g.empty()) return false;
    std::string err;
    if (!pngSave(path, g, err)) {
        showMessage("Не удалось сохранить файл", path + ": " + err);
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
        if (!quiet) showMessage("Файл не найден", path);
        return false;
    }
    const MapParseResult r = mapLoad(path, m.map);
    if (!r.ok) {
        if (!quiet) showMessage("Карта не прочитана", r.error);
        return false;
    }
    m.currentMapFile = path;
    m.mapModified = false;
    m.docMode = DOC_MAP;
    m.resetSelection();
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
        showMessage("Карта не сохранена",
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

/// Вопрос о несохранённых изменениях. false — операцию нужно отменить.
bool Editor::confirmDiscard(const std::string& what) {
    if (!m.isModified()) return true;
    const int answer =
        ask3("Несохранённые изменения", "В документе есть изменения, которых нет в файле. " + what,
             "Сохранить", "Не сохранять", "Отмена");
    if (answer == 1) return commandSave();
    return answer == 0;
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
        if (askNewSize("Создать новый тайл", nw, nh, MIN_TILE, MAX_TILE)) m.newTileOfSize(nw, nh);
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
