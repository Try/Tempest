cmake_minimum_required(VERSION 3.12)

project(fonts-tempest)

set(HEADER "${CMAKE_CURRENT_BINARY_DIR}/builtin_fonts.h")
set(CPP    "${CMAKE_CURRENT_BINARY_DIR}/builtin_fonts.cpp")

file(GLOB_RECURSE TEMPEST_FONTS "${CMAKE_CURRENT_LIST_DIR}/*.ttf")

add_custom_command(
  OUTPUT     ${HEADER} ${CPP}
  DEPENDS    ${TEMPEST_FONTS}
  COMMAND    ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/"
  COMMAND    ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_LIST_DIR}/compile_fonts.cmake"
  WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )

add_custom_target(fonts
  DEPENDS ${HEADER} ${CPP}
  SOURCES ${SOURCES} "${CMAKE_CURRENT_LIST_DIR}/compile_fonts.cmake")

add_library(${PROJECT_NAME} STATIC ${HEADER} ${CPP})
add_dependencies(${PROJECT_NAME} fonts)

target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/sprv")
set_target_properties(${PROJECT_NAME} PROPERTIES PUBLIC_HEADER ${HEADER})

