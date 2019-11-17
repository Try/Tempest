CMAKE_MINIMUM_REQUIRED(VERSION 3.1)

PROJECT(TempestTests LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 14)

include_directories("${CMAKE_SOURCE_DIR}/../../Engine/include")
link_directories   ("${CMAKE_SOURCE_DIR}/../../lib")
option(BUILD_SHARED_MOLTEN_TEMPEST "Build shared MoltenTempest." ON)

set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/testsuite)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/testsuite)

add_definitions(-DGTEST_LANGUAGE_CXX11)

if($ENV{GOOGLETEST_DIR})
  SET(GOOGLETEST_DIR $ENV{GOOGLETEST_DIR})
else()
  message(WARNING "Using googletest src dir specified at Qt Creator wizard")
  SET(GOOGLETEST_DIR "${CMAKE_SOURCE_DIR}/../googletest")
  message(WARNING ${GOOGLETEST_DIR})
endif()

if(EXISTS ${GOOGLETEST_DIR})
  SET(GTestSrc ${GOOGLETEST_DIR}/googletest)
  SET(GMockSrc ${GOOGLETEST_DIR}/googlemock)
else()
#  message( FATAL_ERROR "No googletest src dir found - set GOOGLETEST_DIR to enable!")
endif()

file(GLOB SOURCES
  "*.h"
  "*.cpp"
  )

include_directories(${GTestSrc} ${GTestSrc}/include ${GMockSrc} ${GMockSrc}/include)

add_executable(${PROJECT_NAME}
  ${SOURCES}
  ${GTestSrc}/src/gtest-all.cc
  ${GMockSrc}/src/gmock-all.cc)

set(BUILD_SHARED_LIBS ${BUILD_SHARED_MOLTEN_TEMPEST})
add_subdirectory("${CMAKE_SOURCE_DIR}/../../Engine" build)

if(UNIX)
  target_link_libraries(${PROJECT_NAME} -lpthread)
endif()

add_test(${PROJECT_NAME} COMMAND ${PROJECT_NAME})
target_link_libraries(${PROJECT_NAME} MoltenTempest)

# copy data to binary directory
add_custom_command(
    TARGET ${PROJECT_NAME} POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${CMAKE_SOURCE_DIR}/../../bin/
    ${CMAKE_CURRENT_BINARY_DIR}/testsuite/data/)

install(
    TARGETS ${PROJECT_NAME}
    DESTINATION bin
    )
