#include "llvm-c/Target.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/Mangling.h"
#include "llvm/ExecutionEngine/Orc/Shared/ExecutorAddress.h"
#include "llvm/ExecutionEngine/Orc/Shared/ExecutorSymbolDef.h"
#include <iostream>
#include <llvm-19/llvm/Support/raw_ostream.h>
#include <llvm/ExecutionEngine/ExecutionEngine.h>
#include <llvm/ExecutionEngine/GenericValue.h>
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/Module.h"
#include "llvm/IR/Type.h"
#include "llvm/Support/Error.h"
#include <algorithm>
#include <cassert>
#include <cstdio>
#include <functional>
#include <llvm/IR/Mangler.h>
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
llvm::Type* getCPUStateType(llvm::LLVMContext& Ctx) {
  if (auto* type = llvm::StructType::getTypeByName(Ctx, "CPUState")) {
    return type;
  }
  llvm::Type* wordType = llvm::Type::getInt32Ty(Ctx);
  llvm::Type* pcType = wordType;
  llvm::Type* finishedType = llvm::Type::getInt1Ty(Ctx);
  llvm::Type* memoryPtrType = llvm::PointerType::get(Ctx, 0);
  llvm::Type* icountType = llvm::Type::getInt64Ty(Ctx);
  llvm::ArrayType* regsArrayType = llvm::ArrayType::get(wordType, 32);

  std::vector<llvm::Type*> structMemberTypes = {
    regsArrayType,
    pcType,
    finishedType,
    memoryPtrType,
    icountType
  };

  llvm::StructType* cpuStateType =
    llvm::StructType::create(Ctx, structMemberTypes, "CPUState", /*IsPacked=*/false);

  return cpuStateType;
}


template <typename T, bool Signed = true>
void loadHelper(const isa::Instruction *inst, CPUState &state) {
  auto rs = state.getReg(inst->rs1());
  auto addr = rs + inst->imm();
  isa::Word loaded = state.memory->read<T>(addr);
  if constexpr (Signed) {
    loaded = isa::signExtend<sizeofBits<isa::Word>(), sizeofBits<T>()>(loaded);
  }

  state.setReg(inst->rd(), loaded);
}

template <typename T>
void storeHelper(const isa::Instruction *inst, CPUState &state) {
  auto addr = state.getReg(inst->rs1()) + inst->imm();
  T val = state.getReg(inst->rs2());

  state.memory->write(addr, val);
}

extern "C" {

void doLB(const isa::Instruction *inst, CPUState &state) {
  loadHelper<isa::Byte>(inst, state);
}


void doLBU(const isa::Instruction *inst, CPUState &state) {
  loadHelper<isa::Byte, false>(inst, state);
}

void doLH(const isa::Instruction *inst, CPUState &state) {
  loadHelper<isa::Half>(inst, state);
}


void doLHU(const isa::Instruction *inst, CPUState &state) {
  loadHelper<isa::Half, false>(inst, state);
}


void doLW(const isa::Instruction *inst, CPUState &state) {
  loadHelper<isa::Word>(inst, state);
}

void doSB(const isa::Instruction *inst, CPUState &state) {
  storeHelper<isa::Byte>(inst, state);
}


void doSH(const isa::Instruction *inst, CPUState &state) {
  storeHelper<isa::Half>(inst, state);
}

void doSW(const isa::Instruction *inst, CPUState &state) {
  storeHelper<isa::Word>(inst, state);
}

void doSyscall(CPUState &state) {
  state.emulateSysCall();
}

}

class LLVMBasedJIT : public JitEngine {
  std::unique_ptr<llvm::orc::LLJIT> m_jit;

  using TBFunc = void(*)(CPUState&);

public:
  LLVMBasedJIT(std::unique_ptr<llvm::orc::LLJIT> JIT);

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
    state.dump(std::cout);
    return true;
  }

  std::pair<std::unique_ptr<llvm::Module>, std::unique_ptr<llvm::LLVMContext>> translate(isa::Word pc, const BBInfo &info);
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

llvm::Function *getOrDeclareMemoryFunction(llvm::Module &M, llvm::IRBuilder<> &Builder, llvm::StringRef Name) {
  if (auto *F = M.getFunction(Name)) {
    return F;
  }

  llvm::Type *cpuStatePtrTy = llvm::PointerType::get(M.getContext(), 0);
  llvm::Type *instPtrTy = llvm::PointerType::get(M.getContext(), 0);

  llvm::FunctionType *ft = llvm::FunctionType::get(
    Builder.getVoidTy(),
    {instPtrTy, cpuStatePtrTy},
    false
  );

  auto *F = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, Name, M);
  return F;
}

std::pair<std::unique_ptr<llvm::Module>, std::unique_ptr<llvm::LLVMContext>> LLVMBasedJIT::translate(const isa::Word pc, const BBInfo &info) {
  auto ctxPtr = std::make_unique<llvm::LLVMContext>();
  auto modulePtr = std::make_unique<llvm::Module>(std::to_string(pc), *ctxPtr);
  llvm::IRBuilder<> builder{*ctxPtr};

  auto *fnTy = llvm::FunctionType::get(llvm::Type::getVoidTy(*ctxPtr), {llvm::PointerType::getUnqual(getCPUStateType(*ctxPtr))}, false);
  auto *fn   = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, std::to_string(pc), *modulePtr);

  llvm::orc::MangleAndInterner mangle(m_jit->getExecutionSession(), m_jit->getDataLayout());

  llvm::Type *cpuStatePtrTy = llvm::PointerType::get(*ctxPtr, 0);
  llvm::FunctionType *ft = llvm::FunctionType::get(
    builder.getVoidTy(),
    {cpuStatePtrTy},
    false
  );

  auto *F = llvm::Function::Create(ft, llvm::Function::ExternalLinkage, "doSyscall", *modulePtr);

  IRData data{.Module=*modulePtr, .Builder=builder, .CurrentFunction=fn, .MemoryFunctions={
    getOrDeclareMemoryFunction(*modulePtr, builder, "doLB"),
    getOrDeclareMemoryFunction(*modulePtr, builder, "doLH"),
    getOrDeclareMemoryFunction(*modulePtr, builder, "doLW"),
    getOrDeclareMemoryFunction(*modulePtr, builder, "doLBU"),
    getOrDeclareMemoryFunction(*modulePtr, builder, "doLHU"),
    getOrDeclareMemoryFunction(*modulePtr, builder, "doSB"),
    getOrDeclareMemoryFunction(*modulePtr, builder, "doSH"),
    getOrDeclareMemoryFunction(*modulePtr, builder, "doSW"),
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
    auto& jdExpected = m_jit->getMainJITDylib();
    llvm::orc::MangleAndInterner mangle(m_jit->getExecutionSession(), m_jit->getDataLayout());
    llvm::orc::SymbolMap mySymbolMap = {
      {
        mangle("doLB"),
        llvm::orc::ExecutorSymbolDef(
          llvm::orc::ExecutorAddr::fromPtr(&doLB),
          llvm::JITSymbolFlags::Callable
        )
      },
      {
        mangle("doLBU"),
        llvm::orc::ExecutorSymbolDef(
          llvm::orc::ExecutorAddr::fromPtr(&doLBU),
          llvm::JITSymbolFlags::Callable
        )
      },
      {
        mangle("doLH"),
        llvm::orc::ExecutorSymbolDef(
          llvm::orc::ExecutorAddr::fromPtr(&doLH),
          llvm::JITSymbolFlags::Callable
        )
      },
      {
        mangle("doLHU"),
        llvm::orc::ExecutorSymbolDef(
          llvm::orc::ExecutorAddr::fromPtr(&doLHU),
          llvm::JITSymbolFlags::Callable
        )
      },
      {
        mangle("doLW"),
        llvm::orc::ExecutorSymbolDef(
          llvm::orc::ExecutorAddr::fromPtr(&doLW),
          llvm::JITSymbolFlags::Callable
        )
      },
      {
        mangle("doSB"),
        llvm::orc::ExecutorSymbolDef(
          llvm::orc::ExecutorAddr::fromPtr(&doSB),
          llvm::JITSymbolFlags::Callable
        )
      },
      {
        mangle("doSH"),
        llvm::orc::ExecutorSymbolDef(
          llvm::orc::ExecutorAddr::fromPtr(&doSH),
          llvm::JITSymbolFlags::Callable
        )
      },
      {
        mangle("doSW"),
        llvm::orc::ExecutorSymbolDef(
          llvm::orc::ExecutorAddr::fromPtr(&doSW),
          llvm::JITSymbolFlags::Callable
        )
      },
      {
        mangle("doSyscall"),
        llvm::orc::ExecutorSymbolDef(
          llvm::orc::ExecutorAddr::fromPtr(&doSyscall),
          llvm::JITSymbolFlags::Callable
        )
      },
    };

    [[maybe_unused]] auto x = jdExpected.define(
      llvm::orc::absoluteSymbols(std::move(mySymbolMap))
    );
  }

} // end anonymouse namespace

llvm::Expected<std::unique_ptr<ExecEngine>> makeLLVMBasedJIT() {
  LLVMInitializeNativeTarget();
  LLVMInitializeNativeAsmPrinter();
  LLVMInitializeNativeAsmParser();
  auto jitOrErr = llvm::orc::LLJITBuilder().create();
  if (!jitOrErr) return jitOrErr.takeError();
  return std::make_unique<LLVMBasedJIT>(std::move(*jitOrErr));
}

} // end namespace prot::engine
