CMAKE_MINIMUM_REQUIRED(VERSION 3.1)

PROJECT(TempestTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)

include_directories("${CMAKE_SOURCE_DIR}/../../Engine/include")
link_directories   ("${CMAKE_SOURCE_DIR}/../../lib")

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/testsuite)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/testsuite)

add_definitions(-DGTEST_LANGUAGE_CXX11)
if(MSVC)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  add_definitions(-DNOMINMAX)
endif()

if($ENV{GOOGLETEST_DIR})
  SET(GOOGLETEST_DIR $ENV{GOOGLETEST_DIR})
else()
  message(WARNING "Using googletest src dir specified at Qt Creator wizard")
  SET(GOOGLETEST_DIR "${CMAKE_SOURCE_DIR}/../googletest")
  message(WARNING ${GOOGLETEST_DIR})
endif()

if(EXISTS ${GOOGLETEST_DIR})
  SET(GTestSrc ${GOOGLETEST_DIR}/googletest)
  SET(GMockSrc ${GOOGLETEST_DIR}/googlemock)
else()
#  message( FATAL_ERROR "No googletest src dir found - set GOOGLETEST_DIR to enable!")
endif()

# GLSL to SPIRV compiler
find_program(GLSLANGVALIDATOR glslangValidator)
if(NOT GLSLANGVALIDATOR)
  message(FATAL_ERROR "glslangValidator required")
endif()

function(compile_shader SOURCE)
  set(OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/testsuite/shader/${SOURCE}.sprv")
  set(SOURCE_FILE "${CMAKE_SOURCE_DIR}/../shader/${SOURCE}")

  add_custom_command(
    OUTPUT ${OUTPUT_FILE}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/testsuite/shader/"
    COMMAND ${GLSLANGVALIDATOR} -V ${SOURCE_FILE} -o ${OUTPUT_FILE}
    DEPENDS ${SOURCE_FILE})
  set(SHADERS ${SHADERS} ${OUTPUT_FILE} PARENT_SCOPE)
endfunction(compile_shader)


file(GLOB SOURCES
  "*.h"
  "*.cpp"
  "**/*.h"
  "**/*.cpp"
  "../shader/*.*"
  )

include_directories(${GTestSrc} ${GTestSrc}/include ${GMockSrc} ${GMockSrc}/include)

compile_shader(simple_test.vert)
compile_shader(simple_test.frag)
compile_shader(simple_test.comp)
compile_shader(image_store_test.comp)

compile_shader(tess.vert)
compile_shader(tess.frag)
compile_shader(tess.tesc)
compile_shader(tess.tese)

compile_shader(ssbo_write_verify.comp)
compile_shader(ssbo_write.vert)

compile_shader(push_constant.comp)

compile_shader(link_defect.vert)
compile_shader(link_defect.frag)

add_executable(${PROJECT_NAME}
  ${SOURCES}
  ${SHADERS}
  ${GTestSrc}/src/gtest-all.cc
  ${GMockSrc}/src/gmock-all.cc)

set(BUILD_SHARED_LIBS ${BUILD_SHARED_MOLTEN_TEMPEST})
add_subdirectory("${CMAKE_SOURCE_DIR}/../../Engine" build)

if(UNIX)
  target_link_libraries(${PROJECT_NAME} -lpthread)
endif()

add_test(${PROJECT_NAME} COMMAND ${PROJECT_NAME})

target_link_libraries(${PROJECT_NAME} Tempest)

# copy data to binary directory
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND}
    -E copy_directory ${CMAKE_SOURCE_DIR}/../../bin/
    ${CMAKE_CURRENT_BINARY_DIR}/testsuite/data/)

install(
    TARGETS ${PROJECT_NAME}
    DESTINATION bin
    )
