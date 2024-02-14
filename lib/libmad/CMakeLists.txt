file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/include")
idf_component_register(
  SRCS bit.c decoder.c fixed.c frame.c huffman.c layer12.c layer3.c stream.c
  synth.c timer.c version.c
  INCLUDE_DIRS "${CMAKE_BINARY_DIR}/include"
  PRIV_INCLUDE_DIRS ".")

set (MAD_H "${CMAKE_BINARY_DIR}/include/mad.h")
configure_file( "${COMPONENT_DIR}/config.h.in" "${CMAKE_BINARY_DIR}/include/config.h" )
configure_file( "${COMPONENT_DIR}/mad.h.in" "${MAD_H}" )

set(EXPORTED_HEADERS
   detect_fpm.h
   version.h
   fixed.h
   bit.h
   timer.h
   stream.h
   frame.h
   synth.h
   decoder.h
)

foreach( header ${EXPORTED_HEADERS} )
   file( READ "${COMPONENT_DIR}/${header}" HEADER_DATA )
   string( REPLACE "# include" "// # include" HEADER_DATA_REPLACED "${HEADER_DATA}")
   file( APPEND ${MAD_H} "// \"${header}\"\n\n${HEADER_DATA_REPLACED}\n" )
endforeach()

file( APPEND ${MAD_H} "# ifdef __cplusplus\n}\n# endif\n#endif\n" )

target_compile_definitions(${COMPONENT_LIB} 
   PRIVATE
      HAVE_CONFIG_H
      ${FPM}
)

target_compile_options(${COMPONENT_LIB} 
   PRIVATE
      $<$<C_COMPILER_ID:AppleClang,Clang,GNU>:-Wall>
      $<$<C_COMPILER_ID:AppleClang,Clang,GNU>:-Wno-implicit-function-declaration>
      $<$<C_COMPILER_ID:AppleClang,Clang,GNU>:-Wno-stringop-overflow>
      $<$<C_COMPILER_ID:AppleClang,Clang,GNU>:-fPIC>
      -Ofast
)

