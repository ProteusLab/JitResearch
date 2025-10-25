#ifndef INCLUDE_PROT_JIT_FACTORY_HH_INCLUDED
#define INCLUDE_PROT_JIT_FACTORY_HH_INCLUDED

#include "prot/exec_engine.hh"

#include <functional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace prot::engine {

class JitFactory {
public:
  [[nodiscard]] static std::vector<std::string_view> backends();
  static std::unique_ptr<ExecEngine> createEngine(const std::string &backend);
  static bool exist(const std::string &backend);

private:
  static const std::unordered_map<std::string_view,
                                  std::function<std::unique_ptr<ExecEngine>()>>
      kFactories;
};

} // namespace prot::engine

#endif // INCLUDE_PROT_JIT_FACTORY_HH_INCLUDED
