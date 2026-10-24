# True Joint Tile Maker — перенос на C++ и SDL3.
#
# Старая реализация на PascalABC.NET лежит в pascal/ и служит эталоном:
# см. docs/porting.md.
#
#   cmake --preset debug && cmake --build --preset debug && ctest --preset debug
#
# Состав:
#   tjtm_core — цвет, настройки, карты, растр, форматы файлов. Без SDL:
#               его тесты собираются и идут на любой машине.
#   tjtm_gfx  — программный кадр и примитивы GraphABC, текст через SDL3_ttf.
#   tjtm      — сам редактор: окно, ввод, экраны.
cmake_minimum_required(VERSION 3.24)
project(TrueJointTileMaker VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(TJTM_SANITIZE "Собирать с AddressSanitizer и UndefinedBehaviorSanitizer" OFF)
option(TJTM_WERROR "Считать предупреждения ошибками" OFF)
option(TJTM_FETCH_SDL "Скачать и собрать SDL3, если её нет в системе" ON)
option(TJTM_BUILD_APP "Собирать редактор (без него — только ядро и его тесты)" ON)

include(CTest)

# ccache ускоряет пересборку, но обязательным не является: на Windows
# его обычно нет, и сборка без него должна идти как ни в чём не бывало.
find_program(TJTM_CCACHE ccache)
if(TJTM_CCACHE)
    set(CMAKE_C_COMPILER_LAUNCHER "${TJTM_CCACHE}")
    set(CMAKE_CXX_COMPILER_LAUNCHER "${TJTM_CCACHE}")
endif()

# Общие флаги для кода проекта, но не для сторонних библиотек.
add_library(tjtm_options INTERFACE)
if(MSVC)
    target_compile_options(tjtm_options INTERFACE
        /W4 /utf-8 /permissive- $<$<BOOL:${TJTM_WERROR}>:/WX>)
    target_compile_definitions(tjtm_options INTERFACE _CRT_SECURE_NO_WARNINGS NOMINMAX)
else()
    target_compile_options(tjtm_options INTERFACE
        -Wall -Wextra -Wpedantic -Wshadow -Wconversion -Wsign-conversion
        $<$<BOOL:${TJTM_WERROR}>:-Werror>)
    if(TJTM_SANITIZE)
        target_compile_options(tjtm_options INTERFACE
            -fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined)
        target_link_options(tjtm_options INTERFACE -fsanitize=address,undefined)
    endif()
endif()

# --- ядро -------------------------------------------------------------------

add_library(tjtm_core STATIC
    src/core/color.cpp
    src/core/config.cpp
    src/core/fsutil.cpp
    src/core/mapdoc.cpp
    src/core/model.cpp
    src/core/png.cpp
    src/core/text.cpp
    src/core/zlib.cpp
)
target_include_directories(tjtm_core PUBLIC src)
target_link_libraries(tjtm_core PUBLIC tjtm_options)

# --- SDL3 -------------------------------------------------------------------

if(TJTM_BUILD_APP)
    include(cmake/sdl3.cmake)

    # Программный кадр и текст: всё рисование редактора.
    add_library(tjtm_gfx STATIC
        src/gfx/font.cpp
        src/gfx/surface.cpp
        src/platform/platform.cpp
        src/platform/script.cpp
    )
    target_link_libraries(tjtm_gfx PUBLIC tjtm_core SDL3::SDL3 SDL3_ttf::SDL3_ttf)

    add_executable(tjtm
        src/app/canvas.cpp
        src/app/files.cpp
        src/app/main.cpp
        src/app/menu.cpp
        src/app/panel.cpp
        src/app/screens.cpp
        src/app/tools.cpp
        src/app/ui.cpp
    )
    target_link_libraries(tjtm PRIVATE tjtm_gfx SDL3_image::SDL3_image)
    # На Windows — окно без консоли рядом.
    set_target_properties(tjtm PROPERTIES WIN32_EXECUTABLE TRUE)

    # Рядом с программой — то же, что было рядом с TJTM.exe: ресурсы,
    # стартовая картинка и настройки. Config.txt копируется, только если
    # его ещё нет: программа сама пишет в него, и сборка не должна затирать.
    add_custom_command(TARGET tjtm POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${PROJECT_SOURCE_DIR}/Resources $<TARGET_FILE_DIR:tjtm>/Resources
        COMMAND ${CMAKE_COMMAND} -E copy_directory
                ${PROJECT_SOURCE_DIR}/Картинки $<TARGET_FILE_DIR:tjtm>/Картинки
        COMMAND ${CMAKE_COMMAND} -DSRC=${PROJECT_SOURCE_DIR}/Config.txt
                -DDST=$<TARGET_FILE_DIR:tjtm>/Config.txt -P ${PROJECT_SOURCE_DIR}/cmake/copy_if_missing.cmake
        VERBATIM)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
