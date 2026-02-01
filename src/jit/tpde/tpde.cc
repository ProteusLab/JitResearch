#include "prot/jit/tpde.hh"
#include "prot/jit/base.hh"
#include "prot/llvm/builder.hh"

#include <iostream>
#include <ranges>

#include <llvm/TargetParser/Host.h>

#include <forward_list>

#include <fmt/core.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>

#include <tpde-llvm/OrcCompiler.hpp>

namespace prot::engine {

namespace {

class TPDEJit final : public JitEngine {
public:
  TPDEJit()
      : m_jit([] {
          llvm::Triple triple{llvm::sys::getProcessTriple()};
          return tpde_llvm::LLVMCompiler::create(triple);
        }()) {}

  JitFunction translate(const BBInfo &info) override {
    auto name = std::to_string(m_moduleId++);
    const auto &[ctx, module] = ll::translate(name, info.insns);

    auto *func = module->getFunction(name);
    m_mappers.push_front(
        m_jit->compile_and_map(*module, [](std::string_view sv) {
          // fmt::println(std::cerr, "SYM: {}", sv);
          const auto &mapper = ll::getFuncMapper();
          return mapper.at(sv);
        }));

    void *ptr = m_mappers.front().lookup_global(func);
    if (ptr == nullptr) {
      throw std::runtime_error{"Failed to find entry function in TPDE"};
    }

    return reinterpret_cast<JitFunction>(ptr);
  }

private:
  std::unique_ptr<tpde_llvm::LLVMCompiler> m_jit;
  std::forward_list<tpde_llvm::JITMapper> m_mappers;
  std::size_t m_moduleId{};

  using TBFunc = void (*)(CPUState &);
};
} // namespace

std::unique_ptr<ExecEngine> makeTPDE() { return std::make_unique<TPDEJit>(); }
} // namespace prot::engine
