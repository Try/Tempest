cmake_minimum_required(VERSION 3.4)

project(MoltenTempest)
set(CMAKE_CXX_STANDARD 14)

option(BUILD_SHARED_LIBS "Build shared libraries." OFF)

if(MSVC)
  option(TEMPEST_BUILD_DIRECTX12   "Build directx12 support" ON )
else()
  option(TEMPEST_BUILD_DIRECTX12   "Build directx12 support" OFF)
endif()

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_definitions(-DVULKAN_HPP_NO_EXCEPTIONS)
add_definitions(-DVULKAN_HPP_NO_SMART_HANDLE)
add_definitions(-DSTB_IMAGE_IMPLEMENTATION)
add_definitions(-DSTB_TRUETYPE_IMPLEMENTATION)
add_definitions(-DSTB_IMAGE_WRITE_IMPLEMENTATION)
add_definitions(-DAL_ALEXT_PROTOTYPES)
if(WIN32 AND TEMPEST_BUILD_DIRECTX12)
  add_definitions(-DTEMPEST_BUILD_DIRECTX12)
endif()

### Compillers
if(MSVC)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  add_definitions(-DNOMINMAX)
  add_compile_options(/FS)
endif()

### Platforms
if(WIN32)
  add_definitions(-DVK_USE_PLATFORM_WIN32_KHR)
endif()

### Shaders
set(GEN_SHADERS_HEADER "${PROJECT_BINARY_DIR}/sprv/builtin_shader.h")

file(GLOB SHADER_SOURCES
  "${PROJECT_SOURCE_DIR}/shaders/*.vert"
  "${PROJECT_SOURCE_DIR}/shaders/*.frag")

# GLSL to SPIRV compiler
find_program(GLSLANGVALIDATOR glslangValidator)
if(NOT GLSLANGVALIDATOR)
  message(FATAL_ERROR "glslangValidator required")
endif()

function(add_shader OUTPUT SOURCE)
  set(OUTPUT_FILE "${PROJECT_BINARY_DIR}/sprv/${OUTPUT}")
  set(SOURCE_FILE "${PROJECT_SOURCE_DIR}/shaders/${SOURCE}")
  set(SHADERS      ${SHADERS}      ${SOURCE_FILE} PARENT_SCOPE)
  set(SHADERS_SPRV ${SHADERS_SPRV} ${OUTPUT_FILE} PARENT_SCOPE)

  list(SUBLIST ARGV 2 -1 VARS)
  add_custom_command(
    OUTPUT     ${OUTPUT_FILE}
    BYPRODUCTS ${OUTPUT_FILE}
    DEPENDS    ${SOURCE_FILE}
    COMMAND    ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/sprv/"
    COMMAND    ${GLSLANGVALIDATOR} -V ${VARS} "${SOURCE_FILE}" -o ${OUTPUT_FILE}
    )
endfunction(add_shader)

add_shader(empty.vert.sprv     empty.vert     "")
add_shader(empty.frag.sprv     empty.frag     "")
add_shader(tex_brush.vert.sprv tex_brush.vert "")
add_shader(tex_brush.frag.sprv tex_brush.frag "")

add_custom_command(
  OUTPUT     ${GEN_SHADERS_HEADER}
  BYPRODUCTS ${GEN_SHADERS_HEADER}
  DEPENDS    ${SHADERS_SPRV}
  COMMAND    ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/sprv/"
  COMMAND    ${CMAKE_COMMAND} -P "${PROJECT_SOURCE_DIR}/shaders/link_shaders.cmake"
  WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )

add_custom_target(shaders
  DEPENDS ${GEN_SHADERS_HEADER}
  SOURCES ${SHADER_SOURCES} "${PROJECT_SOURCE_DIR}/shaders/link_shaders.cmake")

include_directories("include"
                    "."
                    "thirdparty/openal/include"
                    "thirdparty/openal/openal32/include"
                    "thirdparty/squish"
                    "$ENV{VK_SDK_PATH}/include"
                    "${PROJECT_BINARY_DIR}/sprv")

if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
  link_directories(${PROJECT_NAME} "$ENV{VK_SDK_PATH}/lib")
else()
  link_directories(${PROJECT_NAME} "$ENV{VK_SDK_PATH}/Lib32")
endif()

file(GLOB SOURCES
    "*.h"
    "*.cpp"
    "**/*.h"
    "**/*.cpp"
    "**/**/*.h"
    "**/**/*.cpp"
    "**/**/**/*.h"
    "**/**/**/*.cpp"
    "**/**/**/**/*.h"
    "**/**/**/**/*.cpp"
  )

# FIXME: the headers include some SOURCES files so installing them is useless
# the files in include should be the headers themselves!
file(GLOB_RECURSE PUB_HEADERS "include/Tempest/**")

add_subdirectory("thirdparty/openal")
add_subdirectory("thirdparty/spirv_cross")
add_library(${PROJECT_NAME} ${GEN_SHADERS_HEADER} ${SOURCES})

set_target_properties(
    ${PROJECT_NAME} PROPERTIES
    PUBLIC_HEADER "${PUB_HEADERS}"
    )

if(WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE vulkan-1 shlwapi)
  target_link_libraries(${PROJECT_NAME} PRIVATE winmm Kernel32)
  if(TEMPEST_BUILD_DIRECTX12)
    target_link_libraries(${PROJECT_NAME} PRIVATE d3d12 d3dcompiler dxgi)
  endif()
else()
  target_link_libraries(${PROJECT_NAME} PRIVATE vulkan X11)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE OpenAL)
add_dependencies(${PROJECT_NAME} shaders)

install(
    TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include/${PROJECT_NAME}
    )
