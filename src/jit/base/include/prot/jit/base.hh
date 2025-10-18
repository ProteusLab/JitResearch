#ifndef INCLUDE_JIT_BASE_HH_INCLUDED
#define INCLUDE_JIT_BASE_HH_INCLUDED

#include "prot/interpreter.hh"

#include <unordered_map>
#include <vector>

namespace prot::engine {
class JitEngine : public Interpreter {
  static constexpr std::size_t kExecThreshold = 0;

public:
  void step(CPUState &cpu) override;

protected:
  // simple bb counting
  struct BBInfo final {
    std::vector<isa::Instruction> insns;
    std::size_t num_exec{};
  };
  [[nodiscard]] const BBInfo *getBBInfo(isa::Addr pc) const;

private:
  void interpret(CPUState &cpu, BBInfo &info);
  void execute(CPUState &cpu, const isa::Instruction &insn) final {
    Interpreter::execute(cpu, insn);
  }

private:
  virtual bool doJIT(CPUState &state) = 0;

  std::unordered_map<isa::Addr, BBInfo> m_cacheBB;
};
} // namespace prot::engine

#endif // INCLUDE_JIT_BASE_HH_INCLUDED
