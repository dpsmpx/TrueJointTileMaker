// Программа целиком: сценарии tests/screens/*.txt без экрана и сверка
// снимков с оригиналом по областям из tests/screens/exact.txt.
//
// Эталонные вырезки сняты с оригинала под Mono тем же сценарием через
// tools/screens.sh. Здесь перенос проходит его сам (--script), на
// драйвере offscreen, и каждая область обязана совпасть до пикселя.

#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "core/model.h"
#include "core/png.h"
#include "core/project.h"

namespace fs = std::filesystem;

namespace {

struct Region {
    std::string scenario, shot;
    int x = 0, y = 0, w = 0, h = 0;
};

std::vector<Region> readRegions() {
    std::vector<Region> out;
    std::ifstream f(std::string(TJTM_SOURCE_DIR) + "/tests/screens/exact.txt");
    std::string line;
    while (std::getline(f, line)) {
        const std::size_t hash = line.find('#');
        if (hash != std::string::npos) line.erase(hash);
        std::istringstream ls(line);
        Region r;
        if (ls >> r.scenario >> r.shot >> r.x >> r.y >> r.w >> r.h) out.push_back(r);
    }
    return out;
}

/// Свежий каталог программы: настройки, ресурсы и картинки из репозитория.
/// Программа пишет в Config.txt, и прогоны не должны влиять друг на друга.
fs::path makeBase(const std::string& name) {
    const fs::path src(TJTM_SOURCE_DIR);
    const fs::path base = fs::temp_directory_path() / ("tjtm_app_test_" + name);
    fs::remove_all(base);
    fs::create_directories(base);
    fs::copy_file(src / "Config.txt", base / "Config.txt");
    fs::copy(src / "Resources", base / "Resources", fs::copy_options::recursive);
    fs::copy(src / fs::path(u8"Картинки"), base / fs::path(u8"Картинки"),
             fs::copy_options::recursive);
    return base;
}

/// Пройти сценарий; снимки — в base/shots.
bool runScenario(const std::string& scenario, const fs::path& base) {
    const fs::path shots = base / "shots";
    fs::create_directories(shots);
    const std::string script = std::string(TJTM_SOURCE_DIR) + "/tests/screens/" + scenario + ".txt";
#ifdef _WIN32
    _putenv_s("SDL_VIDEODRIVER", "offscreen");
    _putenv_s("SDL_FRAMEBUFFER_ACCELERATION", "0");
#else
    setenv("SDL_VIDEODRIVER", "offscreen", 1);
    // Программный кадровый буфер: иначе SDL поднимает EGL, и утечки Mesa
    // в сборке asan достаются программе.
    setenv("SDL_FRAMEBUFFER_ACCELERATION", "0", 1);
#endif
    const std::string cmd = "\"" + std::string(TJTM_APP_EXE) + "\" --base \"" + base.string() +
                            "\" --script \"" + script + "\" --shots \"" + shots.string() + "\"";
#ifdef _WIN32
    // cmd /c срезает первую и последнюю кавычку строки: без внешней пары
    // от пути к программе осталась бы половина.
    return std::system(("\"" + cmd + "\"").c_str()) == 0;
#else
    return std::system(cmd.c_str()) == 0;
#endif
}

class AppScreens : public ::testing::TestWithParam<std::string> {};

TEST_P(AppScreens, MatchOriginal) {
    const std::string scenario = GetParam();
    const fs::path base = makeBase(scenario);
    ASSERT_TRUE(runScenario(scenario, base)) << "сценарий " << scenario << " не прошёл";

    int checked = 0;
    std::map<std::string, tjtm::Grid<tjtm::Color>> shots;
    for (const Region& r : readRegions()) {
        if (r.scenario != scenario) continue;
        if (shots.count(r.shot) == 0) {
            std::string err;
            tjtm::Grid<tjtm::Color> g;
            ASSERT_TRUE(tjtm::pngLoad((base / "shots" / (r.shot + ".png")).string(), g, err))
                << r.shot << ": " << err;
            shots.emplace(r.shot, std::move(g));
        }
        const auto& shot = shots.at(r.shot);
        std::string err;
        tjtm::Grid<tjtm::Color> ref;
        const std::string refPath = std::string(TJTM_SOURCE_DIR) + "/tests/screens/golden/" +
                                    scenario + "/" + r.shot + "_" + std::to_string(r.x) + "_" +
                                    std::to_string(r.y) + ".png";
        ASSERT_TRUE(tjtm::pngLoad(refPath, ref, err)) << refPath << ": " << err;
        ASSERT_EQ(ref.w, r.w);
        ASSERT_EQ(ref.h, r.h);
        int bad = 0;
        int firstX = -1;
        int firstY = -1;
        for (int y = 0; y < r.h; ++y) {
            for (int x = 0; x < r.w; ++x) {
                const tjtm::Color a = ref.at(x, y);
                const tjtm::Color b = shot.at(r.x + x, r.y + y);
                if (a.argb() != b.argb()) {
                    if (bad == 0) {
                        firstX = r.x + x;
                        firstY = r.y + y;
                    }
                    ++bad;
                }
            }
        }
        EXPECT_EQ(bad, 0) << r.shot << " " << r.w << "x" << r.h << "+" << r.x << "+" << r.y
                          << ": первое расхождение в (" << firstX << ", " << firstY << ")";
        ++checked;
    }
    EXPECT_GT(checked, 0) << "для сценария " << scenario << " нет областей в exact.txt";
    fs::remove_all(base);
}

INSTANTIATE_TEST_SUITE_P(Screens, AppScreens,
                         ::testing::Values("tile", "map", "draw", "mapedit", "pro1", "pal2"));

/// Сценарии, которые сверяются только глазами (tools/screens.sh), обязаны
/// хотя бы проходить: экраны открываются и закрываются, программа не падает.
TEST(AppScreens, MenuScenarioRuns) {
    const fs::path base = makeBase("menu");
    ASSERT_TRUE(runScenario("menu", base));
    for (const char* shot :
         {"01_menu", "03_settings", "04_help", "05_colorops", "06_tileset", "07_new"}) {
        EXPECT_TRUE(fs::exists(base / "shots" / (std::string(shot) + ".png"))) << shot;
    }
    fs::remove_all(base);
}

/// Этап 3: «Сохранить как» с расширением .tjt пишет проект, «Загрузить
/// файл» открывает его обратно поверх испорченного рисунка.
TEST(AppScreens, ProjectSaveAndReopen) {
    const fs::path base = makeBase("tjt");
    const fs::path project = base / "sprite.tjt";
    const fs::path answers = base / "dialogs.txt";
    {
        std::ofstream f(answers, std::ios::binary);
        f << project.string() << "\n" << project.string() << "\n";
    }
#ifdef _WIN32
    _putenv_s("TJTM_TEST_DIALOGS", answers.string().c_str());
#else
    setenv("TJTM_TEST_DIALOGS", answers.string().c_str(), 1);
#endif
    const bool ran = runScenario("tjt", base);
#ifdef _WIN32
    _putenv_s("TJTM_TEST_DIALOGS", "");
#else
    unsetenv("TJTM_TEST_DIALOGS");
#endif
    ASSERT_TRUE(ran);

    std::ifstream in(project, std::ios::binary);
    ASSERT_TRUE(in.good()) << "проект не записан";
    const std::vector<std::uint8_t> bytes((std::istreambuf_iterator<char>(in)),
                                          std::istreambuf_iterator<char>());
    EXPECT_TRUE(tjtm::isProjectFile(bytes));
    tjtm::Model m;
    std::string err;
    ASSERT_TRUE(tjtm::projectDecode(bytes, m, err)) << err;

    std::map<std::string, tjtm::Grid<tjtm::Color>> shots;
    for (const char* name : {"01_drawn", "02_reopened"}) {
        tjtm::Grid<tjtm::Color> g;
        ASSERT_TRUE(tjtm::pngLoad((base / "shots" / (std::string(name) + ".png")).string(), g, err))
            << name << ": " << err;
        shots.emplace(name, std::move(g));
    }
    // Холст до сохранения и после открытия: панель справа пишет имя файла
    // и в сравнение не входит.
    int bad = 0;
    for (int y = 0; y < tjtm::CANVAS_H; ++y)
        for (int x = 0; x < tjtm::CANVAS_W; ++x)
            if (shots.at("01_drawn").at(x, y).argb() != shots.at("02_reopened").at(x, y).argb())
                ++bad;
    EXPECT_EQ(bad, 0);
    fs::remove_all(base);
}

}  // namespace
