cmake_minimum_required(VERSION 3.22)
project(TempestExample LANGUAGES CXX)

if(NOT ANDROID)
  message(FATAL_ERROR "Configure this example with the Android NDK toolchain")
endif()

set(TEMPEST_BUILD_SHARED OFF CACHE BOOL "" FORCE)
# Graphics and audio backends are outside this window lifecycle example.
set(TEMPEST_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
set(TEMPEST_BUILD_VULKAN OFF CACHE BOOL "" FORCE)
add_subdirectory(../../Engine Engine)

add_library(TempestExample SHARED main.cpp)
set_target_properties(TempestExample PROPERTIES OUTPUT_NAME tempest-example)
target_link_libraries(TempestExample PRIVATE Tempest)
target_include_directories(TempestExample PRIVATE ../../Engine/include)

add_android_apk(TempestExample-apk
  PACKAGE_NAME org.tempest.example
  CODE TempestExample
  MANIFEST AndroidManifest.xml)
