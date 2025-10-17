#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Error.h"
#include <algorithm>
#include <cassert>
#include <functional>
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include "llvm/IR/LegacyPassManager.h"
#include <llvm/IR/LLVMContext.h>
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Transforms/Scalar/GVN.h"
#include <memory>
#include <string>
#include <utility>

#include "prot/cpu_state.hh"
#include "prot/exec_engine.hh"
#include "prot/isa.hh"
#include "prot/jit/base.hh"
#include "prot/jit/llvmbasedjitbuilder.hh"



namespace prot::engine {
namespace {
class LLVMBasedJIT : public JitEngine {
  std::unique_ptr<llvm::orc::LLJIT> m_jit;

  using TBFunc = void(*)(CPUState&);

public:
  LLVMBasedJIT(std::unique_ptr<llvm::orc::LLJIT> JIT)
    : m_jit(std::move(JIT)) {}

private:
  bool doJIT(CPUState &state) override {
    const auto pc = state.getPC();
    auto found = m_jit->lookup(std::to_string(pc));
    if (found) {
      std::invoke(found->toPtr<TBFunc>(), state);
      return true;
    }

    const auto *bbInfo = getBBInfo(pc);
    if (bbInfo == nullptr) {
      return false;
    }

    auto [module, ctx] = translate(pc, *bbInfo);
    llvm::orc::ThreadSafeModule tsm(std::move(module), std::move(ctx));
    tsm = optimizeIRModule(std::move(tsm));
    auto err = m_jit->addIRModule(std::move(tsm));
    if (err) {
      return false;
    }

    found = m_jit->lookup(std::to_string(pc));
    assert(found);
    std::invoke(found->toPtr<TBFunc>(), state);
    return true;
  }

  static std::pair<std::unique_ptr<llvm::Module>, std::unique_ptr<llvm::LLVMContext>> translate(isa::Word pc, const BBInfo &info);
  static llvm::orc::ThreadSafeModule optimizeIRModule(llvm::orc::ThreadSafeModule TSM);
};

llvm::orc::ThreadSafeModule LLVMBasedJIT::optimizeIRModule(llvm::orc::ThreadSafeModule TSM) {
  TSM.withModuleDo([](llvm::Module &M) {
    auto funcPM = std::make_unique<llvm::legacy::FunctionPassManager>(&M);
    funcPM->add(llvm::createInstructionCombiningPass());
    funcPM->add(llvm::createReassociatePass());
    funcPM->add(llvm::createGVNPass());
    funcPM->add(llvm::createCFGSimplificationPass());
    funcPM->doInitialization();
    for (auto &f : M) funcPM->run(f);
  });
  return TSM;
}

std::pair<std::unique_ptr<llvm::Module>, std::unique_ptr<llvm::LLVMContext>> LLVMBasedJIT::translate(const isa::Word pc, const BBInfo &info) {
  auto ctxPtr = std::make_unique<llvm::LLVMContext>();
  auto modulePtr = std::make_unique<llvm::Module>(std::to_string(pc), *ctxPtr);
  llvm::IRBuilder<> builder{*ctxPtr};

  auto *fnTy = llvm::FunctionType::get(llvm::Type::getVoidTy(*ctxPtr), {}, false);
  auto *fn   = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, std::to_string(pc), *modulePtr);

  IRData data{.Module=*modulePtr, .Builder=builder, .CurrentFunction=fn, .MemoryFunctions={}};

  std::ranges::for_each(info.insns, std::bind_front(buildInstruction, data));

  return std::make_pair(std::move(modulePtr), std::move(ctxPtr));
}
} // end anonymouse namespace

llvm::Expected<std::unique_ptr<ExecEngine>> makeLLVMBasedJIT() {
  auto jitOrErr = llvm::orc::LLJITBuilder().create();
  if (!jitOrErr) return jitOrErr.takeError();
  return std::make_unique<LLVMBasedJIT>(std::move(*jitOrErr));
}

} // end namespace prot::engine
