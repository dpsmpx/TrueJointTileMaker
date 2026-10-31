// Настройки: чтение и запись Config.txt, перенос TJTMConfig.
//
// Формат — «ключ = значение», по строке на параметр, порядок не важен.
// Строки с # или ; в начале — комментарии. Файл в UTF-8; если в нём не
// узнан ни один ключ, он перечитывается как CP1251 — так писали старые
// версии. Любая ошибка чтения не фатальна: остаются значения по умолчанию.
#pragma once

#include <string>
#include <vector>

namespace tjtm {

/// Допустимый диапазон стороны тайла.
inline constexpr int kCfgMinTile = 4;
inline constexpr int kCfgMaxTile = 512;

struct Config {
    bool showHelpOnStart = false;
    bool showMenuOnStart = true;
    int defaultWidth = 32;
    int defaultHeight = 32;
    bool showGrid = false;
    bool showChecker = true;
    std::string startupImage = "Картинки/debug.png";
    bool autoLoadLast = false;
    std::string lastFile;
    std::string lastFolder;
    std::string paletteFile = "Resources/Palitra.png";
    bool showSeamless = false;
    bool startInMapMode = false;
    /// Размер новой карты совпадает с картами игры.
    int defaultMapWidth = 48;
    int defaultMapHeight = 18;
    std::string mapsFolder;
    std::string lastMapFile;
    std::string tilesFolder;
    /// Своя папка значков, а не общая с игрой: значки — имущество
    /// редактора, и в наборе игры им делать нечего.
    std::string iconsFolder = "Resources/Icons";

    // --- интерфейс 1.7 и дальше (docs/roadmap.md, «Интерфейс — план») ---
    //
    // У оригинала этих ключей нет. В файл они пишутся, только когда
    // отличаются от значений по умолчанию: файл по умолчанию остаётся
    // таким же, как у оригинала.

    /// Тема окна: «светлая», «тёмная» или «классика» (облик 1.6).
    std::string theme = "светлая";
    /// Всплывающие подсказки при наведении.
    bool tooltips = true;
    /// Рамка клетки под курсором на холсте.
    bool cellFrame = true;

    /// Применяет строки файла. Возвращает число распознанных параметров.
    int applyLines(const std::vector<std::string>& lines);
    /// Текст файла настроек вместе с комментариями.
    std::string serialize() const;
};

/// Логическое значение так, как оно пишется в файл.
const char* ruBool(bool b);

/// Читает файл. Сначала сбрасывает настройки к значениям по умолчанию.
/// Возвращает true, если распознан хотя бы один параметр.
bool configLoad(const std::string& fileName, Config& cfg);
/// Пишет файл в UTF-8 с меткой BOM и переводами строки CRLF — так его
/// писал оригинал на Windows (File.WriteAllLines с Encoding.UTF8).
bool configSave(const std::string& fileName, const Config& cfg);

}  // namespace tjtm
