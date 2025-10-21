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
#include <llvm/IR/Mangler.h>
#include <llvm/Support/raw_ostream.h>
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
llvm::Type *getCPUStateType(llvm::LLVMContext &Ctx) {
  if (auto *type = llvm::StructType::getTypeByName(Ctx, "CPUState")) {
    return type;
  }
  llvm::Type *wordType = llvm::Type::getInt32Ty(Ctx);
  llvm::Type *pcType = wordType;
  llvm::Type *finishedType = llvm::Type::getInt1Ty(Ctx);
  llvm::Type *memoryPtrType = llvm::PointerType::get(Ctx, 0);
  llvm::Type *icountType = llvm::Type::getInt64Ty(Ctx);
  llvm::ArrayType *regsArrayType = llvm::ArrayType::get(wordType, 32);

  std::vector<llvm::Type *> structMemberTypes = {
      regsArrayType, pcType, finishedType, memoryPtrType, icountType};

  llvm::StructType *cpuStateType = llvm::StructType::create(
      Ctx, structMemberTypes, "CPUState", /*IsPacked=*/false);

  return cpuStateType;
}

template <typename T, bool Signed = true>
isa::Word loadHelper(isa::Imm addr, CPUState &state) {
  isa::Word loaded = state.memory->read<T>(addr);
  if constexpr (Signed) {
    loaded = isa::signExtend<sizeofBits<isa::Word>(), sizeofBits<T>()>(loaded);
  }
  return loaded;
}

template <typename T> void storeHelper(T val, isa::Imm addr, CPUState &state) {
  state.memory->write(addr, val);
}

isa::Word doLB(isa::Imm addr, CPUState &state) {
  return loadHelper<isa::Byte>(addr, state);
}

isa::Word doLBU(isa::Imm addr, CPUState &state) {
  return loadHelper<isa::Byte, false>(addr, state);
}

isa::Word doLH(isa::Imm addr, CPUState &state) {
  return loadHelper<isa::Half>(addr, state);
}

isa::Word doLHU(isa::Imm addr, CPUState &state) {
  return loadHelper<isa::Half, false>(addr, state);
}

isa::Word doLW(isa::Imm addr, CPUState &state) {
  return loadHelper<isa::Word>(addr, state);
}

void doSB(isa::Byte val, isa::Imm addr, CPUState &state) {
  storeHelper<isa::Byte>(val, addr, state);
}

void doSH(isa::Half val, isa::Imm addr, CPUState &state) {
  storeHelper<isa::Half>(val, addr, state);
}

void doSW(isa::Word val, isa::Imm addr, CPUState &state) {
  storeHelper<isa::Word>(val, addr, state);
}

void doSyscall(CPUState &state) { state.emulateSysCall(); }

class LLVMBasedJIT : public JitEngine {
  std::unique_ptr<llvm::orc::LLJIT> m_jit;

  using TBFunc = void (*)(CPUState &);

public:
  LLVMBasedJIT(std::unique_ptr<llvm::orc::LLJIT> JIT);

private:
  bool doJIT(CPUState &state) override {
    const auto pc = state.getPC();
    auto found = m_jit->lookup(std::to_string(pc));
    if (found) {
      std::invoke(found->toPtr<TBFunc>(), state);
      const auto *bbInfo = getBBInfo(pc);
      state.icount += bbInfo->insns.size();
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
    state.icount += bbInfo->insns.size();
    return true;
  }

  std::pair<std::unique_ptr<llvm::Module>, std::unique_ptr<llvm::LLVMContext>>
  translate(isa::Word pc, const BBInfo &info);
  static llvm::orc::ThreadSafeModule
  optimizeIRModule(llvm::orc::ThreadSafeModule TSM);
};

llvm::orc::ThreadSafeModule
LLVMBasedJIT::optimizeIRModule(llvm::orc::ThreadSafeModule TSM) {
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
  return TSM;
}

llvm::Function *getOrDeclareMemoryFunction(llvm::Module &M,
                                           llvm::IRBuilder<> &Builder,
                                           llvm::StringRef Name,
                                           bool isLoad = true) {
  if (auto *F = M.getFunction(Name)) {
    return F;
  }

  llvm::Type *cpuStatePtrTy = llvm::PointerType::get(M.getContext(), 0);
  llvm::Type *instPtrTy = llvm::PointerType::get(M.getContext(), 0);

  llvm::FunctionType *ft = llvm::FunctionType::get(
      isLoad ? llvm::Type::getInt32Ty(M.getContext()) : Builder.getVoidTy(),
      {isLoad ? llvm::Type::getInt32Ty(M.getContext()) : instPtrTy,
       cpuStatePtrTy},
      false);

  auto *F =
      llvm::Function::Create(ft, llvm::Function::ExternalLinkage, Name, M);
  return F;
}

std::pair<std::unique_ptr<llvm::Module>, std::unique_ptr<llvm::LLVMContext>>
LLVMBasedJIT::translate(const isa::Word pc, const BBInfo &info) {
  auto ctxPtr = std::make_unique<llvm::LLVMContext>();
  auto modulePtr = std::make_unique<llvm::Module>(std::to_string(pc), *ctxPtr);
  llvm::IRBuilder<> builder{*ctxPtr};

  auto *fnTy = llvm::FunctionType::get(
      llvm::Type::getVoidTy(*ctxPtr),
      {llvm::PointerType::getUnqual(getCPUStateType(*ctxPtr))}, false);
  auto *fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage,
                                    std::to_string(pc), *modulePtr);

  llvm::orc::MangleAndInterner mangle(m_jit->getExecutionSession(),
                                      m_jit->getDataLayout());

  llvm::Type *cpuStatePtrTy = llvm::PointerType::get(*ctxPtr, 0);
  llvm::FunctionType *ft =
      llvm::FunctionType::get(builder.getVoidTy(), {cpuStatePtrTy}, false);

  auto *F = llvm::Function::Create(ft, llvm::Function::ExternalLinkage,
                                   "doSyscall", *modulePtr);

  IRData data{
      .Module = *modulePtr,
      .Builder = builder,
      .CurrentFunction = fn,
      .MemoryFunctions = {
          getOrDeclareMemoryFunction(*modulePtr, builder, "doLB"),
          getOrDeclareMemoryFunction(*modulePtr, builder, "doLH"),
          getOrDeclareMemoryFunction(*modulePtr, builder, "doLW"),
          getOrDeclareMemoryFunction(*modulePtr, builder, "doLBU"),
          getOrDeclareMemoryFunction(*modulePtr, builder, "doLHU"),
          getOrDeclareMemoryFunction(*modulePtr, builder, "doSB", false),
          getOrDeclareMemoryFunction(*modulePtr, builder, "doSH", false),
          getOrDeclareMemoryFunction(*modulePtr, builder, "doSW", false),
          F,
      }};
  llvm::BasicBlock *entryBB = llvm::BasicBlock::Create(*ctxPtr, "entry", fn);
  builder.SetInsertPoint(entryBB);

  std::ranges::for_each(info.insns, std::bind_front(buildInstruction, data));
  data.Builder.CreateRetVoid();

  return std::make_pair(std::move(modulePtr), std::move(ctxPtr));
}

LLVMBasedJIT::LLVMBasedJIT(std::unique_ptr<llvm::orc::LLJIT> JIT)
    : m_jit(std::move(JIT)) {
  auto &jdExpected = m_jit->getMainJITDylib();
  llvm::orc::MangleAndInterner mangle(m_jit->getExecutionSession(),
                                      m_jit->getDataLayout());
  llvm::orc::SymbolMap mySymbolMap = {
      {mangle("doLB"),
       llvm::orc::ExecutorSymbolDef(llvm::orc::ExecutorAddr::fromPtr(&doLB),
                                    llvm::JITSymbolFlags::Callable)},
      {mangle("doLBU"),
       llvm::orc::ExecutorSymbolDef(llvm::orc::ExecutorAddr::fromPtr(&doLBU),
                                    llvm::JITSymbolFlags::Callable)},
      {mangle("doLH"),
       llvm::orc::ExecutorSymbolDef(llvm::orc::ExecutorAddr::fromPtr(&doLH),
                                    llvm::JITSymbolFlags::Callable)},
      {mangle("doLHU"),
       llvm::orc::ExecutorSymbolDef(llvm::orc::ExecutorAddr::fromPtr(&doLHU),
                                    llvm::JITSymbolFlags::Callable)},
      {mangle("doLW"),
       llvm::orc::ExecutorSymbolDef(llvm::orc::ExecutorAddr::fromPtr(&doLW),
                                    llvm::JITSymbolFlags::Callable)},
      {mangle("doSB"),
       llvm::orc::ExecutorSymbolDef(llvm::orc::ExecutorAddr::fromPtr(&doSB),
                                    llvm::JITSymbolFlags::Callable)},
      {mangle("doSH"),
       llvm::orc::ExecutorSymbolDef(llvm::orc::ExecutorAddr::fromPtr(&doSH),
                                    llvm::JITSymbolFlags::Callable)},
      {mangle("doSW"),
       llvm::orc::ExecutorSymbolDef(llvm::orc::ExecutorAddr::fromPtr(&doSW),
                                    llvm::JITSymbolFlags::Callable)},
      {mangle("doSyscall"), llvm::orc::ExecutorSymbolDef(
                                llvm::orc::ExecutorAddr::fromPtr(&doSyscall),
                                llvm::JITSymbolFlags::Callable)},
  };

  [[maybe_unused]] auto x =
      jdExpected.define(llvm::orc::absoluteSymbols(std::move(mySymbolMap)));
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
