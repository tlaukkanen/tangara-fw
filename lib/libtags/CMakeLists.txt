idf_component_register(
  SRCS 437.c 8859.c flac.c id3genres.c id3v1.c id3v2.c it.c m4a.c mod.c opus.c
  s3m.c tags.c utf16.c vorbis.c wav.c xm.c
  INCLUDE_DIRS .
)
target_compile_options(${COMPONENT_LIB} PRIVATE -Wno-array-bounds)
