cmake_minimum_required(VERSION 2.8)

project(Empty)
set (CMAKE_CXX_STANDARD 14)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/empty)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/empty)

include_directories("${CMAKE_SOURCE_DIR}/../../Engine/include")

set(BUILD_SHARED_LIBS ${BUILD_SHARED_MOLTEN_TEMPEST})
add_subdirectory("${CMAKE_SOURCE_DIR}/../../Engine" build)

add_executable(${PROJECT_NAME} "main.cpp" "game.cpp")
target_link_libraries(${PROJECT_NAME} Tempest)

# copy data to binary directory
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND}
    -E copy_directory ${CMAKE_SOURCE_DIR}/../../bin/
    ${CMAKE_CURRENT_BINARY_DIR}/empty/)
