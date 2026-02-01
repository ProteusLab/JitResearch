llvm_map_components_to_libnames(
  LLVM_LIBRARIES
  Core
  OrcJIT
  Support
  IRReader
  X86AsmParser
  native
  x86asmparser)

add_library(prot_llvm_builder STATIC builder.cc)
target_link_libraries(
  prot_llvm_builder
  PUBLIC PROT::isa ${LLVM_LIBRARIES}
  PRIVATE PROT::defaults fmt::fmt PROT::cpu_state PROT::memory)
target_include_directories(prot_llvm_builder SYSTEM PUBLIC include
                                                           ${LLVM_INCLUDE_DIRS})

add_library(PROT::LLVM::builder ALIAS prot_llvm_builder)
