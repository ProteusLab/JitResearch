add_library(prot_jit_asmjit STATIC asmjit.cc)
target_link_libraries(
  prot_jit_asmjit
  PUBLIC PROT::isa PROT::exec_engine
  PRIVATE PROT::defaults fmt::fmt asmjit PROT::JIT::base)
target_include_directories(prot_jit_asmjit PUBLIC include)

add_library(PROT::JIT::asmjit ALIAS prot_jit_asmjit)
