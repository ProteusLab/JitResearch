#include "include/prot/jit/factory.hh"

namespace prot::engine {
const std::unordered_map<std::string,
                         std::function<std::unique_ptr<ExecEngine>()>>
    JitFactory::kFactories = {
        {JitFactory::kXbyakJitName, []() { return makeXbyak(); }},
        {JitFactory::kAsmJitName, []() { return makeAsmJit(); }}};

std::unique_ptr<ExecEngine>
JitFactory::createEngine(const std::string &backend) {
  auto it = kFactories.find(backend);
  if (it != kFactories.end())
    return it->second();

  throw std::invalid_argument("Undefined JIT backend: " + backend);
}

bool JitFactory::exist(const std::string &backend) {
  return kFactories.find(backend) != kFactories.end();
}
} // namespace prot::engine
