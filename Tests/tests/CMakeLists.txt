CMAKE_MINIMUM_REQUIRED(VERSION 3.15)

PROJECT(TempestTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/testsuite)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/testsuite)

# Setup testing
enable_testing()
add_executable(${PROJECT_NAME})
add_test(${PROJECT_NAME} COMMAND ${PROJECT_NAME})

target_include_directories(${PROJECT_NAME} PRIVATE .)
target_include_directories(${PROJECT_NAME} PRIVATE "${CMAKE_SOURCE_DIR}/../../Engine/include")

add_definitions(-DGTEST_LANG_CXX11=1)
if(MSVC)
  add_definitions(-D_USE_MATH_DEFINES)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  add_definitions(-DNOMINMAX)
  #set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /wd4996")
endif()

if($ENV{GOOGLETEST_DIR})
  SET(GOOGLETEST_DIR $ENV{GOOGLETEST_DIR})
else()
  SET(GOOGLETEST_DIR "${CMAKE_SOURCE_DIR}/../googletest")
endif()

if(EXISTS ${GOOGLETEST_DIR})
  SET(GTestSrc ${GOOGLETEST_DIR}/googletest)
  SET(GMockSrc ${GOOGLETEST_DIR}/googlemock)
else()
#  message( FATAL_ERROR "No googletest src dir found - set GOOGLETEST_DIR to enable!")
endif()

# GLSL to SPIRV compiler
find_program(GLSLANGVALIDATOR glslangValidator "/opt/homebrew/bin")
if(NOT GLSLANGVALIDATOR)
  message(FATAL_ERROR "glslangValidator required")
endif()

function(compile_shader SOURCE)
  set(OUTPUT_FILE "${CMAKE_CURRENT_BINARY_DIR}/testsuite/shader/${SOURCE}.sprv")
  set(SOURCE_FILE "${CMAKE_SOURCE_DIR}/../shader/${SOURCE}")

  set(ARGS "")

  if("${SOURCE}" MATCHES "\.(spv14)\.")
    list(APPEND ARGS "--target-env" "spirv1.4")
  endif()

  add_custom_command(
    OUTPUT ${OUTPUT_FILE}
    COMMAND ${CMAKE_COMMAND} -E make_directory "${CMAKE_BINARY_DIR}/testsuite/shader/"
    COMMAND ${GLSLANGVALIDATOR} -V ${ARGS} ${SOURCE_FILE} -o ${OUTPUT_FILE}
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
compile_shader(image_atomic_test.comp)
compile_shader(image_atomic3d_test.comp)
compile_shader(ssbo_read.comp)
compile_shader(ssbo_zero_length.comp)
compile_shader(overlap_test.comp)
compile_shader(varying_test.frag)
compile_shader(texel_fetch.comp)

compile_shader(depth_write_test.vert)
compile_shader(depth_only.frag)

compile_shader(bindless.comp)
compile_shader(bindless2.comp)
compile_shader(bindless_unused.comp)
compile_shader(array_texture.comp)
compile_shader(array_image.comp)
compile_shader(array_ssbo.comp)

compile_shader(fillbuf.comp)
compile_shader(img2buf.comp)
compile_shader(comp_test.frag)

compile_shader(ubo_input.vert)

compile_shader(tess.vert)
compile_shader(tess.frag)
compile_shader(tess.tesc)
compile_shader(tess.tese)

compile_shader(tess_basic.vert)
compile_shader(tess_basic.tesc)
compile_shader(tess_basic.tese)
compile_shader(tess_basic.frag)

compile_shader(geom_basic.vert)
compile_shader(geom_basic.geom)
compile_shader(geom_basic.frag)

compile_shader(ssbo_write_verify.comp)
compile_shader(ssbo_write.vert)

compile_shader(push_constant.comp)
compile_shader(push_test.vert)
compile_shader(push_test.frag)

compile_shader(link_defect.vert)
compile_shader(link_defect.frag)

compile_shader(loop_defect.comp)

compile_shader(texture.vert)
compile_shader(texture.frag)

compile_shader(ray_test.frag)
compile_shader(ray_test_face.frag)

compile_shader(simple_test.spv14.task)
compile_shader(simple_test.spv14.mesh)

compile_shader(simple_test.mesh.comp)
compile_shader(mesh_prefix_sum.comp)
compile_shader(mesh_compactage.comp)

compile_shader(array_length.vert)
compile_shader(array_length.frag)

target_sources(${PROJECT_NAME} PRIVATE
  ${SOURCES}
  ${SHADERS}
  ${GTestSrc}/src/gtest-all.cc
  ${GMockSrc}/src/gmock-all.cc)

set(BUILD_SHARED_LIBS ${BUILD_SHARED_MOLTEN_TEMPEST})
add_subdirectory("${CMAKE_SOURCE_DIR}/../../Engine" build)

if(UNIX)
  target_link_libraries(${PROJECT_NAME} -lpthread)
endif()

target_link_libraries(${PROJECT_NAME} Tempest)

# copy data to binary directory
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND}
    -E copy_directory ${CMAKE_SOURCE_DIR}/../assets/
    ${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/assets/)

install(
    TARGETS ${PROJECT_NAME}
    DESTINATION bin
    )
