find_program(MAKE_EXECUTABLE NAMES make REQUIRED)

set(IR_INSTALL_DIR ${ir_BINARY_DIR})
set(IR_LIBRARY
    "${IR_INSTALL_DIR}/lib/${CMAKE_STATIC_LIBRARY_PREFIX}ir${CMAKE_STATIC_LIBRARY_SUFFIX}"
)

ExternalProject_Add(
  ir_makefile_build
  SOURCE_DIR ${ir_SOURCE_DIR}
  UPDATE_DISCONNECTED TRUE # Do not check git updates
  BUILD_IN_SOURCE 1
  CONFIGURE_COMMAND ""
  BUILD_COMMAND ${MAKE_EXECUTABLE} CC=${CMAKE_C_COMPILER} -j CFLAGS=""
  INSTALL_COMMAND ${MAKE_EXECUTABLE} PREFIX=${IR_INSTALL_DIR} install
  LOG_CONFIGURE False
  LOG_BUILD True
  LOG_MERGED_STDOUTERR False
  LOG_OUTPUT_ON_FAILURE True
  BUILD_BYPRODUCTS ${IR_LIBRARY})

set(IR_INCLUDE_DIR "${IR_INSTALL_DIR}/include")
make_directory(${IR_INCLUDE_DIR})

add_library(ir_iface INTERFACE)
target_include_directories(ir_iface INTERFACE SYSTEM ${IR_INCLUDE_DIR})
target_link_libraries(ir_iface INTERFACE ${IR_LIBRARY})
add_dependencies(ir_iface ir_makefile_build)
set_target_properties(ir_iface PROPERTIES EXCLUDE_FROM_ALL True)

add_library(prot_jit_ir STATIC ir.cc)
target_link_libraries(
  prot_jit_ir
  PUBLIC PROT::isa PROT::exec_engine
  PRIVATE PROT::defaults ir_iface PROT::JIT::base)
target_include_directories(prot_jit_ir PUBLIC include)

add_library(PROT::JIT::ir ALIAS prot_jit_ir)
