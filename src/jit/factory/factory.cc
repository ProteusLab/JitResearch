#include "include/prot/jit/factory.hh"

#include <algorithm>
#include <ranges>

#include "prot/jit/asmjit.hh"
#include "prot/jit/base.hh"
#include "prot/jit/lightning.hh"
#include "prot/jit/llvmbasedjit.hh"
#include "prot/jit/mir.hh"
#include "prot/jit/tpde.hh"
#include "prot/jit/xbyak.hh"

namespace prot::engine {
const std::unordered_map<std::string_view,
                         std::function<std::unique_ptr<Translator>()>>
    JitFactory::kFactories = {
        {"xbyak", []() { return makeXbyak(); }},
        {"asmjit", []() { return makeAsmJit(); }},
        {"cached-interp", []() { return std::unique_ptr<Translator>(); }},
        {"llvm", []() { return makeLLVMBasedJIT(); }},
        {"lightning", []() { return makeLightning(); }},
        {"mir", []() { return makeMirJit(); }},
        {"tpde", []() { return makeTPDE(); }}};

std::vector<std::string_view> JitFactory::backends() {
  std::vector<std::string_view> res(kFactories.size());
  std::ranges::copy(kFactories | std::views::keys, res.begin());
  return res;
}

std::unique_ptr<Translator>
JitFactory::createTranslator(const std::string &backend) {
  if (const auto it = kFactories.find(backend); it != kFactories.end()) {
    return it->second();
  }

  throw std::invalid_argument("Undefined JIT backend: " + backend);
}

bool JitFactory::exist(const std::string &backend) {
  return kFactories.contains(backend);
}
} // namespace prot::engine
