# adding normal llvmbased-jit library

llvm_map_components_to_libnames(
  LLVM_LIBRARIES
  Core
  OrcJIT
  Support
  NativeCodeGen
  Core
  IRReader
  Native
  X86AsmParser
)

add_library(llvmbased_ext INTERFACE)
target_include_directories(llvmbased_ext SYSTEM INTERFACE ${llvmbased_SOURCE_DIR})

add_library(prot_jit_llvmbased STATIC llvmbasedjit.cc llvmbasedjitbuilder.cc)
target_link_libraries(
  prot_jit_llvmbased
  PUBLIC PROT::isa PROT::exec_engine ${LLVM_LIBRARIES}
  PRIVATE PROT::defaults fmt::fmt llvmbased_ext PROT::JIT::base)
target_include_directories(prot_jit_llvmbased PUBLIC include ${LLVM_INCLUDE_DIRS})

add_library(PROT::JIT::llvmbased ALIAS prot_jit_llvmbased)
