cmake_minimum_required(VERSION 3.4)

project(MoltenTempest)
set(CMAKE_CXX_STANDARD 14)

option(BUILD_SHARED_LIBS "Build shared libraries." OFF)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

add_definitions(-DVULKAN_HPP_NO_EXCEPTIONS)
add_definitions(-DVULKAN_HPP_NO_SMART_HANDLE)
add_definitions(-DSTB_IMAGE_IMPLEMENTATION)
add_definitions(-DSTB_TRUETYPE_IMPLEMENTATION)
add_definitions(-DSTB_IMAGE_WRITE_IMPLEMENTATION)
add_definitions(-DAL_ALEXT_PROTOTYPES)

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

include_directories("include"
                    "."
                    "thirdparty/openal/include"
                    "thirdparty/openal/openal32/include"
                    "thirdparty/squish"
                    "$ENV{VK_SDK_PATH}/include")

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
add_library(${PROJECT_NAME} ${SOURCES})

set_target_properties(
    ${PROJECT_NAME} PROPERTIES
    PUBLIC_HEADER "${PUB_HEADERS}"
    )

if(WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE vulkan-1 shlwapi)
  target_link_libraries(${PROJECT_NAME} PRIVATE winmm Kernel32)
  if(MSVC)
    target_link_libraries(${PROJECT_NAME} PRIVATE d3d12 dxgi)
  endif()
else()
  target_link_libraries(${PROJECT_NAME} PRIVATE vulkan X11 GL)
endif()

target_link_libraries(${PROJECT_NAME} PRIVATE OpenAL)

install(
    TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include/${PROJECT_NAME}
    )
