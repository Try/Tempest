cmake_minimum_required(VERSION 2.8)

project(Empty)
set (CMAKE_CXX_STANDARD 14)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/empty)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/empty)
set(TEMPEST_BUILD_SHARED NO)

add_executable(${PROJECT_NAME})

set(BUILD_SHARED_LIBS ${BUILD_SHARED_MOLTEN_TEMPEST})
include_directories("${CMAKE_SOURCE_DIR}/../../Engine/include")
add_subdirectory("${CMAKE_SOURCE_DIR}/../../Engine" build)
target_link_libraries(${PROJECT_NAME} Tempest)

# shaders
add_subdirectory(shader)
target_link_libraries(${PROJECT_NAME} Shaders)

target_sources(${PROJECT_NAME} PRIVATE "main.cpp" "game.cpp")

# copy data to binary directory
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND}
    -E copy_directory ${CMAKE_SOURCE_DIR}/../../bin/
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/)
