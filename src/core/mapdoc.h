// Карты локаций Andors-Love: перенос TJTMMap.
//
// Формат разобран по src/world.cpp игры и проверен на всех её картах:
// разбор и обратная запись дают файл байт в байт.
//
//   name  <название>
//   size  <ширина> <высота>
//   grid
//   <ровно height строк ровно по width символов>
//   objects
//   npc|item|exit|spawn|chest|note|bed|sign <x> <y> <остальное>
//   end
//
// Сетка хранит только геометрию: проходимо или нет. Остальное описано
// объектами. У объекта разбираются вид и координаты, а хвост строки
// хранится как есть: редактор не понимает условия перехода и содержимое
// сундуков, но и терять их при пересохранении не должен.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace tjtm {

/// Коды клеток в порядке enum Tile из src/types.h игры.
enum : int {
    MT_FLOOR = 0,
    MT_WALL = 1,
    MT_WATER = 2,
    MT_TREE = 3,
    MT_GRASS = 4,
    MT_ROAD = 5,
    MT_DEADWATER = 6,
    MT_COUNT = 7,
};

/// Границы стороны локации. Игра верхней не задаёт, редактору она нужна.
inline constexpr int kMapMinSide = 4;
inline constexpr int kMapMaxSide = 256;

struct MapObject {
    std::string kind;
    int x = 0;
    int y = 0;
    std::string rest;

    friend bool operator==(const MapObject&, const MapObject&) = default;
};

char mapTileChar(int code);
int mapCharTile(char32_t c);
std::string mapTileName(int code);
bool mapTileWalkable(int code);
int mapKindCount();
std::string mapKindName(int i);
/// Заготовка хвоста строки для нового объекта данного вида.
std::string mapKindTemplate(int i);
bool mapKindKnown(const std::string& kind);
/// Строка объекта в том виде, в каком она попадёт в файл.
std::string mapObjectLine(const MapObject& o);

struct MapDoc {
    std::string name;
    int w = 0;
    int h = 0;
    /// Клетки, по строкам: grid[y * w + x].
    std::vector<std::uint8_t> grid;
    std::vector<MapObject> objects;

    std::uint8_t at(int x, int y) const {
        return grid[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                    static_cast<std::size_t>(x)];
    }
    std::uint8_t& at(int x, int y) {
        return grid[static_cast<std::size_t>(y) * static_cast<std::size_t>(w) +
                    static_cast<std::size_t>(x)];
    }
    bool inside(int x, int y) const { return x >= 0 && y >= 0 && x < w && y < h; }

    /// Пол внутри, стена по краю: локация сразу замкнута, как в картах игры.
    void reset(int nw, int nh);
    /// Меняет размер, сохраняя общую часть. Объекты за новой границей
    /// убираются: игра требует, чтобы все координаты были внутри.
    void resize(int nw, int nh);
    /// Индекс последнего объекта в клетке или -1.
    int objectAt(int x, int y) const;
    void addObject(const std::string& kind, int x, int y, const std::string& rest);
    void deleteObject(int idx);
    /// Текст файла: UTF-8 без BOM, перевод строки LF, как карты игры.
    std::string serialize() const;
    /// Замечания проверки, по одному на строку; пусто — замечаний нет.
    std::vector<std::string> validate() const;

    friend bool operator==(const MapDoc&, const MapDoc&) = default;
};

/// Итог разбора. При ошибке doc не трогается: оригинал портил открытую
/// карту ещё до того, как находил ошибку в файле.
struct MapParseResult {
    bool ok = false;
    std::string error;
    /// Файл открыт, но что-то пришлось поправить.
    std::string warning;
};

MapParseResult mapParse(const std::string& text, MapDoc& doc);
MapParseResult mapLoad(const std::string& fileName, MapDoc& doc);
bool mapSave(const std::string& fileName, const MapDoc& doc);

}  // namespace tjtm
