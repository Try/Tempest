cmake_minimum_required(VERSION 3.14...4.2)

set(PNGLIB_MAJOR 1)
set(PNGLIB_MINOR 6)
set(PNGLIB_REVISION 54)
set(PNGLIB_SUBREVISION 0)
#set(PNGLIB_SUBREVISION "git")
set(PNGLIB_VERSION ${PNGLIB_MAJOR}.${PNGLIB_MINOR}.${PNGLIB_REVISION})
set(PNGLIB_ABI_VERSION ${PNGLIB_MAJOR}${PNGLIB_MINOR})
set(PNGLIB_SHARED_VERSION ${PNGLIB_ABI_VERSION}.${PNGLIB_REVISION}.${PNGLIB_SUBREVISION})

project(png_static
        VERSION ${PNGLIB_VERSION}
        LANGUAGES C)

include_directories("./libpng/")
include_directories(".")
file(GLOB SOURCES
    "libpng/*.h"
    "libpng/*.c"
  )

add_library(${PROJECT_NAME} STATIC ${SOURCES})
