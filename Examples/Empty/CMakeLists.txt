cmake_minimum_required(VERSION 2.8)

project(Empty)
set (CMAKE_CXX_STANDARD 14)

include_directories("../../Engine/include")
link_directories   ("../../lib")

set(CMAKE_BINARY_DIR ../../)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/../lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

add_executable(${PROJECT_NAME} "main.cpp" "game.cpp")
target_link_libraries(${PROJECT_NAME} MoltenTempest)
