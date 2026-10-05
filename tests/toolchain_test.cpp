// Проверка инструментов: библиотеки линкуются, ресурсы находятся,
// эталонный корпус игры подключён. Тесты самого переноса придут позже.

#include <SDL.h>
#include <SDL_image.h>
#include <gtest/gtest.h>

#include <cstdlib>
#include <filesystem>
#include <string>

namespace fs = std::filesystem;

namespace {

const fs::path kSource = TJTM_SOURCE_DIR;

}  // namespace

// Палитра по умолчанию — 8x8, как PAL_COLS x PAL_ROWS в оригинале.
TEST(Toolchain, DefaultPaletteLoads) {
    const fs::path path = kSource / "Resources" / "Palitra.png";
    SDL_Surface* s = IMG_Load(path.string().c_str());
    ASSERT_NE(s, nullptr) << IMG_GetError();
    EXPECT_EQ(s->w, 8);
    EXPECT_EQ(s->h, 8);
    SDL_FreeSurface(s);
}

// Старая реализация на месте: на ней держится сравнение с эталоном.
TEST(Toolchain, PascalReferenceSourcesPresent) {
    for (const char* name : {"TJTM.pas", "TJTMColor.pas", "TJTMConfig.pas", "TJTMMap.pas"}) {
        EXPECT_TRUE(fs::exists(kSource / "pascal" / name)) << name;
    }
}

// Карты игры — эталон для разбора и записи. Корпус ставит tools/setup-env.sh;
// без него тест пропускается, а не падает.
TEST(Toolchain, AndorsLoveCorpusAvailable) {
    const char* dir = std::getenv("ANDORS_LOVE_DIR");
    if (dir == nullptr || !fs::exists(fs::path(dir) / "data" / "maps")) {
        GTEST_SKIP() << "ANDORS_LOVE_DIR не задан: корпус не подключён";
    }
    int maps = 0;
    for (const auto& e : fs::directory_iterator(fs::path(dir) / "data" / "maps")) {
        if (e.path().extension() == ".map") ++maps;
    }
    EXPECT_GT(maps, 0);
}
