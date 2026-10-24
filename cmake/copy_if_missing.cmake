# Копирует SRC в DST, только если DST ещё нет.
if(NOT EXISTS "${DST}")
    configure_file("${SRC}" "${DST}" COPYONLY)
endif()
