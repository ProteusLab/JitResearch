add_library(prot_jit_base STATIC base.cc)
target_link_libraries(
  prot_jit_base
  PUBLIC PROT::isa PROT::interpreter
  PRIVATE PROT::defaults fmt::fmt)
target_include_directories(prot_jit_base PUBLIC include)

add_library(PROT::JIT::base ALIAS prot_jit_base)
