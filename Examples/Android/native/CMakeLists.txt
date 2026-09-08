cmake_minimum_required(VERSION 3.16)
project(TempestExample LANGUAGES CXX)

if(NOT ANDROID)
  message(FATAL_ERROR "Build this packaging example through Examples/Android")
endif()

add_library(TempestExample SHARED main.cpp)
set_target_properties(TempestExample PROPERTIES OUTPUT_NAME tempest-example)
target_link_libraries(TempestExample PRIVATE android log)
include(../../../Engine/cmake/TempestAndroid.cmake)
tempest_android_native_target(TempestExample)
