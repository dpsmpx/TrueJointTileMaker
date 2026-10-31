// Программа целиком: сценарии tests/screens/*.txt без экрана и сверка
// снимков с оригиналом по областям из tests/screens/exact.txt.
//
// Эталонные вырезки сняты с оригинала под Mono тем же сценарием через
// tools/screens.sh. Здесь перенос проходит его сам (--script), на
// драйвере offscreen, и каждая область обязана совпасть до пикселя.

#include <gtest/gtest.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "core/anim.h"
#include "core/gameset.h"
#include "core/gif.h"
#include "core/mapdoc.h"
#include "core/model.h"
#include "core/png.h"
#include "core/project.h"
#include "core/rasterfmt.h"

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

/// Свежий каталог программы: настройки сценариев, ресурсы и картинки из
/// репозитория. Программа пишет в Config.txt, и прогоны не должны влиять
/// друг на друга.
fs::path makeBase(const std::string& name, const std::string& scenario = {}) {
    const fs::path src(TJTM_SOURCE_DIR);
    const fs::path base = fs::temp_directory_path() / ("tjtm_app_test_" + name);
    fs::remove_all(base);
    fs::create_directories(base);
    // Настройки сценариев, а не выпуска (tests/screens/Config.txt), и
    // добавка сценария в конце: последнее значение ключа побеждает.
    fs::copy_file(src / "tests" / "screens" / "Config.txt", base / "Config.txt");
    {
        std::ifstream add(src / "tests" / "screens" / ((scenario.empty() ? name : scenario) + ".config"),
                          std::ios::binary);
        if (add) {
            std::ofstream out(base / "Config.txt", std::ios::binary | std::ios::app);
            out << add.rdbuf();
        }
    }
    fs::copy(src / "Resources", base / "Resources", fs::copy_options::recursive);
    fs::copy(src / fs::path(u8"Картинки"), base / fs::path(u8"Картинки"),
             fs::copy_options::recursive);
    // Свои файлы сценария (набор тайлов, карта) — tests/screens/ИМЯ.files.
    const fs::path extra =
        src / "tests" / "screens" / ((scenario.empty() ? name : scenario) + ".files");
    if (fs::exists(extra)) fs::copy(extra, base, fs::copy_options::recursive);
    return base;
}

void setEnv(const char* name, const std::string& value) {
#ifdef _WIN32
    _putenv_s(name, value.c_str());
#else
    if (value.empty()) {
        unsetenv(name);
    } else {
        setenv(name, value.c_str(), 1);
    }
#endif
}

/// Ответы диалогам файлов: tests/screens/СЦЕНАРИЙ.dialogs, где @BASE@ —
/// каталог прогона. Нет файла — диалогов в сценарии нет.
std::string prepareDialogs(const std::string& scenario, const fs::path& base) {
    std::ifstream in(std::string(TJTM_SOURCE_DIR) + "/tests/screens/" + scenario + ".dialogs");
    if (!in) return {};
    const fs::path answers = base / "dialogs.txt";
    std::ofstream out(answers, std::ios::binary);
    std::string line;
    while (std::getline(in, line)) {
        const std::size_t at = line.find("@BASE@");
        if (at != std::string::npos) line.replace(at, 6, base.string());
        out << line << "\n";
    }
    return answers.string();
}

/// Сценарии оригинала: их эталон снят с 0.6.0, и перенос идёт с жестами и
/// видом оригинала (--classic), а не с правилами плана «Интерфейс».
bool isOriginalScenario(const std::string& scenario) {
    return scenario == "tile" || scenario == "map" || scenario == "draw" ||
           scenario == "mapedit" || scenario == "menu";
}

/// Пройти сценарий; снимки — в base/shots.
bool runScenario(const std::string& scenario, const fs::path& base) {
    const fs::path shots = base / "shots";
    fs::create_directories(shots);
    const std::string script = std::string(TJTM_SOURCE_DIR) + "/tests/screens/" + scenario + ".txt";
    setEnv("SDL_VIDEODRIVER", "offscreen");
    // Программный кадровый буфер: иначе SDL поднимает EGL, и утечки Mesa
    // в сборке asan достаются программе.
    setEnv("SDL_FRAMEBUFFER_ACCELERATION", "0");
    setEnv("TJTM_TEST_DIALOGS", prepareDialogs(scenario, base));
    const std::string cmd = "\"" + std::string(TJTM_APP_EXE) + "\" --base \"" + base.string() +
                            "\" --script \"" + script + "\" --shots \"" + shots.string() + "\"" +
                            (isOriginalScenario(scenario) ? " --classic" : "");
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
                         ::testing::Values("tile", "map", "draw", "mapedit", "pro1", "pal2",
                                           "anim4", "prec5", "layers6", "game7", "fx8", "art9",
                                           "map10"));

/// Настройки выпуска (Config.txt в корне): при запуске — сразу редактор с
/// последним файлом, без меню во всё окно (план «Интерфейс», A9).
TEST(AppScreens, ReleaseConfigOpensLastFile) {
    const fs::path base = makeBase("start");
    fs::copy_file(fs::path(TJTM_SOURCE_DIR) / "Config.txt", base / "Config.txt",
                  fs::copy_options::overwrite_existing);
    {
        std::ofstream add(base / "Config.txt", std::ios::binary | std::ios::app);
        add << "LastFile = " << reinterpret_cast<const char*>(u8"Картинки/debug.png") << "\r\n";
    }
    ASSERT_TRUE(runScenario("start", base));
    std::string err;
    tjtm::Grid<tjtm::Color> shot;
    ASSERT_TRUE(tjtm::pngLoad((base / "shots" / "01_start.png").string(), shot, err)) << err;
    tjtm::Grid<tjtm::Color> img;
    ASSERT_TRUE(tjtm::pngLoad((base / fs::path(u8"Картинки") / "debug.png").string(), img, err))
        << err;
    // Картинка вписана в холст: её середина — посреди холста. Меню на её
    // месте рисовало бы кнопки.
    const int ps = std::min(tjtm::CANVAS_W / img.w, tjtm::CANVAS_H / img.h);
    const int ox = (tjtm::CANVAS_W - img.w * ps) / 2;
    const int oy = (tjtm::CANVAS_H - img.h * ps) / 2;
    int same = 0;
    for (int y = 0; y < img.h; ++y) {
        for (int x = 0; x < img.w; ++x) {
            const tjtm::Color c = img.at(x, y);
            if (c.a == 255 && shot.at(ox + x * ps + ps / 2, oy + y * ps + ps / 2).argb() == c.argb())
                ++same;
        }
    }
    int opaque = 0;
    for (const tjtm::Color c : img.d)
        opaque += c.a == 255 ? 1 : 0;
    EXPECT_EQ(same, opaque) << "на холсте — последний файл, а не меню";
    fs::remove_all(base);
}

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
    ASSERT_TRUE(runScenario("tjt", base));

    ASSERT_TRUE(fs::exists(project)) << "проект не записан";
    std::vector<std::uint8_t> bytes;
    {
        // Файл закрывается сразу: открытый, он не дал бы Windows убрать
        // каталог прогона.
        std::ifstream in(project, std::ios::binary);
        bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
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

/// Этап 4: выгрузки сценария anim4 — GIF, лист с описанием и проект —
/// записаны и читаются обратно с нужными кадрами, слоями и меткой.
TEST(AppScreens, AnimationExports) {
    const fs::path base = makeBase("anim4x");
    ASSERT_TRUE(runScenario("anim4", base));
    auto bytesOf = [](const fs::path& p) {
        std::ifstream in(p, std::ios::binary);
        return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)),
                                         std::istreambuf_iterator<char>());
    };
    std::string err;
    std::vector<tjtm::AnimFrame> frames;
    int plays = -1;
    ASSERT_TRUE(tjtm::gifDecode(bytesOf(base / "walk.gif"), frames, err, &plays)) << err;
    EXPECT_EQ(frames.size(), 3U);
    EXPECT_EQ(plays, 0);
    EXPECT_EQ(frames[0].image.w, 64);

    tjtm::Grid<tjtm::Color> sheet;
    ASSERT_TRUE(tjtm::pngLoad((base / "walk_sheet.png").string(), sheet, err)) << err;
    EXPECT_EQ(sheet.w, 3 * 64);
    EXPECT_EQ(sheet.h, 64);
    const auto json = bytesOf(base / "walk_sheet.json");
    const std::string js(json.begin(), json.end());
    EXPECT_NE(js.find("\"direction\": \"pingpong\""), std::string::npos);
    EXPECT_NE(js.find("\"image\": \"walk_sheet.png\""), std::string::npos);

    tjtm::Model m;
    ASSERT_TRUE(tjtm::projectDecode(bytesOf(base / "walk.tjt"), m, err)) << err;
    EXPECT_EQ(m.doc.layerCount(), 2);
    EXPECT_EQ(m.doc.frameCount(), 3);
    ASSERT_EQ(m.doc.tags.size(), 1U);
    EXPECT_EQ(m.doc.tags[0].from, 0);
    EXPECT_EQ(m.doc.tags[0].to, 2);
    EXPECT_EQ(m.doc.tags[0].direction, tjtm::TAG_PINGPONG);
    EXPECT_TRUE(m.onionSkin);
    // Второй слой есть только в третьем кадре.
    EXPECT_EQ(m.doc.cel(1, 0), nullptr);
    EXPECT_NE(m.doc.cel(1, 2), nullptr);
    fs::remove_all(base);
}

/// Этап 5: «Сохранить как» с расширениями .bmp и .tif пишет эти форматы,
/// а TIFF открывается обратно своим чтением.
TEST(AppScreens, BmpAndTiffSaved) {
    const fs::path base = makeBase("prec5x");
    ASSERT_TRUE(runScenario("prec5", base));
    auto bytesOf = [](const fs::path& p) {
        std::ifstream in(p, std::ios::binary);
        return std::vector<std::uint8_t>((std::istreambuf_iterator<char>(in)),
                                         std::istreambuf_iterator<char>());
    };
    const auto bmp = bytesOf(base / "out.bmp");
    ASSERT_GE(bmp.size(), 54U);
    EXPECT_EQ(bmp[0], 'B');
    EXPECT_EQ(bmp[18], 79);  // ширина после поворота на 15°
    std::vector<tjtm::Grid<tjtm::Color>> pages;
    std::string err;
    ASSERT_TRUE(tjtm::tiffDecode(bytesOf(base / "out.tif"), pages, err)) << err;
    ASSERT_EQ(pages.size(), 1U);
    EXPECT_EQ(pages[0].w, 79);
    EXPECT_EQ(pages[0].h, 79);
    fs::remove_all(base);
}

/// Этап 6: проект сценария layers6 хранит группу, векторный слой с
/// фигурами и тайлкарту с набором.
TEST(AppScreens, LayersProjectSaved) {
    const fs::path base = makeBase("layers6x");
    ASSERT_TRUE(runScenario("layers6", base));
    std::vector<std::uint8_t> bytes;
    {
        std::ifstream in(base / "layers.tjt", std::ios::binary);
        bytes.assign(std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>());
    }
    tjtm::Model m;
    std::string err;
    ASSERT_TRUE(tjtm::projectDecode(bytes, m, err)) << err;
    ASSERT_EQ(m.doc.layerCount(), 4);
    EXPECT_EQ(m.doc.layers[1].kind, tjtm::LK_VECTOR);
    EXPECT_EQ(m.doc.layers[2].kind, tjtm::LK_GROUP);
    EXPECT_TRUE(m.doc.layers[2].collapsed);
    EXPECT_EQ(m.doc.layers[3].kind, tjtm::LK_TILEMAP);
    ASSERT_TRUE(m.doc.cel(1, 0));
    EXPECT_EQ(m.doc.cel(1, 0)->shapes.size(), 3U);
    ASSERT_TRUE(m.doc.cel(3, 0));
    for (const int v : m.doc.cel(3, 0)->tiles.d)
        EXPECT_EQ(v, 2);
    fs::remove_all(base);
}

/// Этап 7: лист 8x8 и атлас выгружены из набора, варианты стыков
/// разрезаны из заготовки, уже нарисованный вариант сохранился.
TEST(AppScreens, GameSetExported) {
    const fs::path base = makeBase("game7x", "game7");
    ASSERT_TRUE(runScenario("game7", base));
    const fs::path fixture =
        fs::path(TJTM_SOURCE_DIR) / "tests" / "screens" / "game7.files" / "tiles";
    std::string err;
    auto load = [&](const fs::path& p) {
        tjtm::Grid<tjtm::Color> g;
        EXPECT_TRUE(tjtm::pngLoad(p.string(), g, err)) << p.string() << ": " << err;
        return g;
    };

    std::vector<tjtm::Grid<tjtm::Color>> slots;
    ASSERT_TRUE(tjtm::game::sliceSheet(load(base / "sheet.png"), slots, err)) << err;
    for (int i = 0; i < tjtm::TSET_BASE_COUNT; ++i) {
        const fs::path src = fixture / (tjtm::tsetBaseFile(i) + ".png");
        const auto& got = slots[static_cast<std::size_t>(i)];
        if (fs::exists(src)) {
            EXPECT_EQ(got.d, load(src).d) << tjtm::tsetBaseFile(i);
        } else {
            EXPECT_TRUE(got.empty()) << tjtm::tsetBaseFile(i);
        }
    }

    const tjtm::Grid<tjtm::Color> atlas = load(base / "atlas.png");
    // Поток закрыт сразу: в Windows открытый файл не даёт убрать каталог.
    const std::string json = [&] {
        std::ifstream jin(base / "atlas.json", std::ios::binary);
        return std::string((std::istreambuf_iterator<char>(jin)), std::istreambuf_iterator<char>());
    }();
    int frames = 0;
    for (const auto& e : fs::directory_iterator(fixture)) {
        const std::string name = e.path().filename().string();
        const std::size_t at = json.find("\"" + name + "\": {");
        ASSERT_NE(at, std::string::npos) << name;
        int x = 0;
        int y = 0;
        ASSERT_EQ(std::sscanf(json.c_str() + json.find("\"frame\"", at),
                              "\"frame\": { \"x\": %d, \"y\": %d", &x, &y),
                  2);
        const auto img = load(e.path());
        for (int yy = 0; yy < img.h; ++yy) {
            for (int xx = 0; xx < img.w; ++xx)
                ASSERT_EQ(atlas.at(x + xx, y + yy).argb(), img.at(xx, yy).argb()) << name;
        }
        ++frames;
    }
    EXPECT_EQ(frames, 12);

    const auto wall = load(fixture / "wall.png");
    for (int k = 0; k < 16; ++k)
        EXPECT_TRUE(fs::exists(base / "tiles" / ("wall_" + std::to_string(k) + ".png"))) << k;
    EXPECT_EQ(load(base / "tiles" / "wall_0.png").d, wall.d);
    EXPECT_EQ(load(base / "tiles" / "wall_10.png").d, load(fixture / "wall_10.png").d);
    EXPECT_NE(load(base / "tiles" / "wall_5.png").d, wall.d);
    fs::remove_all(base);
}

/// Этап 10: правка формой и замена местности попадают в файл карты, а
/// нетронутый объект пишется прежней строкой.
TEST(AppScreens, MapEditsSaved) {
    const fs::path base = makeBase("map10x", "map10");
    ASSERT_TRUE(runScenario("map10", base));
    tjtm::MapDoc m;
    const tjtm::MapParseResult r = tjtm::mapLoad((base / "maps" / "probe.map").string(), m);
    ASSERT_TRUE(r.ok) << r.error;
    ASSERT_EQ(m.objects.size(), 2U);
    EXPECT_EQ(tjtm::mapObjectLine(m.objects[0]), "npc 4 6 smith");
    EXPECT_EQ(tjtm::mapObjectLine(m.objects[1]), "exit 12 7 cells 24 9");
    int trees = 0;
    int water = 0;
    for (const std::uint8_t c : m.grid) {
        trees += c == tjtm::MT_TREE ? 1 : 0;
        water += c == tjtm::MT_WATER ? 1 : 0;
    }
    EXPECT_EQ(trees, 0) << "деревья смеси заменены водой";
    EXPECT_GT(water, 6);
    fs::remove_all(base);
}

}  // namespace
