cmake_minimum_required(VERSION 3.4)

project(OpenAL)
SET(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/../../cmake")

if(MSVC)
  add_definitions(-DNOMINMAX)
endif()

add_definitions(-DAL_LIBTYPE_STATIC)
add_definitions(-DAL_BUILD_LIBRARY)

include_directories("."
                    "include"
                    "openal32/include")
### Platforms
if(WIN32)
  add_definitions(-DHAVE_WINMM)
  add_definitions(-DHAVE_DSOUND)
  file(GLOB AlcBackend
    "Alc/backends/winmm.c"
    "Alc/backends/dsound.c"
    "Alc/backends/null.c"
    "Alc/backends/loopback.c"
    )
endif()
if(UNIX)
  add_definitions(-DHAVE_ALSA)
  file(GLOB AlcBackend
    "Alc/backends/alsa.c"
    "Alc/backends/null.c"
    "Alc/backends/loopback.c"
    )
endif()


file(GLOB SOURCES
    "*.h"
    "*.c"
    "**/*.h"
    "**/*.c"
  )

add_library(${PROJECT_NAME} STATIC ${SOURCES} ${AlcBackend})

if(WIN32)
  target_link_libraries(${PROJECT_NAME} PRIVATE dxguid Dsound winmm Kernel32)
endif()

if(UNIX)
  target_link_libraries(${PROJECT_NAME} PRIVATE asound)
endif()
