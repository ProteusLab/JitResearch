add_library(prot_jit_lightning STATIC lightning.cc)
target_link_libraries(
  prot_jit_lightning
  PUBLIC PROT::isa PROT::exec_engine
  PRIVATE PROT::defaults fmt::fmt PROT::JIT::base PROT::gnu_lightning)
target_include_directories(prot_jit_lightning PUBLIC include)

add_library(PROT::JIT::lightning ALIAS prot_jit_lightning)
