#include "core/config.h"

#include "core/fsutil.h"
#include "core/pmath.h"
#include "core/text.h"

namespace tjtm {

const char* ruBool(bool b) {
    return b ? "да" : "нет";
}

namespace {

// Понимает русские и английские написания; нераспознанное — def, а не сбой.
bool parseBool(const std::string& s0, bool def) {
    const std::string s = netToLower(netTrim(s0));
    if (s == "да" || s == "yes" || s == "true" || s == "1" || s == "вкл") return true;
    if (s == "нет" || s == "no" || s == "false" || s == "0" || s == "выкл") return false;
    return def;
}

// Мусор в файле даёт def, а не исключение; затем ограничение диапазона.
int parseInt(const std::string& s, int def, int lo, int hi) {
    int v = 0;
    if (!parseNetInt(netTrim(s), v)) v = def;
    return clampI(v, lo, hi);
}

}  // namespace

int Config::applyLines(const std::vector<std::string>& lines) {
    int n = 0;
    for (const std::string& raw : lines) {
        const std::string s = netTrim(stripBom(raw));
        if (s.empty()) continue;
        if (s[0] == '#' || s[0] == ';') continue;

        const std::size_t p = s.find('=');
        if (p == std::string::npos || p == 0) continue;
        const std::string key = netToLower(netTrim(s.substr(0, p)));
        const std::string val = netTrim(s.substr(p + 1));

        if (key == "showhelponstart") {
            showHelpOnStart = parseBool(val, showHelpOnStart);
        } else if (key == "showmenuonstart") {
            showMenuOnStart = parseBool(val, showMenuOnStart);
        } else if (key == "defaulttilewidth") {
            defaultWidth = parseInt(val, defaultWidth, kCfgMinTile, kCfgMaxTile);
        } else if (key == "defaulttileheight") {
            defaultHeight = parseInt(val, defaultHeight, kCfgMinTile, kCfgMaxTile);
        } else if (key == "showgrid") {
            showGrid = parseBool(val, showGrid);
        } else if (key == "showchecker") {
            showChecker = parseBool(val, showChecker);
        } else if (key == "startupimage") {
            startupImage = val;
        } else if (key == "autoloadlastfile") {
            autoLoadLast = parseBool(val, autoLoadLast);
        } else if (key == "lastfile") {
            lastFile = val;
        } else if (key == "lastfolder") {
            lastFolder = val;
        } else if (key == "palettefile") {
            paletteFile = val;
        } else if (key == "showseamless") {
            showSeamless = parseBool(val, showSeamless);
        } else if (key == "startinmapmode") {
            startInMapMode = parseBool(val, startInMapMode);
        } else if (key == "defaultmapwidth") {
            defaultMapWidth = parseInt(val, defaultMapWidth, kCfgMinTile, 256);
        } else if (key == "defaultmapheight") {
            defaultMapHeight = parseInt(val, defaultMapHeight, kCfgMinTile, 256);
        } else if (key == "mapsfolder") {
            mapsFolder = val;
        } else if (key == "lastmapfile") {
            lastMapFile = val;
        } else if (key == "tilesfolder") {
            tilesFolder = val;
        } else if (key == "iconsfolder") {
            iconsFolder = val;
        } else if (key == "tooltips") {
            tooltips = parseBool(val, tooltips);
        } else if (key == "cellframe") {
            cellFrame = parseBool(val, cellFrame);
        } else if (key == "recentfiles") {
            // Пути через «|»: в путях Windows и Linux его не бывает.
            recentFiles.clear();
            std::size_t a = 0;
            while (a <= val.size() && recentFiles.size() < kRecentMax) {
                const std::size_t b = val.find('|', a);
                const std::string one =
                    netTrim(val.substr(a, b == std::string::npos ? std::string::npos : b - a));
                if (!one.empty()) recentFiles.push_back(one);
                if (b == std::string::npos) break;
                a = b + 1;
            }
        } else if (key == "theme") {
            const std::string t = netToLower(val);
            if (t == "classic") {
                theme = "классика";
            } else if (t == "light") {
                theme = "светлая";
            } else if (t == "dark" || t == "темная") {
                theme = "тёмная";
            } else {
                theme = t;
            }
        } else {
            continue;
        }
        ++n;
    }
    return n;
}

bool configLoad(const std::string& fileName, Config& cfg) {
    cfg = Config{};
    if (fileName.empty()) return false;
    const auto data = readFile(fileName);
    if (!data) return false;

    int known = cfg.applyLines(splitNetLines(*data));
    // Файл от прежних версий мог быть сохранён в CP1251: пробуем ещё раз.
    if (known == 0) known = cfg.applyLines(splitNetLines(cp1251ToUtf8(*data)));
    return known > 0;
}

std::string Config::serialize() const {
    std::string out = "\xEF\xBB\xBF";
    auto add = [&out](const std::string& s) {
        out += s;
        out += "\r\n";
    };
    add("# True Joint Tile Maker — файл настроек.");
    add("# Формат: ключ = значение. Строки с # или ; в начале игнорируются.");
    add("# Логические значения: да / нет.");
    add("# Файл сохраняется в кодировке UTF-8.");
    add("");
    add("# Показывать справку при запуске.");
    add(std::string("ShowHelpOnStart = ") + ruBool(showHelpOnStart));
    add("");
    add("# Показывать главное меню при запуске.");
    add(std::string("ShowMenuOnStart = ") + ruBool(showMenuOnStart));
    add("");
    add("# Размер нового тайла по умолчанию, " + std::to_string(kCfgMinTile) + ".." +
        std::to_string(kCfgMaxTile) + ".");
    add("# Используется только когда стартовое изображение не загрузилось.");
    add("DefaultTileWidth = " + std::to_string(defaultWidth));
    add("DefaultTileHeight = " + std::to_string(defaultHeight));
    add("");
    add("# Показывать сетку между пикселями.");
    add(std::string("ShowGrid = ") + ruBool(showGrid));
    add("");
    add("# Показывать шахматку под полупрозрачными пикселями.");
    add(std::string("ShowChecker = ") + ruBool(showChecker));
    add("");
    add("# Рисовать тайл повторяющимся, чтобы сразу видеть швы.");
    add(std::string("ShowSeamless = ") + ruBool(showSeamless));
    add("");
    add("# Запускаться сразу в режиме карты локации.");
    add(std::string("StartInMapMode = ") + ruBool(startInMapMode));
    add("");
    add("# Размер новой карты локации, 4..256.");
    add("DefaultMapWidth = " + std::to_string(defaultMapWidth));
    add("DefaultMapHeight = " + std::to_string(defaultMapHeight));
    add("");
    add("# Папка с картами локаций, например data/maps игры Andors-Love.");
    add("MapsFolder = " + mapsFolder);
    add("");
    add("# Папка с набором тайлов, например data/tiles игры Andors-Love.");
    add("# Экран набора открывает и сохраняет тайлы прямо в ней.");
    add("TilesFolder = " + tilesFolder);
    add("");
    add("# Папка со значками кнопок редактора. Пусто — кнопки с подписями.");
    add("IconsFolder = " + iconsFolder);
    add("");
    add("# Изображение, загружаемое при запуске. Пусто — не загружать.");
    add("# Размер тайла берётся из самого изображения.");
    add("StartupImage = " + startupImage);
    add("");
    add("# Открывать последний файл вместо стартового изображения.");
    add(std::string("AutoLoadLastFile = ") + ruBool(autoLoadLast));
    add("");
    add("# Файл палитры 8x8, загружаемый при запуске.");
    add("PaletteFile = " + paletteFile);
    add("");
    const Config def;
    if (theme != def.theme) {
        add("# Тема окна: светлая, тёмная или классика (облик 1.6).");
        add("Theme = " + theme);
        add("");
    }
    if (tooltips != def.tooltips) {
        add("# Всплывающие подсказки при наведении.");
        add(std::string("Tooltips = ") + ruBool(tooltips));
        add("");
    }
    if (!recentFiles.empty()) {
        std::string list;
        for (const std::string& f : recentFiles) {
            if (!list.empty()) list += '|';
            list += f;
        }
        add("# Недавние файлы, последний — первым. Заполняется программой.");
        add("RecentFiles = " + list);
        add("");
    }
    if (cellFrame != def.cellFrame) {
        add("# Рамка клетки под курсором на холсте.");
        add(std::string("CellFrame = ") + ruBool(cellFrame));
        add("");
    }
    add("# Заполняется программой.");
    add("LastFile = " + lastFile);
    add("LastMapFile = " + lastMapFile);
    add("LastFolder = " + lastFolder);
    return out;
}

void Config::addRecent(const std::string& path) {
    if (path.empty()) return;
    std::erase(recentFiles, path);
    recentFiles.insert(recentFiles.begin(), path);
    if (recentFiles.size() > kRecentMax) recentFiles.resize(kRecentMax);
}

bool configSave(const std::string& fileName, const Config& cfg) {
    if (fileName.empty()) return false;
    return writeFile(fileName, cfg.serialize());
}

}  // namespace tjtm
