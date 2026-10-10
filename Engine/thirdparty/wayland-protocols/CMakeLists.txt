# Wayland protocol bindings, generated with wayland-scanner.
# The XMLs are copies from wayland-protocols (https://gitlab.freedesktop.org/wayland/wayland-protocols).

find_program(WAYLAND_SCANNER wayland-scanner)
if(NOT WAYLAND_SCANNER)
  message(FATAL_ERROR "wayland-scanner not found: install wayland development package or configure with -DTEMPEST_BUILD_WAYLAND=OFF")
endif()

set(WAYLAND_PROTOCOLS
  xdg-shell
  xdg-decoration-unstable-v1
  viewporter
  fractional-scale-v1
  cursor-shape-v1
  tablet-v2           # only for the zwp_tablet_tool_v2 interface that cursor-shape-v1 references; never bound
  pointer-constraints-unstable-v1
  relative-pointer-unstable-v1
  )

set(WAYLAND_GEN_DIR "${CMAKE_CURRENT_BINARY_DIR}/include")
file(MAKE_DIRECTORY "${WAYLAND_GEN_DIR}")

set(WAYLAND_GEN_SOURCES "")
foreach(name ${WAYLAND_PROTOCOLS})
  set(xml    "${CMAKE_CURRENT_SOURCE_DIR}/${name}.xml")
  set(header "${WAYLAND_GEN_DIR}/${name}-client-protocol.h")
  set(source "${WAYLAND_GEN_DIR}/${name}-client-protocol.c")
  add_custom_command(
    OUTPUT  "${header}" "${source}"
    COMMAND "${WAYLAND_SCANNER}" client-header "${xml}" "${header}"
    COMMAND "${WAYLAND_SCANNER}" private-code  "${xml}" "${source}"
    DEPENDS "${xml}"
    COMMENT "Generating wayland protocol ${name}"
    VERBATIM)
  list(APPEND WAYLAND_GEN_SOURCES "${header}" "${source}")
endforeach()

add_library(wayland_protocols STATIC ${WAYLAND_GEN_SOURCES})
target_include_directories(wayland_protocols PUBLIC "${WAYLAND_GEN_DIR}")
target_link_libraries(wayland_protocols PUBLIC wayland-client)
set_target_properties(wayland_protocols PROPERTIES POSITION_INDEPENDENT_CODE ON)
