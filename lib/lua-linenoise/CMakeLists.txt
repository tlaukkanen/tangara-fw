idf_component_register(
  SRCS "linenoise.c" "encodings/utf8.c"
  INCLUDE_DIRS "."
  REQUIRES "console" "esp-idf-lua")
