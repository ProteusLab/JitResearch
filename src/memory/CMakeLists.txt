add_library(prot_mem STATIC memory.cc plain_memory.cc)
target_link_libraries(
  prot_mem
  PUBLIC PROT::isa
  PRIVATE PROT::defaults fmt::fmt)
target_include_directories(prot_mem PUBLIC include)

add_library(PROT::memory ALIAS prot_mem)
