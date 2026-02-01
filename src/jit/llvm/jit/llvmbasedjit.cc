#include "llvm-c/Target.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/Mangling.h"
#include "llvm/ExecutionEngine/Orc/Shared/ExecutorAddress.h"
#include "llvm/ExecutionEngine/Orc/Shared/ExecutorSymbolDef.h"
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LegacyPassManager.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Error.h"
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <functional>
#include <iostream>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include <llvm/IR/LLVMContext.h>
#include <llvm/Support/raw_ostream.h>
#include <memory>
#include <string>
#include <utility>

#include "prot/cpu_state.hh"
#include "prot/exec_engine.hh"
#include "prot/isa.hh"
#include "prot/jit/base.hh"
#include "prot/llvm/builder.hh"

namespace prot::engine {
namespace {
class LLVMBasedJIT : public JitEngine {
  std::unique_ptr<llvm::orc::LLJIT> m_jit;
  std::size_t m_moduleId{};

  using TBFunc = void (*)(CPUState &);

public:
  LLVMBasedJIT(std::unique_ptr<llvm::orc::LLJIT> JIT);

private:
  JitFunction translate(const BBInfo &info) override {
    auto name = std::to_string(m_moduleId++);
    auto &&[ctx, module] = ll::translate(name, info.insns);
    llvm::orc::ThreadSafeModule tsm(std::move(module), std::move(ctx));

    optimizeIRModule(tsm);

    auto err = m_jit->addIRModule(std::move(tsm));
    assert(!err);

    return *m_jit->lookup(name)->toPtr<JitFunction>();
  }

  static void optimizeIRModule(llvm::orc::ThreadSafeModule &TSM);
};

void LLVMBasedJIT::optimizeIRModule(llvm::orc::ThreadSafeModule &TSM) {
  TSM.withModuleDo([](llvm::Module &M) {
    auto funcPM = std::make_unique<llvm::legacy::FunctionPassManager>(&M);
    funcPM->add(llvm::createInstructionCombiningPass());
    funcPM->add(llvm::createReassociatePass());
    funcPM->add(llvm::createGVNPass());
    funcPM->add(llvm::createCFGSimplificationPass());
    funcPM->doInitialization();
    for (auto &f : M)
      funcPM->run(f);
  });
}

LLVMBasedJIT::LLVMBasedJIT(std::unique_ptr<llvm::orc::LLJIT> JIT)
    : m_jit(std::move(JIT)) {
  auto &jdExpected = m_jit->getMainJITDylib();

  llvm::orc::SymbolMap mySymbolMap;
  for (const auto &[name, addr] : ll::getFuncMapper()) {
    mySymbolMap.try_emplace(m_jit->getExecutionSession().intern(name),
                            llvm::orc::ExecutorAddr::fromPtr(addr),
                            llvm::JITSymbolFlags::Callable);
  }

  if (auto err = jdExpected.define(
          llvm::orc::absoluteSymbols(std::move(mySymbolMap)))) {
    std::string msg;
    llvm::raw_string_ostream{msg} << err;
    throw std::runtime_error{"Failed to add special functions to jit: " + msg};
  }
}

} // namespace

std::unique_ptr<ExecEngine> makeLLVMBasedJIT() {
  LLVMInitializeNativeTarget();
  LLVMInitializeNativeAsmPrinter();
  LLVMInitializeNativeAsmParser();
  auto jitOrErr = llvm::orc::LLJITBuilder().create();
  return std::make_unique<LLVMBasedJIT>(std::move(*jitOrErr));
}

} // end namespace prot::engine
