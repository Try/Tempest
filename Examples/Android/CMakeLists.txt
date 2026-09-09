cmake_minimum_required(VERSION 3.22)
project(TempestAndroidPackaging LANGUAGES NONE)

include(../../Engine/cmake/TempestAndroid.cmake)
tempest_android_application(TempestExample
  APPLICATION_ID org.tempest.example
  NATIVE_SOURCE_DIR native
  NATIVE_TARGET TempestExample
  MANIFEST AndroidManifest.xml)
