# Тесты переноса.
#
# Ядро сверяется с эталоном: tests/golden/*.txt снимает с оригинала
# tools/golden.sh, а тесты считают то же самое на переносе. Mono для этого
# не нужен — эталонные файлы лежат в репозитории.

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

add_executable(tjtm_core_tests
    test_color.cpp
    test_config.cpp
    test_map.cpp
    test_model.cpp
    test_png.cpp
)
target_link_libraries(tjtm_core_tests PRIVATE tjtm_core GTest::gtest_main)
target_compile_definitions(tjtm_core_tests PRIVATE TJTM_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

# Если в системе есть zlib, сжатие переноса проверяется и им: поток,
# который понимает только свой же распаковщик, ничего не доказывает.
find_package(ZLIB QUIET)
if(ZLIB_FOUND)
    target_link_libraries(tjtm_core_tests PRIVATE ZLIB::ZLIB)
    target_compile_definitions(tjtm_core_tests PRIVATE TJTM_HAVE_ZLIB)
endif()

include(GoogleTest)
gtest_discover_tests(tjtm_core_tests DISCOVERY_MODE PRE_TEST)
