#include <asmjit/asmjit.h>

#include "include/prot/jit/asmjit.hh"
#include "prot/jit/base.hh"

#include <functional>

#include <cassert>
#include <span>

#include <sys/mman.h>

#include <fmt/core.h>

namespace prot::engine {
namespace {

using JitFunction = void (*)(CPUState &);

class AsmJit : public JitEngine {
public:
  AsmJit() = default;

private:
  bool doJIT(CPUState &state) override {
    const auto pc = state.getPC();
    auto found = m_cacheTB.find(pc);
    if (found != m_cacheTB.end()) {
      found->second(state);
      return true;
    }

    const auto *bbInfo = getBBInfo(pc);
    if (bbInfo == nullptr) {
      // No such bb yet
      return false;
    }

    auto holder = translate(*bbInfo);

    auto [it, wasNew] = m_cacheTB.try_emplace(pc, std::move(holder));
    assert(wasNew);

    it->second(state);

    return true;
  }

  [[nodiscard]] JitFunction translate(const BBInfo &info);
  std::unordered_map<isa::Addr, JitFunction> m_cacheTB;
};

void storeHelper(CPUState &state, isa::Addr addr,
                 std::unsigned_integral auto val) {
  state.memory->write(addr, val);
}

void writeHelper(CPUState &state, isa::Addr addr,
                 std::unsigned_integral auto val) {
  state.memory->write(addr, val);
}

JitFunction AsmJit::translate(const BBInfo &info) {

  for ([[maybe_unused]] const auto &insn : info.insns) {
    // main logic
  }

  return JitFunction{};
} // namespace prot::engine
} // namespace

std::unique_ptr<ExecEngine> makeAsmJit() { return std::make_unique<AsmJit>(); }
} // namespace prot::engine
