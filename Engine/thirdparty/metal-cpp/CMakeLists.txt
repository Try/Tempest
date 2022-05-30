cmake_minimum_required(VERSION 3.16)

project(MetalCpp LANGUAGES CXX C)

if(MSVC)
  add_definitions(-DNOMINMAX)
endif()

include_directories(".")

### Platforms
if(APPLE)
  file(GLOB SOURCES
      "*.hpp"
      "**/*.hpp"
    )
  add_library(${PROJECT_NAME} INTERFACE ${SOURCES})
endif()

if(APPLE)
  #target_link_libraries(${PROJECT_NAME} PRIVATE asound)
endif()
