add_library(mir_ext INTERFACE)
target_include_directories(mir_ext SYSTEM INTERFACE ${mir_SOURCE_DIR})

add_library(prot_jit_mir STATIC mir.cc)
target_link_libraries(
  prot_jit_mir
  PUBLIC PROT::isa PROT::exec_engine
  PRIVATE PROT::defaults fmt::fmt mir_static mir_ext PROT::JIT::base)
target_include_directories(prot_jit_mir PUBLIC include)

add_library(PROT::JIT::mir ALIAS prot_jit_mir)
