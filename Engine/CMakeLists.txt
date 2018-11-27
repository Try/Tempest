cmake_minimum_required(VERSION 3.4)

project(MoltenTempest)
set (CMAKE_CXX_STANDARD 14)

set(CMAKE_WINDOWS_EXPORT_ALL_SYMBOLS ON)
set(CMAKE_BINARY_DIR ../)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

IF (WIN32)
  add_definitions(-DVK_USE_PLATFORM_WIN32_KHR)
ENDIF()

add_definitions(-DVULKAN_HPP_NO_EXCEPTIONS)
add_definitions(-DVULKAN_HPP_NO_SMART_HANDLE)
add_definitions(-DSTB_IMAGE_IMPLEMENTATION)
add_definitions(-DSTB_TRUETYPE_IMPLEMENTATION)
add_definitions(-DSTB_IMAGE_WRITE_IMPLEMENTATION)

include_directories("include"
                    "."
                    "$ENV{VK_SDK_PATH}/Include")
link_directories(${PROJECT_NAME} "$ENV{VK_SDK_PATH}/Lib32")

file(GLOB SOURCES
    "include/*"
    "*.h"
    "*.cpp"
    "**/*.h"
    "**/*.cpp"
    "**/**/*.h"
    "**/**/*.cpp"
  )

add_library( ${PROJECT_NAME} SHARED ${SOURCES} )

target_link_libraries(${PROJECT_NAME} vulkan-1)
