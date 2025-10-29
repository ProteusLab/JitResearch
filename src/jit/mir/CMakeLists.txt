
add_library(prot_mir OBJECT ${mir_SOURCE_DIR}/mir.c
                            ${mir_SOURCE_DIR}/mir-gen.c
                            ${mir_SOURCE_DIR}/mir.h
                            ${mir_SOURCE_DIR}/mir-gen.h)

add_library(prot_mir_static STATIC)
target_link_libraries(prot_mir_static PRIVATE prot_mir)

add_library(prot_jit_mir STATIC mir.cc)

add_library(mir_ext INTERFACE)
target_include_directories(mir_ext SYSTEM INTERFACE ${mir_SOURCE_DIR})

target_link_libraries(
  prot_jit_mir
  PUBLIC PROT::isa PROT::exec_engine
  PRIVATE PROT::defaults fmt::fmt prot_mir mir_ext PROT::JIT::base)
target_include_directories(prot_jit_mir PUBLIC include)

add_library(PROT::JIT::mir ALIAS prot_jit_mir)
