cmake_minimum_required(VERSION 3.4)

project(MoltenTempest)
set(CMAKE_CXX_STANDARD 14)

option(BUILD_SHARED_LIBS "Build shared libraries." OFF)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)

if(WIN32)
  add_definitions(-DVK_USE_PLATFORM_WIN32_KHR)
endif()

add_definitions(-DVULKAN_HPP_NO_EXCEPTIONS)
add_definitions(-DVULKAN_HPP_NO_SMART_HANDLE)
add_definitions(-DSTB_IMAGE_IMPLEMENTATION)
add_definitions(-DSTB_TRUETYPE_IMPLEMENTATION)
add_definitions(-DSTB_IMAGE_WRITE_IMPLEMENTATION)
add_definitions(-DAL_LIBTYPE_STATIC)

if(WIN32)
  # add_definitions(-DHAVE_DSOUND)
  add_definitions(-DHAVE_WINMM)
  add_definitions(-DHAVE_WINDOWS_H)
  file(GLOB AlcBackend
    "thirdparty/openal/Alc/backends/winmm.c"
    "thirdparty/openal/Alc/backends/null.c"
    "thirdparty/openal/Alc/backends/loopback.c"

    )
endif()
if(UNIX)
  # add_definitions(-DHAVE_DSOUND)
  # add_definitions(-DHAVE_ALSA)
  file(GLOB AlcBackend
    "thirdparty/openal/Alc/backends/null.c"
    "thirdparty/openal/Alc/backends/loopback.c"

    )
endif()

add_definitions(-DAL_BUILD_LIBRARY)
add_definitions(-DAL_ALEXT_PROTOTYPES)

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
    "**/**/*.c"
    "**/**/**/*.h"
    "**/**/**/*.cpp"
    "**/**/**/*.c"
    "**/**/**/**/*.h"
    "**/**/**/**/*.cpp"
  )

# FIXME: the headers include some SOURCES files so installing them is useless
# the files in include should be the headers themselves!
file(GLOB PUB_HEADERS
    "include/Tempest/**"
    )

add_library(${PROJECT_NAME} ${SOURCES} ${AlcBackend})

set_target_properties(
    ${PROJECT_NAME} PROPERTIES
    PUBLIC_HEADER ${PUB_HEADERS}
    )

if(WIN32)
  target_link_libraries(${PROJECT_NAME} vulkan-1)
else()
  target_link_libraries(${PROJECT_NAME} vulkan X11 GL)
endif()

if(WIN32)
  target_link_libraries(${PROJECT_NAME} winmm)
endif()

install(
    TARGETS ${PROJECT_NAME}
    LIBRARY DESTINATION lib
    ARCHIVE DESTINATION lib
    PUBLIC_HEADER DESTINATION include/${PROJECT_NAME}
    )
