# Adding this library Sorry for such creepy code

include(ExternalProject)

find_program(MAKE_EXECUTABLE NAMES make REQUIRED)

set(LIGHTNING_INSTALL_DIR ${lightning_BINARY_DIR}/install)

set(LIGHTNING_LIBRARY
    "${LIGHTNING_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}lightning${CMAKE_STATIC_LIBRARY_SUFFIX}"
)

ExternalProject_Add(
  lightning_lib
  SOURCE_DIR ${lightning_SOURCE_DIR}
  PREFIX ${LIGHTNING_INSTALL_DIR}
  UPDATE_DISCONNECTED TRUE # Do not check git updates
  # Configure: run autotools
  CONFIGURE_COMMAND
    ./configure --prefix=${LIGHTNING_INSTALL_DIR} CFLAGS=-O2 --disable-shared
    CC=${CMAKE_C_COMPILER} --disable-disassembler
  # Build & install
  BUILD_COMMAND ${MAKE_EXECUTABLE} -j
  INSTALL_COMMAND ${MAKE_EXECUTABLE} install
  LOG_CONFIGURE True
  LOG_BUILD True
  LOG_MERGED_STDOUTERR True
  LOG_OUTPUT_ON_FAILURE True
  BUILD_IN_SOURCE 1
  BUILD_BYPRODUCTS ${LIGHTNING_LIBRARY})

set(LIGHTNING_INCLUDE_DIR "${LIGHTNING_INSTALL_DIR}/include")
make_directory(${LIGHTNING_INCLUDE_DIR})

add_library(lightning_iface INTERFACE)
target_include_directories(lightning_iface INTERFACE ${LIGHTNING_INCLUDE_DIR})
target_link_libraries(lightning_iface INTERFACE ${LIGHTNING_LIBRARY})
add_dependencies(lightning_iface lightning_lib)

# ---------------------------------------------
add_library(prot_jit_lightning STATIC lightning.cc)
target_link_libraries(
  prot_jit_lightning
  PUBLIC PROT::isa PROT::exec_engine
  PRIVATE PROT::defaults fmt::fmt PROT::JIT::base lightning_iface)
target_include_directories(prot_jit_lightning PUBLIC include)

add_library(PROT::JIT::lightning ALIAS prot_jit_lightning)
