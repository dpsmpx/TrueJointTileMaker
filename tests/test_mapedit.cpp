// Этап 10: форма объекта карты, кисть смесью местностей, замена местности,
// поиск объектов.

#include <gtest/gtest.h>

#include <cstdlib>

#include "core/fsutil.h"
#include "core/mapdoc.h"
#include "core/mapobj.h"
#include "core/model.h"

namespace tjtm {
namespace {

Model mapModel(int w = 16, int h = 10) {
    Model m;
    m.switchMode(DOC_MAP);
    m.map.reset(w, h);
    return m;
}

std::string valueOf(const std::vector<mapobj::Field>& f, const std::string& label) {
    for (const mapobj::Field& x : f) {
        if (x.label == label) return x.value;
    }
    return "<нет поля>";
}

// Каждый объект каждой карты игры: форма разбирает его по грамматике игры,
// не находит ошибок и собирает обратно ту же строку.
TEST(ObjectForm, GameCorpusParsesValidAndRoundTrips) {
    const char* dir = std::getenv("ANDORS_LOVE_DIR");
    if (dir == nullptr || !dirExists(std::string(dir) + "/data/maps")) {
        GTEST_SKIP() << "ANDORS_LOVE_DIR не задан: корпус игры не подключён";
    }
    const std::string maps = std::string(dir) + "/data/maps/";
    int objects = 0;
    for (const auto& name : listFiles(maps)) {
        if (pathExtLower(name) != ".map") continue;
        const auto data = readFile(maps + name);
        ASSERT_TRUE(data.has_value()) << name;
        MapDoc m;
        ASSERT_TRUE(mapParse(*data, m).ok) << name;
        for (const MapObject& o : m.objects) {
            const std::vector<mapobj::Field> f = mapobj::fieldsOf(o.kind, o.rest);
            EXPECT_EQ(mapobj::restOf(f), o.rest) << name << ": " << mapObjectLine(o);
            EXPECT_EQ(mapobj::checkFields(o.kind, f), "") << name << ": " << mapObjectLine(o);
            for (const mapobj::Field& x : f)
                EXPECT_NE(x.label, "Прочее") << name << ": " << mapObjectLine(o);
            ++objects;
        }
    }
    EXPECT_GT(objects, 300);
}

TEST(ObjectForm, FieldsFollowGameGrammar) {
    auto item = mapobj::fieldsOf("item", "bread");
    EXPECT_EQ(valueOf(item, "Предмет"), "bread");
    EXPECT_EQ(valueOf(item, "Количество"), "");
    EXPECT_EQ(mapobj::restOf(item), "bread");
    EXPECT_EQ(mapobj::checkFields("item", item), "");

    auto exit = mapobj::fieldsOf("exit", "gatehouse 39 8 key=iron deny=Заперто  изнутри.");
    EXPECT_EQ(valueOf(exit, "Локация"), "gatehouse");
    EXPECT_EQ(valueOf(exit, "Y цели"), "8");
    EXPECT_EQ(valueOf(exit, "Условия"), "key=iron deny=Заперто  изнутри.");
    EXPECT_EQ(mapobj::checkFields("exit", exit), "");

    auto sign = mapobj::fieldsOf("sign", "Двери не заперты. В каждой келье прибрано.");
    EXPECT_EQ(sign.size(), 1U);
    EXPECT_EQ(sign[0].value, "Двери не заперты. В каждой келье прибрано.");

    // Слова сверх грамматики не теряются.
    auto npc = mapobj::fieldsOf("npc", "elder лишнее слово");
    EXPECT_EQ(valueOf(npc, "Житель"), "elder");
    EXPECT_EQ(valueOf(npc, "Прочее"), "лишнее слово");
    EXPECT_EQ(mapobj::restOf(npc), "elder лишнее слово");

    EXPECT_TRUE(mapobj::fieldsOf("bed", "").empty());
}

TEST(ObjectForm, ChecksCatchWhatGameRejects) {
    auto spawn = mapobj::fieldsOf("spawn", "rat 0 4");
    EXPECT_NE(mapobj::checkFields("spawn", spawn), "");
    spawn = mapobj::fieldsOf("spawn", "rat два 4");
    EXPECT_NE(mapobj::checkFields("spawn", spawn), "");
    auto chest = mapobj::fieldsOf("chest", "400 - bread:0");
    EXPECT_NE(mapobj::checkFields("chest", chest), "");
    chest = mapobj::fieldsOf("chest", "400 - bread");
    EXPECT_NE(mapobj::checkFields("chest", chest), "");
    chest = mapobj::fieldsOf("chest", "400 - bread:2 salt:1");
    EXPECT_EQ(mapobj::checkFields("chest", chest), "");
    auto exit = mapobj::fieldsOf("exit", "forest 2 8 door=open");
    EXPECT_NE(mapobj::checkFields("exit", exit), "");
    exit = mapobj::fieldsOf("exit", "forest 2 8 quest=main");
    EXPECT_NE(mapobj::checkFields("exit", exit), "");
    exit = mapobj::fieldsOf("exit", "forest 2");
    EXPECT_NE(mapobj::checkFields("exit", exit), "") << "нет Y цели";
    auto npc = mapobj::fieldsOf("npc", "");
    EXPECT_NE(mapobj::checkFields("npc", npc), "");
    // Поле-слово с пробелом — ошибка: игра прочла бы только первое слово.
    auto it = mapobj::fieldsOf("item", "bread 1");
    it[0].value = "black bread";
    EXPECT_NE(mapobj::checkFields("item", it), "");
}

TEST(MapMix, WeightsDecideAndStrokeChangesLayout) {
    Model m = mapModel(32, 32);
    m.mapBrush = MT_GRASS;
    EXPECT_EQ(m.mapBrushAt(3, 3), MT_GRASS);
    m.mapMix = true;
    EXPECT_EQ(m.mapBrushAt(3, 3), MT_GRASS) << "без весов — местность кисти";
    m.mapMixWeights[MT_GRASS] = 3;
    m.mapMixWeights[MT_TREE] = 1;
    int trees = 0;
    int grass = 0;
    std::vector<int> layout;
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x) {
            const int t = m.mapBrushAt(x, y);
            trees += t == MT_TREE ? 1 : 0;
            grass += t == MT_GRASS ? 1 : 0;
            layout.push_back(t);
        }
    }
    std::vector<int> again;
    for (int y = 0; y < 32; ++y) {
        for (int x = 0; x < 32; ++x)
            again.push_back(m.mapBrushAt(x, y));
    }
    EXPECT_EQ(layout, again) << "та же клетка в том же мазке — то же";
    EXPECT_EQ(trees + grass, 32 * 32);
    EXPECT_GT(trees, 32 * 32 / 8);
    EXPECT_LT(trees, 32 * 32 * 3 / 8);
    std::vector<int> first;
    for (int x = 0; x < 32; ++x)
        first.push_back(m.mapBrushAt(x, 0));
    ++m.mapMixSeed;
    std::vector<int> second;
    for (int x = 0; x < 32; ++x)
        second.push_back(m.mapBrushAt(x, 0));
    EXPECT_NE(first, second) << "новый мазок — новая раскладка";
}

TEST(MapMix, PaintAndFillUseTheMix) {
    Model m = mapModel(20, 12);
    m.mapMix = true;
    m.mapBrush = MT_GRASS;
    m.mapMixWeights[MT_GRASS] = 1;
    m.mapMixWeights[MT_TREE] = 1;
    m.pushUndo();
    m.floodFillMap(5, 5, m.mapBrush);
    int trees = 0;
    int floors = 0;
    for (int y = 1; y < 11; ++y) {
        for (int x = 1; x < 19; ++x) {
            trees += m.map.at(x, y) == MT_TREE ? 1 : 0;
            floors += m.map.at(x, y) == MT_FLOOR ? 1 : 0;
        }
    }
    EXPECT_GT(trees, 30);
    EXPECT_EQ(floors, 0) << "заливка накрыла весь пол комнаты";
    EXPECT_EQ(m.map.at(0, 0), MT_WALL) << "стены не тронуты";
}

TEST(MapFind, ReplaceTerrainInMapOrSelection) {
    Model m = mapModel(10, 8);
    EXPECT_EQ(m.replaceMapTerrain(MT_FLOOR, MT_GRASS, false), 8 * 6);
    EXPECT_EQ(m.map.at(4, 4), MT_GRASS);
    m.doUndo();
    EXPECT_EQ(m.map.at(4, 4), MT_FLOOR);
    m.selX = 1;
    m.selY = 1;
    m.selW = 3;
    m.selH = 2;
    m.hasSelection = true;
    EXPECT_EQ(m.replaceMapTerrain(MT_FLOOR, MT_WATER, true), 6);
    EXPECT_EQ(m.map.at(3, 2), MT_WATER);
    EXPECT_EQ(m.map.at(4, 2), MT_FLOOR);
    EXPECT_EQ(m.replaceMapTerrain(MT_ROAD, MT_WATER, false), 0) << "нечего — и шага истории нет";
}

TEST(MapFind, ObjectsByKindOrTextIgnoringCase) {
    Model m = mapModel(10, 8);
    m.map.addObject("npc", 2, 2, "elder");
    m.map.addObject("sign", 3, 3, "Старая Мельница");
    m.map.addObject("chest", 4, 4, "10 - bread:1");
    EXPECT_EQ(m.findMapObjects("").size(), 3U);
    EXPECT_EQ(m.findMapObjects("CHEST"), std::vector<int>{2});
    EXPECT_EQ(m.findMapObjects("мельница"), std::vector<int>{1});
    EXPECT_EQ(m.findMapObjects("ELDER"), std::vector<int>{0});
    EXPECT_TRUE(m.findMapObjects("волк").empty());
}

}  // namespace
}  // namespace tjtm
