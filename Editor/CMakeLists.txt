cmake_minimum_required(VERSION 2.8)

project(Editor)
set (CMAKE_CXX_STANDARD 14)

include_directories("../Engine/include")
link_directories   ("../lib")

set(CMAKE_BINARY_DIR ../)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

file(GLOB SOURCES
    "*.h"
    "*.cpp"
    "**/*.h"
    "**/*.cpp"
    "**/**/*.h"
    "**/**/*.cpp"
  )

add_executable(${PROJECT_NAME} ${SOURCES} )
target_link_libraries(${PROJECT_NAME} MoltenTempest)
