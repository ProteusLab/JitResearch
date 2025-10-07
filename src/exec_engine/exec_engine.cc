#include "prot/exec_engine.hh"
#include "prot/memory.hh"
#include <stdexcept>

namespace prot {
void ExecEngine::step(CPUState &cpu) {
  const auto bytes = cpu.memory->read<isa::Word>(cpu.getPC());

  auto &&instr = isa::Instruction::decode(bytes);

  if (!instr.has_value())
    throw std::runtime_error{"Undefined instruction on decode"};

  execute(cpu, *instr);
  ++cpu.icount;
}
} // namespace prot
