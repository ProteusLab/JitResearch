add_custom_command(
  OUTPUT ${ir_SOURCE_DIR}/libir.a
  COMMAND ${CMAKE_MAKE_PROGRAM} CC=${CMAKE_C_COMPILER} -C ${ir_SOURCE_DIR} -f Makefile
  COMMENT "Building IR library using Makefile"
)

add_custom_target(ir_lib DEPENDS ${ir_SOURCE_DIR}/libir.a)

add_library(ir_ext STATIC IMPORTED)
set_target_properties(ir_ext PROPERTIES
  IMPORTED_LOCATION ${ir_SOURCE_DIR}/libir.a
  INTERFACE_INCLUDE_DIRECTORIES "${ir_SOURCE_DIR}"
)
add_dependencies(ir_ext ir_lib)

add_library(prot_jit_ir STATIC ir.cc)
target_link_libraries(prot_jit_ir
  PUBLIC PROT::isa PROT::exec_engine
  PRIVATE PROT::defaults ir_ext PROT::JIT::base
)
target_include_directories(prot_jit_ir PUBLIC include)

add_library(PROT::JIT::ir ALIAS prot_jit_ir)
