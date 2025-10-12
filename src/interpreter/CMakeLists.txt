add_library(prot_interp STATIC interp.cc)
target_link_libraries(
  prot_interp
  PUBLIC PROT::isa PROT::exec_engine
  PRIVATE PROT::defaults fmt::fmt)
target_include_directories(prot_interp PUBLIC include)

add_library(PROT::interpreter ALIAS prot_interp)
