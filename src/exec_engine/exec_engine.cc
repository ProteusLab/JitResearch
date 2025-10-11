#include "prot/exec_engine.hh"
#include "prot/memory.hh"
#include <stdexcept>

#include <fmt/core.h>

namespace prot {
void ExecEngine::step(CPUState &cpu) {
  auto pc = cpu.getPC();

  if (pc % sizeof(isa::Word) != 0) {
    throw std::runtime_error{"Misaligned PC detected"};
  }

  const auto bytes = cpu.memory->read<isa::Word>(cpu.getPC());

  auto &&instr = isa::Instruction::decode(bytes);

  if (!instr.has_value())
    throw std::runtime_error{
        fmt::format("Undefined instruction on decode: {:#x}", bytes)};

  execute(cpu, *instr);
  ++cpu.icount;
}
} // namespace prot
