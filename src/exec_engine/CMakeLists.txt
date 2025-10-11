add_library(prot_exec_engine STATIC exec_engine.cc)
target_link_libraries(
  prot_exec_engine
  PUBLIC PROT::isa PROT::cpu_state PROT::memory
  PRIVATE PROT::defaults fmt::fmt)
target_include_directories(prot_exec_engine PUBLIC include)

add_library(PROT::exec_engine ALIAS prot_exec_engine)
