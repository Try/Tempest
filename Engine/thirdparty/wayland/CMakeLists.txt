# libwayland-client, built as a static library: the binary doesn't depend on libwayland-client.so,
# so it still starts on systems without Wayland and falls back to X11 at runtime.
# Sources: wayland 1.26.0 (https://gitlab.freedesktop.org/wayland/wayland), only the client library.
# Upstream builds with meson; this file mirrors src/meson.build. config.h.in is Tempest's.

set(WAYLAND_VERSION_MAJOR 1)
set(WAYLAND_VERSION_MINOR 26)
set(WAYLAND_VERSION_MICRO 0)
set(WAYLAND_VERSION "${WAYLAND_VERSION_MAJOR}.${WAYLAND_VERSION_MINOR}.${WAYLAND_VERSION_MICRO}")

find_program(WAYLAND_SCANNER wayland-scanner)
if(NOT WAYLAND_SCANNER)
  message(FATAL_ERROR "wayland-scanner not found: install wayland development package or configure with -DTEMPEST_BUILD_WAYLAND=OFF")
endif()

find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(FFI REQUIRED IMPORTED_TARGET GLOBAL libffi)

include(CheckIncludeFile)
include(CheckFunctionExists)
check_include_file(sys/ucred.h HAVE_SYS_UCRED_H)
check_function_exists(accept4  HAVE_ACCEPT4)
check_function_exists(gettid   HAVE_GETTID)

# Generated files go to <build>/src, so the sources' #include "../config.h" finds <build>/config.h.
set(WAYLAND_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/src")
file(MAKE_DIRECTORY "${WAYLAND_GEN_DIR}")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/config.h.in"                "${CMAKE_CURRENT_BINARY_DIR}/config.h")
configure_file("${CMAKE_CURRENT_SOURCE_DIR}/src/wayland-version.h.in"   "${WAYLAND_GEN_DIR}/wayland-version.h" @ONLY)

# Core protocol (wl_display, wl_surface, ...)
set(xml    "${CMAKE_CURRENT_SOURCE_DIR}/protocol/wayland.xml")
set(header "${WAYLAND_GEN_DIR}/wayland-client-protocol.h")
set(source "${WAYLAND_GEN_DIR}/wayland-protocol.c")
add_custom_command(
  OUTPUT  "${header}" "${source}"
  COMMAND "${WAYLAND_SCANNER}" client-header "${xml}" "${header}"
  COMMAND "${WAYLAND_SCANNER}" public-code   "${xml}" "${source}"
  DEPENDS "${xml}"
  COMMENT "Generating wayland core protocol"
  VERBATIM)

add_library(wayland_client STATIC
  "${header}"
  "${source}"
  src/wayland-client.c
  src/connection.c
  src/wayland-os.c
  src/wayland-util.c
  )
target_include_directories(wayland_client PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/src" "${WAYLAND_GEN_DIR}")
target_compile_definitions(wayland_client PRIVATE _POSIX_C_SOURCE=200809L)
target_compile_options(wayland_client PRIVATE -fvisibility=hidden)
target_link_libraries(wayland_client PRIVATE PkgConfig::FFI Threads::Threads)
set_target_properties(wayland_client PROPERTIES C_STANDARD 99 POSITION_INDEPENDENT_CODE ON)
