# True Joint Tile Maker — перенос на C++.
#
# Старая реализация на PascalABC.NET лежит в pascal/ и служит эталоном:
# см. docs/porting.md. Здесь пока только проверка инструментов — окно,
# текст и снимок кадра; сам перенос начнётся отдельно.
#
#   cmake --preset debug && cmake --build --preset debug && ctest --preset debug
cmake_minimum_required(VERSION 3.21)
project(TrueJointTileMaker LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(TJTM_SANITIZE "Собирать с AddressSanitizer и UndefinedBehaviorSanitizer" OFF)
option(TJTM_WERROR "Считать предупреждения ошибками" OFF)

include(CTest)

# ccache ускоряет пересборку, но обязательным не является: на Windows
# его обычно нет, и сборка без него должна идти как ни в чём не бывало.
find_program(TJTM_CCACHE ccache)
if(TJTM_CCACHE)
    set(CMAKE_CXX_COMPILER_LAUNCHER "${TJTM_CCACHE}")
endif()

find_package(SDL2 REQUIRED CONFIG)
find_package(SDL2_ttf REQUIRED CONFIG)
find_package(SDL2_image REQUIRED CONFIG)

# Общие флаги для всего кода проекта, но не для сторонних библиотек.
add_library(tjtm_options INTERFACE)
if(MSVC)
    target_compile_options(tjtm_options INTERFACE /W4 /utf-8 $<$<BOOL:${TJTM_WERROR}>:/WX>)
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

add_executable(tjtm src/main.cpp)
target_link_libraries(tjtm PRIVATE
    tjtm_options SDL2::SDL2 SDL2_ttf::SDL2_ttf SDL2_image::SDL2_image)
# На Windows точку входа даёт SDL2main: он разворачивает WinMain в main.
if(WIN32 AND TARGET SDL2::SDL2main)
    target_link_libraries(tjtm PRIVATE SDL2::SDL2main)
endif()
# Ресурсы ищутся сначала рядом с программой, потом в исходниках:
# так собранную программу можно запускать прямо из каталога сборки.
target_compile_definitions(tjtm PRIVATE TJTM_SOURCE_DIR="${PROJECT_SOURCE_DIR}")

if(BUILD_TESTING)
    add_subdirectory(tests)
endif()
