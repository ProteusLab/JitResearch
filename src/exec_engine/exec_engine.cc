#include "prot/exec_engine.hh"
#include "prot/memory.hh"

namespace prot {
void ExecEngine::step(CPUState &cpu) {
  const auto bytes = cpu.memory->read<isa::Word>(cpu.getPC());
  execute(cpu, isa::Instruction::decode(bytes));
  ++cpu.icount;
}
} // namespace prot
