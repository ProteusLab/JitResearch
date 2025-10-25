add_library(prot_jit_factory STATIC factory.cc)

target_link_libraries(
  prot_jit_factory
  PUBLIC PROT::exec_engine PROT::JIT::xbyak PROT::JIT::asmjit PROT::JIT::llvmbased
  PRIVATE PROT::defaults)

target_include_directories(prot_jit_factory PUBLIC include)

add_library(PROT::JIT::factory ALIAS prot_jit_factory)
