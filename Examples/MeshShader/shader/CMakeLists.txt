cmake_minimum_required(VERSION 3.12)

project(Shaders)

set(HEADER "${PROJECT_BINARY_DIR}/sprv/shader.h")
set(CPP    "${PROJECT_BINARY_DIR}/sprv/shader.cpp")

file(GLOB_RECURSE SOURCES "*.vert" "*.task" "*.mesh" "*.frag" "*.glsl")
file(GLOB_RECURSE GLSL_SOURCES "*.glsl")

# GLSL to SPIRV compiler
find_program(GLSLANGVALIDATOR glslangValidator "/opt/homebrew/bin")
if(NOT GLSLANGVALIDATOR)
  message(FATAL_ERROR "glslangValidator required")
endif()

function(add_shader OUTPUT SOURCE)
  set(OUTPUT_FILE "${PROJECT_BINARY_DIR}/sprv/${OUTPUT}.sprv")
  set(SOURCE_FILE "${CMAKE_SOURCE_DIR}/shader/${SOURCE}")
  set(APP_SHADERS      ${APP_SHADERS}      ${SOURCE_FILE} PARENT_SCOPE)
  set(APP_SHADERS_SPRV ${APP_SHADERS_SPRV} ${OUTPUT_FILE} PARENT_SCOPE)

  list(SUBLIST ARGV 2 -1 VARS)
  add_custom_command(
    OUTPUT     ${OUTPUT_FILE}
    DEPENDS    ${SOURCE_FILE} ${GLSL_SOURCES}
    COMMAND    ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/sprv/"
    COMMAND    ${GLSLANGVALIDATOR} -V ${VARS} --target-env spirv1.4 "${SOURCE_FILE}" -o ${OUTPUT_FILE}
    )
endfunction(add_shader)

# shaders
add_shader(shader.vert  shader.vert "")
add_shader(shader.task  shader.task "")
add_shader(shader.mesh  shader.mesh "")
add_shader(shader.frag  shader.frag "")

add_custom_command(
  OUTPUT     ${HEADER} ${CPP}
  DEPENDS    ${APP_SHADERS_SPRV}
  COMMAND    ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/sprv/"
  COMMAND    ${CMAKE_COMMAND} -P "${CMAKE_CURRENT_LIST_DIR}/link_shaders.cmake"
  WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )

add_custom_target(shader
  DEPENDS ${HEADER} ${CPP}
  SOURCES ${SOURCES} "${CMAKE_CURRENT_LIST_DIR}/link_shaders.cmake")

add_library(${PROJECT_NAME} STATIC ${HEADER} ${CPP})
add_dependencies(${PROJECT_NAME} shader)

target_include_directories(${PROJECT_NAME} PUBLIC "${CMAKE_CURRENT_BINARY_DIR}/sprv")
set_target_properties(
    ${PROJECT_NAME} PROPERTIES
    PUBLIC_HEADER ${HEADER}
    )
