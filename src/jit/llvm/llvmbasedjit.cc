#include "llvm-c/Target.h"
#include "llvm/ExecutionEngine/JITSymbol.h"
#include "llvm/ExecutionEngine/Orc/Core.h"
#include "llvm/ExecutionEngine/Orc/LLJIT.h"
#include "llvm/ExecutionEngine/Orc/Mangling.h"
#include "llvm/ExecutionEngine/Orc/Shared/ExecutorAddress.h"
#include "llvm/ExecutionEngine/Orc/Shared/ExecutorSymbolDef.h"
#include <iostream>
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
#include "llvm/Support/TargetSelect.h"
#include <llvm/ExecutionEngine/Orc/ThreadSafeModule.h>
#include "llvm/IR/LegacyPassManager.h"
#include <llvm/IR/LLVMContext.h>
#include "llvm/Transforms/InstCombine/InstCombine.h"
#include "llvm/Transforms/Scalar.h"
#include "llvm/Support/InitLLVM.h"
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
  auto loaded = state.memory->read<T>(addr);
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
    module->dump();
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

  IRData data{.Module=*modulePtr, .Builder=builder, .CurrentFunction=fn, .MemoryFunctions={
    getOrDeclareMemoryFunction(*modulePtr, builder, "doLB"),
    getOrDeclareMemoryFunction(*modulePtr, builder, "doLH"),
    getOrDeclareMemoryFunction(*modulePtr, builder, "doLW"),
    getOrDeclareMemoryFunction(*modulePtr, builder, "doLBU"),
    getOrDeclareMemoryFunction(*modulePtr, builder, "doLHU"),
    getOrDeclareMemoryFunction(*modulePtr, builder, "doSB"),
    getOrDeclareMemoryFunction(*modulePtr, builder, "doSH"),
    getOrDeclareMemoryFunction(*modulePtr, builder, "doSW"),
  }};
  llvm::BasicBlock *entryBB = llvm::BasicBlock::Create(*ctxPtr, "entry", fn);
  builder.SetInsertPoint(entryBB);

  std::ranges::for_each(info.insns, std::bind_front(buildInstruction, data));
  data.Builder.CreateRetVoid();

  return std::make_pair(std::move(modulePtr), std::move(ctxPtr));
}

LLVMBasedJIT::LLVMBasedJIT(std::unique_ptr<llvm::orc::LLJIT> JIT)
  : m_jit(std::move(JIT)) {
    auto jdExpected = m_jit->createJITDylib("");
    assert(jdExpected);
    llvm::orc::MangleAndInterner mangle(m_jit->getExecutionSession(), m_jit->getDataLayout());
    auto ssp = m_jit->getExecutionSession().getSymbolStringPool();
    llvm::orc::SymbolMap mySymbolMap = {
      {
        ssp->intern("doLB"),
        llvm::orc::ExecutorSymbolDef(
          llvm::orc::ExecutorAddr::fromPtr(&loadHelper<isa::Byte>),
          llvm::JITSymbolFlags::Absolute
        )
      },
      {
        ssp->intern("doLBU"),
        llvm::orc::ExecutorSymbolDef(
          llvm::orc::ExecutorAddr::fromPtr(&loadHelper<isa::Byte, false>),
          llvm::JITSymbolFlags::Absolute
        )
      },
      {
        ssp->intern("doLH"),
        llvm::orc::ExecutorSymbolDef(
          llvm::orc::ExecutorAddr::fromPtr(&loadHelper<isa::Half>),
          llvm::JITSymbolFlags::Absolute
        )
      },
      {
        ssp->intern("doLHU"),
        llvm::orc::ExecutorSymbolDef(
          llvm::orc::ExecutorAddr::fromPtr(&loadHelper<isa::Half, false>),
          llvm::JITSymbolFlags::Absolute
        )
      },
      {
        ssp->intern("doLW"),
        llvm::orc::ExecutorSymbolDef(
          llvm::orc::ExecutorAddr::fromPtr(&loadHelper<isa::Word>),
          llvm::JITSymbolFlags::Absolute
        )
      },
      {
        ssp->intern("doSB"),
        llvm::orc::ExecutorSymbolDef(
          llvm::orc::ExecutorAddr::fromPtr(&storeHelper<isa::Byte>),
          llvm::JITSymbolFlags::Absolute
        )
      },
      {
        ssp->intern("doSH"),
        llvm::orc::ExecutorSymbolDef(
          llvm::orc::ExecutorAddr::fromPtr(&storeHelper<isa::Half>),
          llvm::JITSymbolFlags::Absolute
        )
      },
      {
        ssp->intern("doSW"),
        llvm::orc::ExecutorSymbolDef(
          llvm::orc::ExecutorAddr::fromPtr(&storeHelper<isa::Word>),
          llvm::JITSymbolFlags::Absolute
        )
      },
    };

    [[maybe_unused]] auto x = jdExpected.get().define(
      llvm::orc::absoluteSymbols(std::move(mySymbolMap))
    );
  }

} // end anonymouse namespace

llvm::Expected<std::unique_ptr<ExecEngine>> makeLLVMBasedJIT(int argc, const char** argv) {
  llvm::InitLLVM X{argc, argv};
  LLVMInitializeNativeTarget();
  LLVMInitializeNativeAsmPrinter();
  LLVMInitializeNativeAsmParser();
  auto jitOrErr = llvm::orc::LLJITBuilder().create();
  if (!jitOrErr) return jitOrErr.takeError();
  return std::make_unique<LLVMBasedJIT>(std::move(*jitOrErr));
}

} // end namespace prot::engine
