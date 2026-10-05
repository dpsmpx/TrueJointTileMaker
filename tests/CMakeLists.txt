# Тесты переноса. Пока здесь только проверка инструментов: что GoogleTest,
# SDL_image и ресурсы на месте и что программа проходит кадр без экрана.
#
# Сравнение с эталоном (pascal/ под Mono) описано в docs/porting.md.

find_package(GTest CONFIG QUIET)
if(NOT GTest_FOUND)
    # Системного GoogleTest нет (обычно на Windows без vcpkg) — берём исходники.
    include(FetchContent)
    FetchContent_Declare(googletest
        GIT_REPOSITORY https://github.com/google/googletest.git
        GIT_TAG v1.14.0
        GIT_SHALLOW TRUE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    FetchContent_MakeAvailable(googletest)
endif()

add_executable(tjtm_tests toolchain_test.cpp)
target_link_libraries(tjtm_tests PRIVATE
    tjtm_options GTest::gtest_main SDL2::SDL2 SDL2_image::SDL2_image)
target_compile_definitions(tjtm_tests PRIVATE TJTM_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

include(GoogleTest)
gtest_discover_tests(tjtm_tests DISCOVERY_MODE PRE_TEST)

# Программа целиком: три кадра без экрана и снимок последнего.
# Драйвер offscreen есть в любой сборке SDL2, Xvfb для него не нужен.
# Ускорение кадрового буфера выключено: иначе SDL поднимает EGL и Mesa,
# а их собственные утечки LeakSanitizer приписывает нам.
add_test(NAME app_offscreen_smoke
         COMMAND tjtm --frames 3 --screenshot ${CMAKE_CURRENT_BINARY_DIR}/app_smoke.png)
set_tests_properties(app_offscreen_smoke PROPERTIES
    ENVIRONMENT "SDL_VIDEODRIVER=offscreen;SDL_FRAMEBUFFER_ACCELERATION=0")
