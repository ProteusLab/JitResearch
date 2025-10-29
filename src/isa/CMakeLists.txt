add_library(prot_isa STATIC isa.cc)
add_subdirectory(tests)
target_include_directories(prot_isa PUBLIC include)
target_link_libraries(prot_isa PRIVATE PROT::defaults)

add_library(PROT::isa ALIAS prot_isa)
