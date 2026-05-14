cmake_minimum_required(VERSION 3.16)

project(minivorbis LANGUAGES C)

include_directories(".")
include_directories("./minivorbis/")

file(GLOB SOURCES
    "./*.h"
    "./*.c"
  )

add_library(${PROJECT_NAME} STATIC ${SOURCES})
