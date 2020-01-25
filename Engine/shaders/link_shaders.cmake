file(GLOB SOURCES
    "sprv/*.sprv"
  )

set(HEADER "${CMAKE_CURRENT_BINARY_DIR}/sprv/builtin_shader.h")

file(WRITE ${HEADER}
  "#include <cstdint>\n"
  "\n"
)

foreach(i ${SOURCES})
  get_filename_component(NAME ${i} NAME)
  string(REPLACE "." "_" CLEAN_NAME ${NAME})
  file(READ "${i}" SHADER_SPRV HEX)
  string(LENGTH ${SHADER_SPRV} SHADER_SPRV_LEN)
  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," SHADER_SPRV ${SHADER_SPRV})

  file(APPEND ${HEADER} "static const uint8_t ${CLEAN_NAME}[] = {${SHADER_SPRV}};\n")
endforeach()
