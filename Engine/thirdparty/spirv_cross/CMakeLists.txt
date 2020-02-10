cmake_minimum_required(VERSION 3.4)

project(SPIRV-Cross LANGUAGES CXX C)

include_directories(".")

file(GLOB SOURCES
    "*.hpp"
    "*.cpp"
  )

add_library(${PROJECT_NAME} STATIC ${SOURCES})
