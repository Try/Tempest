file(GLOB_RECURSE TEMPEST_FONTS "${CMAKE_CURRENT_LIST_DIR}/*.ttf")

set(HEADER "${CMAKE_CURRENT_BINARY_DIR}/builtin_fonts.h")
set(CPP    "${CMAKE_CURRENT_BINARY_DIR}/builtin_fonts.cpp")

file(WRITE ${HEADER}
  "#include <cstdint>\n"
  "#include <cstring>\n"
  "#include <stdexcept>\n"
  "\n"
  "struct AppFonts {\n"
  "  const char* data;\n"
  "  size_t      len;\n"
)

foreach(i ${TEMPEST_FONTS})
  get_filename_component(NAME ${i} NAME)
  string(REPLACE "." "_" CLEAN_NAME ${NAME})
  string(REPLACE "-" "_" CLEAN_NAME ${CLEAN_NAME})
  file(APPEND ${HEADER} "  static const AppFonts ${CLEAN_NAME};\n")
endforeach()
file(APPEND ${HEADER} "  static AppFonts get(const char* name);\n")
file(APPEND ${HEADER} "  };\n")

file(APPEND ${HEADER} "inline AppFonts AppFonts::get(const char* name) {\n")
foreach(i ${TEMPEST_FONTS})
  get_filename_component(NAME ${i} NAME)
  string(REPLACE "." "_" CLEAN_NAME ${NAME})
  string(REPLACE "-" "_" CLEAN_NAME ${CLEAN_NAME})
  file(APPEND ${HEADER} "  if(std::strcmp(\"${NAME}\",name)==0)\n")
  file(APPEND ${HEADER} "    return ${CLEAN_NAME};\n")
endforeach()
file(APPEND ${HEADER} "  throw std::runtime_error(\"\");\n")
file(APPEND ${HEADER} "  }\n")

file(WRITE ${CPP}
  "#include \"builtin_fonts.h\"\n")

foreach(i ${TEMPEST_FONTS})
  get_filename_component(NAME ${i} NAME)
  string(REPLACE "." "_" CLEAN_NAME ${NAME})
  string(REPLACE "-" "_" CLEAN_NAME ${CLEAN_NAME})
  file(READ "${i}" SHADER_SPRV HEX)
  string(LENGTH ${SHADER_SPRV} SHADER_SPRV_LEN)
  string(REGEX REPLACE "([0-9a-f][0-9a-f])" "0x\\1," SHADER_SPRV ${SHADER_SPRV})

  file(APPEND ${CPP} "static const uint8_t SRC_${CLEAN_NAME}[] = {${SHADER_SPRV}};\n")
endforeach()
file(APPEND ${CPP} "\n")

foreach(i ${TEMPEST_FONTS})
  get_filename_component(NAME ${i} NAME)
  string(REPLACE "." "_" CLEAN_NAME ${NAME})
  string(REPLACE "-" "_" CLEAN_NAME ${CLEAN_NAME})
  file(APPEND ${CPP} "const AppFonts AppFonts::${CLEAN_NAME} = {")
  file(APPEND ${CPP} "reinterpret_cast<const char*>(SRC_${CLEAN_NAME}),sizeof(SRC_${CLEAN_NAME})")
  file(APPEND ${CPP} "};\n")
endforeach()
file(APPEND ${CPP} "\n")

