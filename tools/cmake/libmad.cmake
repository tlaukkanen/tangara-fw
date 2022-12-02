set(LIBMAD_SRC "$ENV{PROJ_PATH}/lib/libmad")
set(LIBMAD_BIN "${CMAKE_CURRENT_BINARY_DIR}/libmad")
set(LIBMAD_INCLUDE "${LIBMAD_BIN}/include")

externalproject_add(libmad_build
  SOURCE_DIR "${LIBMAD_SRC}"
  PREFIX "${LIBMAD_BIN}"
  CONFIGURE_COMMAND ${LIBMAD_SRC}/configure CC=${CMAKE_C_COMPILER} --srcdir=${LIBMAD_SRC} --prefix=${LIBMAD_BIN} --host=xtensa-elf --disable-debugging --disable-shared
  BUILD_COMMAND make
  INSTALL_COMMAND make install
  BUILD_BYPRODUCTS "${LIBMAD_BIN}/lib/libmad.a" "${LIBMAD_INCLUDE}/mad.h"
)

 add_library(libmad STATIC IMPORTED GLOBAL)
 add_dependencies(libmad libmad_build)

 set_target_properties(libmad PROPERTIES IMPORTED_LOCATION
   "${LIBMAD_BIN}/lib/libmad.a")
 set_target_properties(libmad PROPERTIES INTERFACE_INCLUDE_DIRECTORIES
   "${LIBMAD_INCLUDE}")
