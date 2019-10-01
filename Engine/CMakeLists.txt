cmake_minimum_required(VERSION 3.4)

project(MoltenTempest)
set(CMAKE_CXX_STANDARD 14)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_BINARY_DIR ../)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

if(WIN32)
  add_definitions(-DVK_USE_PLATFORM_WIN32_KHR)
endif()

add_definitions(-DVULKAN_HPP_NO_EXCEPTIONS)
add_definitions(-DVULKAN_HPP_NO_SMART_HANDLE)
add_definitions(-DSTB_IMAGE_IMPLEMENTATION)
add_definitions(-DSTB_TRUETYPE_IMPLEMENTATION)
add_definitions(-DSTB_IMAGE_WRITE_IMPLEMENTATION)

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
    "include/*"
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

add_library( ${PROJECT_NAME} SHARED ${SOURCES} ${AlcBackend} )

if(WIN32)
  target_link_libraries(${PROJECT_NAME} vulkan-1)
else()
  target_link_libraries(${PROJECT_NAME} vulkan X11 GL)
endif()

if(WIN32)
  target_link_libraries(${PROJECT_NAME} winmm)
endif()
