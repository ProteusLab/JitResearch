#ifndef PROT_JIT_FACTORY_HH_INCLUDED
#define PROT_JIT_FACTORY_HH_INCLUDED

#include "prot/exec_engine.hh"
#include "prot/jit/asmjit.hh"
#include "prot/jit/xbyak.hh"

#include <functional>
#include <string>
#include <unordered_map>

namespace prot::engine {

class JitFactory {
public:
  static constexpr auto kAsmJitName = "asmjit";
  static constexpr auto kXbyakJitName = "xbyak";

  static std::unique_ptr<ExecEngine> createEngine(const std::string &backend);
  static bool exist(const std::string &backend);

private:
  static const std::unordered_map<std::string,
                                  std::function<std::unique_ptr<ExecEngine>()>>
      kFactories;
};

} // namespace prot::engine

#endif // PROT_JIT_FACTORY_HH_INCLUDED
