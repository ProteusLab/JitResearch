add_library(prot_jit_tpde STATIC tpde.cc)
target_link_libraries(
  prot_jit_tpde
  PUBLIC PROT::isa PROT::exec_engine
  PRIVATE PROT::defaults fmt::fmt tpde::tpde_llvm PROT::JIT::base PROT::LLVM::builder)
target_include_directories(prot_jit_tpde PUBLIC include)

add_library(PROT::JIT::tpde ALIAS prot_jit_tpde)
