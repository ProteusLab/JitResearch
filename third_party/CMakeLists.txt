# Lightning build & configure

include(ExternalProject)

find_program(MAKE_EXECUTABLE NAMES make REQUIRED)

set(LIGHTNING_INSTALL_DIR ${CMAKE_CURRENT_BINARY_DIR}/lightning/install)

set(LIGHTNING_LIBRARY
    "${LIGHTNING_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}lightning${CMAKE_STATIC_LIBRARY_SUFFIX}"
)

ExternalProject_Add(
  lightning_lib
  SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/lightning
  PREFIX ${LIGHTNING_INSTALL_DIR}
  UPDATE_DISCONNECTED TRUE # Do not check git updates
  # Configure: run autotools
  CONFIGURE_COMMAND
    ./bootstrap && ./configure --prefix=${LIGHTNING_INSTALL_DIR} CFLAGS=-O2
    --disable-shared CC=${CMAKE_C_COMPILER} --disable-disassembler
  # Build & install
  BUILD_COMMAND ${MAKE_EXECUTABLE} -j
  INSTALL_COMMAND ${MAKE_EXECUTABLE} install
  LOG_CONFIGURE False
  LOG_BUILD False
  LOG_MERGED_STDOUTERR False
  LOG_OUTPUT_ON_FAILURE True
  BUILD_IN_SOURCE 1
  BUILD_BYPRODUCTS ${LIGHTNING_LIBRARY})

set(LIGHTNING_INCLUDE_DIR "${LIGHTNING_INSTALL_DIR}/include")
make_directory(${LIGHTNING_INCLUDE_DIR})

add_library(lightning_iface INTERFACE)
target_include_directories(lightning_iface INTERFACE ${LIGHTNING_INCLUDE_DIR})
target_link_libraries(lightning_iface INTERFACE ${LIGHTNING_LIBRARY})
add_dependencies(lightning_iface lightning_lib)
set_target_properties(lightning_iface PROPERTIES EXCLUDE_FROM_ALL True)
add_library(PROT::gnu_lightning ALIAS lightning_iface)
