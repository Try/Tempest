cmake_minimum_required(VERSION 3.16)
project(TempestControllerTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
enable_testing()
find_package(Threads REQUIRED)

set(TEMPEST_ROOT "${CMAKE_CURRENT_LIST_DIR}/../..")
set(GTEST_ROOT "${TEMPEST_ROOT}/Tests/googletest/googletest")
set(CONTROLLER_SOURCES
  "${TEMPEST_ROOT}/Engine/system/gamecontroller.cpp"
  "${TEMPEST_ROOT}/Engine/system/controllerstate.cpp"
  "${TEMPEST_ROOT}/Engine/system/api/controllerapi.cpp")

add_library(ControllerTestMain STATIC "${GTEST_ROOT}/src/gtest-all.cc" "${GTEST_ROOT}/src/gtest_main.cc")
target_include_directories(ControllerTestMain PUBLIC "${GTEST_ROOT}/include" PRIVATE "${GTEST_ROOT}")
target_link_libraries(ControllerTestMain PUBLIC Threads::Threads)

add_executable(ControllerStateTests "${TEMPEST_ROOT}/Tests/tests/controller_test.cpp" ${CONTROLLER_SOURCES})
target_include_directories(ControllerStateTests PRIVATE "${TEMPEST_ROOT}/Engine/include")
target_link_libraries(ControllerStateTests PRIVATE ControllerTestMain)
add_test(NAME controller-state COMMAND ControllerStateTests)
set_tests_properties(controller-state PROPERTIES TIMEOUT 20)

if(APPLE AND NOT CMAKE_SYSTEM_NAME STREQUAL "iOS")
  enable_language(OBJCXX)
  set(CMAKE_OBJCXX_STANDARD 17)
  target_sources(ControllerStateTests PRIVATE "${TEMPEST_ROOT}/Engine/system/api/macoscontrollerapi.mm")
  target_link_libraries(ControllerStateTests PRIVATE "-framework AppKit" "-framework GameController")
  add_executable(MacOSControllerTests "${TEMPEST_ROOT}/Tests/tests/macoscontroller_test.mm" ${CONTROLLER_SOURCES})
  target_include_directories(MacOSControllerTests PRIVATE "${TEMPEST_ROOT}/Engine/include")
  target_link_libraries(MacOSControllerTests PRIVATE ControllerTestMain "-framework AppKit" "-framework GameController")
  add_test(NAME controller-macos COMMAND MacOSControllerTests)
  set_tests_properties(controller-macos PROPERTIES TIMEOUT 20)
endif()

if(NOT MSVC)
  target_compile_options(ControllerStateTests PRIVATE -Wall -Wextra -Werror)
  if(TARGET MacOSControllerTests)
    target_compile_options(MacOSControllerTests PRIVATE -Wall -Wextra -Werror)
  endif()
endif()
