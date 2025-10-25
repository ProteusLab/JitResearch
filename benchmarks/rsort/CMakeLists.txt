project(rsort_bench C)

set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/rsort.c)
set(BIN ${CMAKE_CURRENT_BINARY_DIR}/rsort.out)

add_custom_command(
  OUTPUT ${BIN}
  COMMAND ${CMAKE_C_COMPILER}
    -target riscv32-unknown-elf
    -march=rv32i
    ${SRC}
    -nostdlib
    -e main
    -o ${BIN}
  DEPENDS ${SRC}
  COMMENT "Building ${BIN} for riscv32"
)

add_custom_target(build_rsort_riscv32
  DEPENDS ${BIN}
)

add_custom_target(run_rsort_riscv32_bench_llvm
  COMMAND $<TARGET_FILE:prot_sim_app> --jit ${BIN}
  DEPENDS build_rsort_riscv32 prot_sim_app
  COMMENT "Running ${BIN} with PSIM JIT"
)

add_custom_target(run_rsort_riscv32_bench
  COMMAND $<TARGET_FILE:prot_sim_app> ${BIN}
  DEPENDS build_rsort_riscv32 prot_sim_app
  COMMENT "Running ${BIN} with PSIM JIT"
)

