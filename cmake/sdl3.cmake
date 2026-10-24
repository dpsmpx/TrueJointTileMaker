# SDL3, SDL3_ttf и SDL3_image: сначала установленные в системе, а если их
# нет — скачанные и собранные вместе с проектом.
#
# Установленные ищутся обычным find_package: на Linux их ставит
# tools/setup-env.sh в свой каталог и сообщает CMAKE_PREFIX_PATH, на Windows
# подойдут vcpkg или распакованные выпуски SDL. Если не нашлось ничего,
# FetchContent берёт исходники с GitHub закреплённых выпусков и собирает
# статически: на Windows так получается один exe без DLL рядом.

set(TJTM_SDL_TAG release-3.4.18)
set(TJTM_SDL_TTF_TAG release-3.2.2)
set(TJTM_SDL_IMAGE_TAG release-3.4.8)

find_package(SDL3 CONFIG QUIET)
find_package(SDL3_ttf CONFIG QUIET)
find_package(SDL3_image CONFIG QUIET)

if(SDL3_FOUND AND SDL3_ttf_FOUND AND SDL3_image_FOUND)
    message(STATUS "SDL3 ${SDL3_VERSION}, SDL3_ttf ${SDL3_ttf_VERSION}, SDL3_image ${SDL3_image_VERSION} — установленные")
elseif(TJTM_FETCH_SDL)
    message(STATUS "SDL3 не найдена — скачиваю и собираю вместе с проектом")
    include(FetchContent)
    set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

    set(SDL_SHARED OFF CACHE BOOL "" FORCE)
    set(SDL_STATIC ON CACHE BOOL "" FORCE)
    set(SDL_TEST_LIBRARY OFF CACHE BOOL "" FORCE)
    set(SDL_TESTS OFF CACHE BOOL "" FORCE)
    set(SDL_EXAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3
        GIT_REPOSITORY https://github.com/libsdl-org/SDL.git
        GIT_TAG ${TJTM_SDL_TAG}
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(SDL3)

    # FreeType берётся встроенный; HarfBuzz для кириллицы не нужен — он
    # раскладывает сложные письменности, а их в интерфейсе нет.
    set(SDLTTF_VENDORED ON CACHE BOOL "" FORCE)
    set(SDLTTF_HARFBUZZ OFF CACHE BOOL "" FORCE)
    set(SDLTTF_PLUTOSVG OFF CACHE BOOL "" FORCE)
    set(SDLTTF_SAMPLES OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3_ttf
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_ttf.git
        GIT_TAG ${TJTM_SDL_TTF_TAG}
        GIT_SHALLOW TRUE
        GIT_SUBMODULES external/freetype)
    FetchContent_MakeAvailable(SDL3_ttf)

    # Картинки читает встроенный stb: PNG, JPEG, BMP, GIF, TGA без сторонних
    # библиотек. Форматы, которым они нужны, выключены.
    set(SDLIMAGE_VENDORED OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_BACKEND_STB ON CACHE BOOL "" FORCE)
    set(SDLIMAGE_AVIF OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_JXL OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_TIF OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_WEBP OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_SAMPLES OFF CACHE BOOL "" FORCE)
    set(SDLIMAGE_TESTS OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(SDL3_image
        GIT_REPOSITORY https://github.com/libsdl-org/SDL_image.git
        GIT_TAG ${TJTM_SDL_IMAGE_TAG}
        GIT_SHALLOW TRUE)
    FetchContent_MakeAvailable(SDL3_image)
else()
    message(FATAL_ERROR "SDL3 не найдена, а скачивание выключено (TJTM_FETCH_SDL=OFF)")
endif()
