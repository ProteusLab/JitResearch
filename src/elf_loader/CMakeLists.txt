add_library(prot_elf_loader STATIC elf_loader.cc)
target_link_libraries(
  prot_elf_loader
  PUBLIC PROT::memory PROT::isa
  PRIVATE PROT::defaults elfio::elfio fmt::fmt)
target_include_directories(prot_elf_loader PUBLIC include)

add_library(PROT::elf_loader ALIAS prot_elf_loader)
