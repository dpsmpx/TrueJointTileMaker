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
    test_anim.cpp
    test_art.cpp
    test_color.cpp
    test_config.cpp
    test_doc.cpp
    test_filters.cpp
    test_formats.cpp
    test_gameset.cpp
    test_imageops.cpp
    test_layers.cpp
    test_map.cpp
    test_model.cpp
    test_palette.cpp
    test_precision.cpp
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
# Список тестов спрашивается у самой программы. Под Wine первый запуск
# поднимает префикс десятки секунд, и пяти секунд по умолчанию не хватает.
set(TJTM_DISCOVERY_TIMEOUT 120)
gtest_discover_tests(tjtm_core_tests DISCOVERY_MODE PRE_TEST
    DISCOVERY_TIMEOUT ${TJTM_DISCOVERY_TIMEOUT})

# Программа целиком: сценарии без экрана и сверка снимков с оригиналом.
if(TARGET tjtm)
    add_executable(tjtm_app_tests test_app.cpp)
    target_link_libraries(tjtm_app_tests PRIVATE tjtm_core GTest::gtest_main)
    target_compile_definitions(tjtm_app_tests PRIVATE
        TJTM_SOURCE_DIR="${PROJECT_SOURCE_DIR}"
        TJTM_APP_EXE="$<TARGET_FILE:tjtm>")
    add_dependencies(tjtm_app_tests tjtm)
    gtest_discover_tests(tjtm_app_tests DISCOVERY_MODE PRE_TEST
        DISCOVERY_TIMEOUT ${TJTM_DISCOVERY_TIMEOUT})
endif()
