#ifndef PROT_EXEC_ENGINE_HH_INCLUDED
#define PROT_EXEC_ENGINE_HH_INCLUDED

#include "prot/cpu_state.hh"
#include "prot/isa.hh"
#include "prot/memory.hh"

namespace prot {
struct ExecEngine {
  virtual ~ExecEngine() = default;

  virtual void execute(CPUState &cpu, const isa::Instruction &insn) = 0;
  virtual void step(CPUState &cpu);
};
} // namespace prot

#endif // PROT_EXEC_ENGINE_HH_INCLUDED
