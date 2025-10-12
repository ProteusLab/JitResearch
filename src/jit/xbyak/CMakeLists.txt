# adding normal xbyak library

add_library(xbyak_ext INTERFACE)
target_include_directories(xbyak_ext SYSTEM INTERFACE ${xbyak_SOURCE_DIR})

add_library(prot_jit_xbyak STATIC xbyak.cc)
target_link_libraries(
  prot_jit_xbyak
  PUBLIC PROT::isa PROT::exec_engine
  PRIVATE PROT::defaults fmt::fmt xbyak_ext PROT::JIT::base)
target_include_directories(prot_jit_xbyak PUBLIC include)

add_library(PROT::JIT::xbyak ALIAS prot_jit_xbyak)
