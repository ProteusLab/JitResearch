add_library(prot_hart STATIC hart.cc)
target_link_libraries(
  prot_hart
  PUBLIC PROT::isa PROT::cpu_state PROT::memory PROT::exec_engine PROT::elf_loader
  PRIVATE PROT::defaults)
target_include_directories(prot_hart PUBLIC include)

add_library(PROT::hart ALIAS prot_hart)
