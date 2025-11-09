cmake_minimum_required(VERSION 3.22 FATAL_ERROR)

project(prot_jit LANGUAGES C CXX)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set_property(CACHE CMAKE_INSTALL_PREFIX
               PROPERTY VALUE "${CMAKE_BINARY_DIR}/install")
endif()

option(PROT_ENABLE_WERROR "Enable -Werror option (CI)" OFF)
option(PROT_BUILD_BENCHMARKS
       "Enable benchmarks build (requires riscv gnu toolchain)" OFF)
enable_testing()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

include(CPM)
# Provide default CXX settings
include(defaults)
include(dependencies)
include(utils)

add_subdirectory(third_party)
add_subdirectory(src)
add_subdirectory(tools)

if(PROT_BUILD_BENCHMARKS)
  set(RISCV_TOOLCHAIN ${CMAKE_CURRENT_SOURCE_DIR}/cmake/toolchain/riscv.cmake)
  set(DEFAULT_ARGS
      -DCMAKE_INSTALL_PREFIX:PATH=${CMAKE_INSTALL_PREFIX}
      -DCMAKE_BUILD_TYPE:STRING=Release
      -DCMAKE_TOOLCHAIN_FILE:PATH=${RISCV_TOOLCHAIN})
  if (DEFINED RISCV_TOOLCHAIN_DIR)
    list(APPEND DEFAULT_ARGS -DRISCV_TOOLCHAIN_DIR:PATH=${RISCV_TOOLCHAIN_DIR})
  endif()
  ExternalProject_Add(
    benchmarks
    SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/benchmarks
    BINARY_DIR ${CMAKE_CURRENT_BINARY_DIR}/benchmarks
    CONFIGURE_HANDLED_BY_BUILD True
    BUILD_ALWAYS True
    CMAKE_ARGS ${DEFAULT_ARGS})
endif()
