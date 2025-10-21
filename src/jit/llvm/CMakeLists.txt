# adding normal llvmbased-jit library

llvm_map_components_to_libnames(
  LLVM_LIBRARIES
  Core
  OrcJIT
  Support
  Core
  IRReader
  X86AsmParser
  native
  x86asmparser
)

add_library(prot_jit_llvmbased STATIC llvmbasedjit.cc llvmbasedjitbuilder.cc)
target_link_libraries(
  prot_jit_llvmbased
  PUBLIC PROT::isa PROT::exec_engine ${LLVM_LIBRARIES}
  PRIVATE PROT::defaults fmt::fmt PROT::JIT::base)
target_include_directories(prot_jit_llvmbased SYSTEM PUBLIC include ${LLVM_INCLUDE_DIRS})

add_library(PROT::JIT::llvmbased ALIAS prot_jit_llvmbased)
