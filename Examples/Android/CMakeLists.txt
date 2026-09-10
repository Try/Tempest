cmake_minimum_required(VERSION 3.22)
project(TempestExample LANGUAGES CXX)

if(NOT ANDROID)
  message(FATAL_ERROR "Configure this example with the Android NDK toolchain")
endif()

# Tempest applications get this helper through add_subdirectory(Engine).
# This packaging sample does not depend on the Android backend yet.
include(../../Engine/cmake/TempestAndroid.cmake)

add_library(TempestExample SHARED main.cpp)
set_target_properties(TempestExample PROPERTIES OUTPUT_NAME tempest-example)
target_link_libraries(TempestExample PRIVATE android log)

add_android_apk(TempestExample-apk
  PACKAGE_NAME org.tempest.example
  CODE TempestExample
  MANIFEST AndroidManifest.xml)
