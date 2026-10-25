// Карты переноса против эталона TJTMMap и против самой игры: отчёт о
// разборе тех же файлов совпадает с оригиналом, а все карты Andors-Love
// после чтения и записи совпадают с собой байт в байт.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdlib>
#include <iomanip>
#include <sstream>

#include "core/fsutil.h"
#include "core/mapdoc.h"
#include "golden_util.h"

using namespace tjtm;

namespace {

const std::string kGolden = std::string(TJTM_SOURCE_DIR) + "/tests/golden/";

std::string hexOf(const std::string& s) {
    std::ostringstream o;
    for (char ch : s)
        o << std::hex << std::setw(2) << std::setfill('0') << int(static_cast<unsigned char>(ch));
    return o.str();
}

void dumpMap(std::vector<std::string>& out, const MapDoc& m) {
    out.push_back("name [" + m.name + "]");
    out.push_back("size " + std::to_string(m.w) + " " + std::to_string(m.h));
    for (int y = 0; y < m.h; ++y) {
        std::string row;
        for (int x = 0; x < m.w; ++x)
            row.push_back(mapTileChar(m.at(x, y)));
        out.push_back("row " + row);
    }
    for (const auto& o : m.objects) {
        out.push_back("obj [" + mapObjectLine(o) + "] kind=[" + o.kind + "] rest=[" + o.rest + "]");
    }
    for (const auto& v : m.validate())
        out.push_back("valid [" + v + "]");
    out.push_back("saved " + hexOf(m.serialize()));
}

std::vector<std::string> fileLines(const std::string& path) {
    std::vector<std::string> v;
    std::istringstream is(*readFile(path));
    std::string line;
    while (std::getline(is, line))
        v.push_back(line);
    return v;
}

}  // namespace

TEST(Map, ParsesLikeOriginal) {
    std::vector<std::string> got;
    std::vector<std::string> names;
    for (const auto& n : listFiles(kGolden + "map_in")) {
        if (pathExtLower(n) == ".map") names.push_back(n);
    }
    std::sort(names.begin(), names.end());

    // Где перенос намеренно расходится с оригиналом (docs/fixes.md):
    // 14 — size после grid. Оригинал принимал файл с сеткой одного
    // размера и заявленным другим и падал на первой же клетке; перенос
    // такой файл отвергает.
    std::vector<std::string> want = fileLines(kGolden + "map.txt");

    for (const auto& n : names) {
        got.push_back("file " + n);
        MapDoc m;
        m.reset(10, 10);
        const MapParseResult r = mapLoad(kGolden + "map_in/" + n, m);
        if (n == "14_size_after_grid.map") {
            EXPECT_FALSE(r.ok);
            // Пропускаем строки эталона по этому файлу.
            auto it = std::find(want.begin(), want.end(), "file " + n);
            ASSERT_NE(it, want.end());
            auto next = std::find_if(it + 1, want.end(),
                                     [](const std::string& s) { return s.starts_with("file "); });
            want.erase(it + 1, next);
            continue;
        }
        got.push_back(std::string("ok ") + (r.ok ? "1" : "0"));
        got.push_back("error [" + r.error + "]");
        got.push_back("warning [" + r.warning + "]");
        if (r.ok) dumpMap(got, m);
    }

    MapDoc m;
    ASSERT_TRUE(mapLoad(kGolden + "map_in/11_validate.map", m).ok);
    got.push_back("file resize-3x3");
    m.resize(3, 3);
    dumpMap(got, m);
    got.push_back("file resize-8x6");
    m.resize(8, 6);
    dumpMap(got, m);
    got.push_back("file objects");
    m.addObject("bed", 2, 2, "");
    m.addObject("npc", 2, 2, "elder");
    got.push_back("objectAt " + std::to_string(m.objectAt(2, 2)) + " " +
                  std::to_string(m.objectAt(0, 0)) + " " + std::to_string(m.objectAt(7, 5)));
    m.deleteObject(0);
    m.deleteObject(-1);
    m.deleteObject(100);
    dumpMap(got, m);
    got.push_back("file new-2x300");
    m.reset(2, 300);
    dumpMap(got, m);

    ASSERT_EQ(got.size(), want.size());
    for (std::size_t i = 0; i < want.size(); ++i)
        EXPECT_EQ(got[i], want[i]) << "строка " << i + 1;
}

TEST(Map, FailedLoadKeepsDocument) {
    MapDoc m;
    m.reset(8, 6);
    m.addObject("npc", 2, 2, "elder");
    const MapDoc before = m;
    EXPECT_FALSE(mapLoad(kGolden + "map_in/05_truncated.map", m).ok);
    EXPECT_EQ(m, before);
}

// Договор с игрой: каждая её карта после чтения и записи — тот же файл.
TEST(Map, GameCorpusRoundTripsByteForByte) {
    const char* dir = std::getenv("ANDORS_LOVE_DIR");
    if (dir == nullptr || !dirExists(std::string(dir) + "/data/maps")) {
        GTEST_SKIP() << "ANDORS_LOVE_DIR не задан: корпус игры не подключён";
    }
    const std::string maps = std::string(dir) + "/data/maps/";
    int n = 0;
    for (const auto& name : listFiles(maps)) {
        if (pathExtLower(name) != ".map") continue;
        const auto data = readFile(maps + name);
        ASSERT_TRUE(data.has_value()) << name;
        MapDoc m;
        const MapParseResult r = mapParse(*data, m);
        ASSERT_TRUE(r.ok) << name << ": " << r.error;
        EXPECT_TRUE(r.warning.empty()) << name << ": " << r.warning;
        EXPECT_EQ(m.serialize(), *data) << name;
        EXPECT_TRUE(m.validate().empty()) << name;
        ++n;
    }
    EXPECT_GE(n, 50);
}
