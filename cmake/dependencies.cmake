# CLI11: Command Line Input argument parser library
CPMAddPackage(
  NAME CLI11
  GITHUB_REPOSITORY CLIUtils/CLI11
  VERSION 2.5.0
  EXCLUDE_FROM_ALL True
  SYSTEM True)

# fmt: Format Library
CPMAddPackage(
  NAME fmt
  GITHUB_REPOSITORY fmtlib/fmt
  GIT_TAG 12.0.0
  EXCLUDE_FROM_ALL True
  SYSTEM True)

# elfio: ELF Loader Library
CPMAddPackage(
  NAME elfio
  GITHUB_REPOSITORY serge1/ELFIO
  GIT_TAG Release_3.12
  EXCLUDE_FROM_ALL True
  SYSTEM True)

# gtest
CPMAddPackage(
  NAME googletest
  GITHUB_REPOSITORY google/googletest
  VERSION 1.17.0
  OPTIONS "INSTALL_GTEST OFF"
  EXCLUDE_FROM_ALL True
  SYSTEM True)

# xbyak
CPMAddPackage(
  NAME xbyak
  GITHUB_REPOSITORY herumi/xbyak
  VERSION 7.30
  EXCLUDE_FROM_ALL True
  SYSTEM True
  DOWNLOAD_ONLY)

# llvm
find_package(LLVM 18.1.3 CONFIG REQUIRED)

# asmjit
CPMAddPackage(
  NAME asmjit
  GITHUB_REPOSITORY asmjit/asmjit
  GIT_TAG 0b3aec39d18a98a87449f031a469b60aedae1a9b
  EXCLUDE_FROM_ALL True
  SYSTEM True
  OPTIONS "ASMJIT_STATIC ON"
  OPTIONS "ASMJIT_STATIC ON"
)
