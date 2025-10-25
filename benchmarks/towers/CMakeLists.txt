project(towers_bench C)

set(SRC ${CMAKE_CURRENT_SOURCE_DIR}/towers_main.c)
set(BIN ${CMAKE_CURRENT_BINARY_DIR}/towers.out)

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

add_custom_target(build_towers_riscv32
  DEPENDS ${BIN}
)

add_custom_target(run_towers_riscv32_bench_llvm
  COMMAND $<TARGET_FILE:prot_sim_app> --jit ${BIN}
  DEPENDS build_towers_riscv32 prot_sim_app
  COMMENT "Running ${BIN} with PSIM JIT"
)

add_custom_target(run_towers_riscv32_bench
  COMMAND $<TARGET_FILE:prot_sim_app> ${BIN}
  DEPENDS build_towers_riscv32 prot_sim_app
  COMMENT "Running ${BIN} with PSIM JIT"
)

