cmake_minimum_required(VERSION 3.16)

project(Tempest)
set(CMAKE_CXX_STANDARD 14)

option(TEMPEST_BUILD_SHARED "Build shared Tempest." ON)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_MODULE_PATH "${CMAKE_CURRENT_LIST_DIR}/cmake")

if(MSVC)
  option(TEMPEST_BUILD_DIRECTX12   "Build directx12 support" ON )
else()
  option(TEMPEST_BUILD_DIRECTX12   "Build directx12 support" OFF)
endif()

if(APPLE)
  option(TEMPEST_BUILD_VULKAN      "Build vulkan support"    OFF)
  option(TEMPEST_BUILD_METAL       "Build metal support"     ON )
else()
  option(TEMPEST_BUILD_VULKAN      "Build vulkan support"    ON )
  option(TEMPEST_BUILD_METAL       "Build metal support"     OFF)
endif()

### The Library
# avoid cmake link_directories issue
if("${CMAKE_SIZEOF_VOID_P}" EQUAL "8")
  link_directories("$ENV{VK_SDK_PATH}/lib")
else()
  link_directories("$ENV{VK_SDK_PATH}/Lib32")
endif()

if(TEMPEST_BUILD_SHARED)
  add_library(${PROJECT_NAME} SHARED)
  set(CMAKE_POSITION_INDEPENDENT_CODE ON)
else()
  add_library(${PROJECT_NAME} STATIC)
endif()

### Compillers
if(MSVC)
  add_definitions(-D_CRT_SECURE_NO_WARNINGS)
  add_definitions(-D_CRT_NONSTDC_NO_DEPRECATE)
  add_definitions(-DNOMINMAX)
  add_compile_options(/FS)
endif()

if(APPLE)
  add_definitions(-D_LARGEFILE64_SOURCE)
endif()

### Platforms
if(WIN32)
  find_package(D3D12)
endif()

### zlib
add_subdirectory("thirdparty/zlib")
set(ZLIB_LIBRARY zlibstatic)
set(ZLIB_INCLUDE_DIR "thirdparty/zlib")
target_include_directories(${PROJECT_NAME} PRIVATE "thirdparty/zlib")

### squish
target_include_directories(${PROJECT_NAME} PRIVATE "thirdparty/squish")

### libpng16
set(PNG_SHARED OFF)
set(SKIP_INSTALL_ALL ON)
add_subdirectory("thirdparty/libpng")
target_include_directories(png_static PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
target_include_directories(png_static PRIVATE "thirdparty/zlib")
target_include_directories(${PROJECT_NAME} PRIVATE "thirdparty/libpng")
target_link_libraries(${PROJECT_NAME} PRIVATE png_static)

### stb
add_definitions(-DSTB_IMAGE_IMPLEMENTATION)
add_definitions(-DSTB_TRUETYPE_IMPLEMENTATION)
add_definitions(-DSTB_IMAGE_WRITE_IMPLEMENTATION)

### OpenAl
add_definitions(-DAL_ALEXT_PROTOTYPES)
add_subdirectory("thirdparty/openal")
target_link_libraries(${PROJECT_NAME} PRIVATE OpenAL)
target_include_directories(${PROJECT_NAME} PRIVATE
  "thirdparty/openal/include"
  "thirdparty/openal/openal32/include")
if(WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE winmm)
endif()

### Vulkan
if(TEMPEST_BUILD_VULKAN)
  add_definitions(-DTEMPEST_BUILD_VULKAN)
  add_definitions(-DVULKAN_HPP_NO_EXCEPTIONS)
  add_definitions(-DVULKAN_HPP_NO_SMART_HANDLE)
  target_include_directories(${PROJECT_NAME} PRIVATE "$ENV{VK_SDK_PATH}/include")
  if(WIN32)
    target_link_libraries(${PROJECT_NAME} PRIVATE vulkan-1)
  else()
    target_link_libraries(${PROJECT_NAME} PRIVATE vulkan)
  endif()
endif()

### Directx12
if(WIN32 AND TEMPEST_BUILD_DIRECTX12)
  add_definitions(-DTEMPEST_BUILD_DIRECTX12)
  target_link_libraries(${PROJECT_NAME} PRIVATE d3dcompiler dxgi)
endif()

### Spirv-cross
add_subdirectory("thirdparty/spirv_cross")

### Shaders
set(GEN_SHADERS_HEADER "${PROJECT_BINARY_DIR}/sprv/builtin_shader.h")

file(GLOB SHADER_SOURCES
  "${PROJECT_SOURCE_DIR}/shaders/*.vert"
  "${PROJECT_SOURCE_DIR}/shaders/*.frag"
  "${PROJECT_SOURCE_DIR}/shaders/*.comp")

# GLSL to SPIRV compiler
find_program(GLSLANGVALIDATOR glslangValidator "/opt/homebrew/bin")
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
    DEPENDS    ${SOURCE_FILE}
    COMMAND    ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/sprv/"
    COMMAND    ${GLSLANGVALIDATOR} -V ${VARS} "${SOURCE_FILE}" -o ${OUTPUT_FILE}
    )
endfunction(add_shader)

add_shader(blit.vert.sprv      blit.vert    "")
add_shader(blit.frag.sprv      blit.frag    "")

add_shader(empty.vert.sprv     empty.vert     "")
add_shader(empty.frag.sprv     empty.frag     "")
add_shader(tex_brush.vert.sprv tex_brush.vert "")
add_shader(tex_brush.frag.sprv tex_brush.frag "")

add_custom_command(
  OUTPUT     ${GEN_SHADERS_HEADER}
  DEPENDS    ${SHADERS_SPRV}
  COMMAND    ${CMAKE_COMMAND} -E make_directory "${PROJECT_BINARY_DIR}/sprv/"
  COMMAND    ${CMAKE_COMMAND} -P "${PROJECT_SOURCE_DIR}/shaders/link_shaders.cmake"
  WORKING_DIRECTORY "${PROJECT_BINARY_DIR}"
  )

add_custom_target(shaders
  DEPENDS ${GEN_SHADERS_HEADER}
  SOURCES ${SHADER_SOURCES} "${PROJECT_SOURCE_DIR}/shaders/link_shaders.cmake")
target_include_directories(${PROJECT_NAME} PRIVATE "${PROJECT_BINARY_DIR}/sprv")
add_dependencies(${PROJECT_NAME} shaders)

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

if(APPLE)
  enable_language(OBJCXX)
  file(GLOB ObjCSOURCES
    "*.mm"
    "**/*.mm"
    "**/**/*.mm"
    "**/**/**/*.mm"
    "**/**/**/**/*.mm")
endif()

# FIXME: the headers include some SOURCES files so installing them is useless
# the files in include should be the headers themselves!
file(GLOB_RECURSE PUB_HEADERS "include/Tempest/**")

### Sources
target_sources(${PROJECT_NAME} PRIVATE ${GEN_SHADERS_HEADER} ${SOURCES} ${ObjCSOURCES})
include_directories("." "include")

set_target_properties(
    ${PROJECT_NAME} PROPERTIES
    PUBLIC_HEADER "${PUB_HEADERS}"
    )

if(WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE shlwapi Kernel32)
elseif(APPLE)
  target_link_libraries(${PROJECT_NAME} PRIVATE "-framework AppKit")
elseif(LINUX)
  target_link_libraries(${PROJECT_NAME} PRIVATE X11)
endif()

install(
    TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include/${PROJECT_NAME}
    )
