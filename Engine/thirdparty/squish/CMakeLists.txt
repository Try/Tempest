cmake_minimum_required(VERSION 3.15)

project(squish-tempest)

include_directories(".")
file(GLOB SOURCES
    "*.h"
    "*.cpp"
  )

add_library(${PROJECT_NAME} STATIC ${SOURCES})

