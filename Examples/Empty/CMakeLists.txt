cmake_minimum_required(VERSION 2.8)

project(Empty)
set(CMAKE_CXX_STANDARD 20)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PROJECT_NAME})
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/${PROJECT_NAME})
add_executable(${PROJECT_NAME})
#set(TEMPEST_BUILD_SHARED NO)

# Tempest
add_subdirectory("${CMAKE_SOURCE_DIR}/../../Engine" build)
include_directories("${CMAKE_SOURCE_DIR}/../../Engine/include")
target_link_libraries(${PROJECT_NAME} Tempest)

# shaders
add_subdirectory(shader)
target_link_libraries(${PROJECT_NAME} Shaders)

# source
target_sources(${PROJECT_NAME} PRIVATE "main.cpp" "game.cpp")

# copy data to binary directory
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND}
    -E copy_directory ${CMAKE_SOURCE_DIR}/assets/
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets)
