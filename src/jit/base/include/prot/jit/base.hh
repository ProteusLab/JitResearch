#ifndef INCLUDE_JIT_BASE_HH_INCLUDED
#define INCLUDE_JIT_BASE_HH_INCLUDED

#include "prot/interpreter.hh"

#include <functional>
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

class CachedInterpreter final : public JitEngine {
  bool doJIT([[maybe_unused]] CPUState &stat) override { return false; }
};

// Helper class to store JITed code
// Especially helpful for libraries w/out propper mem pool support

using JitFunction = void (*)(CPUState &);
class CodeHolder final {
  struct Unmap {
    std::size_t m_size = 0;

  public:
    explicit Unmap(std::size_t size) noexcept : m_size(size) {}

    void operator()(void *ptr) const noexcept;
  };

public:
  explicit CodeHolder(std::span<const std::byte> src);

  template <typename T> [[nodiscard]] auto as() const {
    return reinterpret_cast<T>(m_data.get());
  }
  void operator()(CPUState &state) const { as<JitFunction>()(state); }

private:
  std::unique_ptr<std::byte, Unmap> m_data;
};
} // namespace prot::engine

#endif // INCLUDE_JIT_BASE_HH_INCLUDED
