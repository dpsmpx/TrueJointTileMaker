// Файлы и команды (TJTM.pas: «Файловые операции», «Палитра и тайлсет»,
// «Карты локаций», «Команды»). Каждый вход в файловую систему проверяет
// ошибку: испорченный или чужой файл не должен ронять редактор.

#include <SDL3/SDL.h>
#include <SDL3_image/SDL_image.h>

#include "app/editor.h"
#include "core/fsutil.h"
#include "core/png.h"
#include "core/text.h"
#include "core/theme.h"

namespace tjtm {

namespace {

const std::vector<FileFilter> kPngFilter = {{"Изображения PNG", "png"}, {"Все файлы", "*"}};
const std::vector<FileFilter> kMapFilter = {{"Карты локаций", "map"}, {"Все файлы", "*"}};

/// Расширение по умолчанию, если в диалоге сохранения его не написали —
/// AddExtension оригинала.
std::string withExtension(const std::string& path, const std::string& ext) {
    if (path.empty() || !pathExtLower(path).empty()) return path;
    return path + "." + ext;
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
    return withExtension(r, "png");
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
    Grid<Color> g;
    if (!readImageGrid(path, g, quiet)) return false;
    m.setTile(std::move(g), path);
    if (remember) rememberFile(path, false);
    return true;
}

bool Editor::saveTileTo(const std::string& path) {
    if (path.empty()) return false;
    std::string err;
    if (!pngSave(path, m.tile, err)) {
        showMessage("Не удалось сохранить файл", path + ": " + err);
        return false;
    }
    m.currentFile = path;
    m.tileModified = false;
    rememberFile(path, false);
    return true;
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
    if (suggest.empty()) suggest = "tile.png";
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
