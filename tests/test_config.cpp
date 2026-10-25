// Настройки переноса против эталона TJTMConfig: те же входные файлы,
// тот же отчёт о прочитанных значениях, та же запись файла.

#include <gtest/gtest.h>

#include <algorithm>
#include <filesystem>

#include "core/config.h"
#include "core/fsutil.h"
#include "golden_util.h"

using namespace tjtm;

namespace {

const std::string kGolden = std::string(TJTM_SOURCE_DIR) + "/tests/golden/";

void dump(std::vector<std::string>& out, const std::string& name, bool known, const Config& c) {
    auto b = [](bool v) { return std::string(v ? "1" : "0"); };
    out.push_back("file " + name);
    out.push_back("known " + b(known));
    out.push_back("showHelpOnStart " + b(c.showHelpOnStart));
    out.push_back("showMenuOnStart " + b(c.showMenuOnStart));
    out.push_back("defaultWidth " + std::to_string(c.defaultWidth));
    out.push_back("defaultHeight " + std::to_string(c.defaultHeight));
    out.push_back("showGrid " + b(c.showGrid));
    out.push_back("showChecker " + b(c.showChecker));
    out.push_back("startupImage [" + c.startupImage + "]");
    out.push_back("autoLoadLast " + b(c.autoLoadLast));
    out.push_back("lastFile [" + c.lastFile + "]");
    out.push_back("lastFolder [" + c.lastFolder + "]");
    out.push_back("paletteFile [" + c.paletteFile + "]");
    out.push_back("showSeamless " + b(c.showSeamless));
    out.push_back("startInMapMode " + b(c.startInMapMode));
    out.push_back("defaultMapWidth " + std::to_string(c.defaultMapWidth));
    out.push_back("defaultMapHeight " + std::to_string(c.defaultMapHeight));
    out.push_back("mapsFolder [" + c.mapsFolder + "]");
    out.push_back("lastMapFile [" + c.lastMapFile + "]");
    out.push_back("tilesFolder [" + c.tilesFolder + "]");
    out.push_back("iconsFolder [" + c.iconsFolder + "]");
}

std::vector<std::string> fileLines(const std::string& path) {
    std::vector<std::string> v;
    const auto data = readFile(path);
    if (!data) return v;
    std::string cur;
    for (char ch : *data) {
        if (ch == '\n') {
            v.push_back(cur);
            cur.clear();
        } else if (ch != '\r') {
            cur.push_back(ch);
        }
    }
    if (!cur.empty()) v.push_back(cur);
    return v;
}

}  // namespace

TEST(Config, ParsesLikeOriginal) {
    std::vector<std::string> got;
    std::vector<std::string> names;
    for (const auto& n : listFiles(kGolden + "config_in")) {
        if (pathExtLower(n) == ".txt") names.push_back(n);
    }
    std::sort(names.begin(), names.end());  // StringComparer.Ordinal
    for (const auto& n : names) {
        Config c;
        const bool known = configLoad(kGolden + "config_in/" + n, c);
        dump(got, n, known, c);
    }
    Config c;
    const bool missing = configLoad(kGolden + "config_in/нет-такого-файла.txt", c);
    dump(got, "missing", missing, c);

    const std::vector<std::string> want = fileLines(kGolden + "config.txt");
    ASSERT_EQ(got.size(), want.size());
    for (std::size_t i = 0; i < want.size(); ++i)
        EXPECT_EQ(got[i], want[i]) << "строка " << i + 1;
}

// Оригинал на Windows пишет CRLF, эталон снят под Mono с LF; сравнение
// идёт без учёта перевода строки, а сам CRLF проверяется отдельно.
TEST(Config, SavesLikeOriginal) {
    const std::string def = Config{}.serialize();
    ASSERT_EQ(def.substr(0, 3), "\xEF\xBB\xBF");
    EXPECT_NE(def.find("\r\n"), std::string::npos);

    std::string lf;
    for (char ch : def) {
        if (ch != '\r') lf.push_back(ch);
    }
    EXPECT_EQ(lf, *readFile(kGolden + "config_saved_default.txt"));

    Config mixed;
    configLoad(kGolden + "config_in/02_mixed.txt", mixed);
    std::string lf2;
    for (char ch : mixed.serialize()) {
        if (ch != '\r') lf2.push_back(ch);
    }
    EXPECT_EQ(lf2, *readFile(kGolden + "config_saved_mixed.txt"));
}

TEST(Config, SaveLoadRoundTrip) {
    const auto dir = std::filesystem::temp_directory_path() / "tjtm_config_test";
    std::filesystem::create_directories(dir);
    const std::string path = (dir / "Config.txt").string();
    Config a;
    a.defaultWidth = 100;
    a.tilesFolder = "C:/игра/data/tiles";
    a.showGrid = true;
    ASSERT_TRUE(configSave(path, a));
    Config b;
    ASSERT_TRUE(configLoad(path, b));
    EXPECT_EQ(b.defaultWidth, 100);
    EXPECT_EQ(b.tilesFolder, a.tilesFolder);
    EXPECT_TRUE(b.showGrid);
    std::filesystem::remove_all(dir);
}
