add_library(prot_cpu_state STATIC cpu_state.cc)
target_link_libraries(prot_cpu_state PUBLIC PROT::isa PRIVATE PROT::defaults)
target_include_directories(prot_cpu_state PUBLIC include)

add_library(PROT::cpu_state ALIAS prot_cpu_state)
