cmake_minimum_required(VERSION 3.22 FATAL_ERROR)

project(prot_jit)

if(CMAKE_INSTALL_PREFIX_INITIALIZED_TO_DEFAULT)
  set_property(CACHE CMAKE_INSTALL_PREFIX PROPERTY VALUE "${CMAKE_BINARY_DIR}/install")
endif()

option(PROT_ENABLE_WERROR "Enable -Werror option (CI)" OFF)
option(PROT_BUILD_TESTS "Enable tests building (CI)" OFF)

enable_testing()

include(cmake/CPM.cmake)
# Provide default CXX settings
include(cmake/defaults.cmake)
include(cmake/dependencies.cmake)
include(cmake/utils.cmake)


add_subdirectory(src)
add_subdirectory(tools)
