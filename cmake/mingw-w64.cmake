# Кросс-сборка под Windows из Linux компилятором MinGW-w64.
#
# Windows — вторая заявленная платформа, а собрать под неё здесь больше
# нечем. Пресет mingw проверяет, что перенос собирается для Windows и не
# тянет ничего из Linux: SDL3 и GoogleTest скачиваются и собираются под
# Windows заново (cmake/sdl3.cmake, tests/CMakeLists.txt).
#
# Модель потоков posix: std::mutex и std::thread в MinGW есть только в ней.
set(CMAKE_SYSTEM_NAME Windows)
set(CMAKE_SYSTEM_PROCESSOR x86_64)
set(CMAKE_C_COMPILER x86_64-w64-mingw32-gcc-posix)
set(CMAKE_CXX_COMPILER x86_64-w64-mingw32-g++-posix)
set(CMAKE_RC_COMPILER x86_64-w64-mingw32-windres)

set(CMAKE_FIND_ROOT_PATH /usr/x86_64-w64-mingw32)
set(CMAKE_FIND_ROOT_PATH_MODE_PROGRAM NEVER)
set(CMAKE_FIND_ROOT_PATH_MODE_LIBRARY ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_INCLUDE ONLY)
set(CMAKE_FIND_ROOT_PATH_MODE_PACKAGE ONLY)

# Один exe без DLL рантайма MinGW рядом.
set(CMAKE_EXE_LINKER_FLAGS_INIT "-static -static-libgcc -static-libstdc++")

# Тесты Windows-сборки идут под Wine, если он есть: ctest --preset mingw.
find_program(TJTM_WINE wine)
if(TJTM_WINE)
    set(CMAKE_CROSSCOMPILING_EMULATOR "${TJTM_WINE}")
endif()
