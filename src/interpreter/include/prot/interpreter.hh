#ifndef PROT_INTERPRETER_HH_INCLUDED
#define PROT_INTERPRETER_HH_INCLUDED

#include <array>

#include "prot/exec_engine.hh"

namespace prot::engine {
class Interpreter : public ExecEngine {
public:
  void execute(CPUState &cpu, const isa::Instruction &insn) override;
};
} // namespace prot::engine

#endif // PROT_INTERPRETER_HH_INCLUDED
