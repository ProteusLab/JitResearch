#include "prot/llvm/builder.hh"

#include "prot/cpu_state.hh"
#include "prot/isa.hh"
#include "prot/memory.hh"

#include <cstdint>
#include <vector>

#include <llvm/IR/DerivedTypes.h>
#include <llvm/IR/Module.h>
#include <llvm/IR/Type.h>
#include <llvm/IR/Value.h>

namespace prot::ll {
namespace {
struct InsnIRBuilder : public llvm::IRBuilder<> {
  explicit InsnIRBuilder(llvm::Module &module)
      : llvm::IRBuilder<>(module.getContext()) {}

  void build(const isa::Instruction &insn);

  llvm::Module *getModule() const { return GetInsertBlock()->getModule(); }
  llvm::Function *getFn() const { return GetInsertBlock()->getParent(); }
  llvm::Value *getCpuStatePtr() const { return getFn()->getArg(0); }

  void setCurPC(isa::Addr pc) { m_curPC = pc; }
  isa::Addr getCurPC() const { return m_curPC; }

  void setOutPC(llvm::Value *v) { m_outPC = v; }
  llvm::Value *getOutPC() const { return m_outPC; }

  llvm::Value *getReg(std::size_t idx) {
    auto *cpuState = getCpuStatePtr();
    auto *regsArrTy = getCPUStateType()->getStructElementType(0);

    llvm::Value *regsPtr = CreateStructGEP(getCPUStateType(), cpuState, 0);
    return CreateInBoundsGEP(regsArrTy, regsPtr, {getInt32(0), getInt32(idx)});
  }

  llvm::StructType *getCPUStateType() const {
    auto &Ctx = getContext();
    if (auto *type = llvm::StructType::getTypeByName(Ctx, "CPUState")) {
      return type;
    }

    llvm::Type *wordType = llvm::Type::getInt32Ty(Ctx);
    llvm::Type *pcType = wordType;
    llvm::Type *finishedType = llvm::Type::getInt1Ty(Ctx);
    llvm::Type *memoryPtrType = llvm::PointerType::get(Ctx, 0);
    llvm::Type *icountType = llvm::Type::getInt64Ty(Ctx);
    llvm::ArrayType *regsArrayType = llvm::ArrayType::get(wordType, 32);

    return llvm::StructType::create(
        Ctx,
        {regsArrayType, pcType, finishedType, memoryPtrType, icountType,
         wordType, memoryPtrType, /*tb_cache_base=*/memoryPtrType,
         /*next_tb=*/memoryPtrType},
        "CPUState", /*IsPacked=*/false);
  }

  void generateLoad(const isa::Instruction &insn);
  void generateStore(const isa::Instruction &insn);

private:
  isa::Addr m_curPC{};
  llvm::Value *m_outPC{};
};

struct CpuStateMethInfo final {
  llvm::Type *OutTy{};
  std::vector<llvm::Type *> OtherArgs;
};
[[nodiscard]] llvm::Function *getCpuStateMeth(llvm::Module &Module,
                                              llvm::StringRef Name,
                                              const CpuStateMethInfo &info) {
  if (auto *f = Module.getFunction(Name)) {
    return f;
  }

  llvm::Type *cpuStatePtrTy = llvm::PointerType::get(Module.getContext(), 0);

  std::vector<llvm::Type *> args = {cpuStatePtrTy};
  args.insert(args.end(), info.OtherArgs.begin(), info.OtherArgs.end());

  llvm::FunctionType *ft = llvm::FunctionType::get(info.OutTy, args, false);

  return llvm::Function::Create(ft, llvm::Function::ExternalLinkage, Name,
                                Module);
}

template <auto Func>
  requires(std::is_pointer_v<decltype(Func)>)
class ExtFunctionInfo final {
public:
  using Creator = CpuStateMethInfo (*)(llvm::Module &);

private:
  std::string_view m_name;
  Creator m_create;

public:
  constexpr ExtFunctionInfo(std::string_view name, Creator create)
      : m_name(name), m_create(create) {}

  [[nodiscard]] auto operator()(llvm::Module &Module) const {
    return getCpuStateMeth(Module, m_name, m_create(Module));
  }

  [[nodiscard]] static void *addr() { return reinterpret_cast<void *>(Func); }
  [[nodiscard]] constexpr auto name() const { return m_name; }
};

void doSyscall(CPUState &state) { state.emulateSysCall(); }

constexpr auto kExtTable = std::make_tuple(ExtFunctionInfo<&doSyscall>{
    "doSyscall", [](llvm::Module &Mod) {
      auto &Ctx = Mod.getContext();
      return CpuStateMethInfo{.OutTy = llvm::Type::getVoidTy(Ctx),
                              .OtherArgs = {}};
    }});

template <typename Func> void forExtFunc(Func &&f) {
  std::apply(
      [&f](auto &&...args) { (f(std::forward<decltype(args)>(args)), ...); },
      kExtTable);
}
template <auto Func> constexpr const auto &getSpecialFunc() {
  return std::get<ExtFunctionInfo<Func>>(kExtTable);
}

void InsnIRBuilder::generateLoad(const isa::Instruction &insn) {
  if (insn.rd() == 0) {
    return;
  }
  auto *cpuStructTy = getCPUStateType();
  auto *regsArrTy = cpuStructTy->getStructElementType(0);

  llvm::Value *cpuStatePtr = getCpuStatePtr();
  llvm::Value *regsPtr = CreateStructGEP(cpuStructTy, cpuStatePtr, 0);
  llvm::Value *rdPtr =
      CreateInBoundsGEP(regsArrTy, regsPtr, {getInt32(0), getInt32(insn.rd())});

  llvm::Value *rs1Ptr = CreateInBoundsGEP(regsArrTy, regsPtr,
                                          {getInt32(0), getInt32(insn.rs1())});
  llvm::Value *rs1Val = CreateLoad(getInt32Ty(), rs1Ptr);

  llvm::Value *addrVal = CreateAdd(rs1Val, getInt32(insn.imm()));

  llvm::Value *memBase =
      CreateLoad(llvm::PointerType::get(getContext(), 0),
                 CreateStructGEP(cpuStructTy, cpuStatePtr, 6));
  auto *loadAddr =
      CreateGEP(getInt8Ty(), memBase, CreateZExt(addrVal, getInt64Ty()));

  auto [type, do_sext] = [&] {
    switch (auto opc = insn.opcode()) {
      using enum isa::Opcode;
    case kLB:
    case kLBU:
      return std::pair{getInt8Ty(), opc == kLB};
    case kLH:
    case kLHU:
      return std::pair{getInt16Ty(), opc == kLH};
    case kLW:
      return std::pair{getInt32Ty(), false};
    default:
      throw std::invalid_argument{"Bad opcode"};
    }
  }();

  llvm::Value *loaded = CreateLoad(type, loadAddr);
  if (do_sext) {
    loaded = CreateSExt(loaded, getInt32Ty());
  } else {
    loaded = CreateZExt(loaded, getInt32Ty());
  }

  CreateStore(loaded, rdPtr);
}

void InsnIRBuilder::generateStore(const isa::Instruction &insn) {
  auto *cpuStructTy = getCPUStateType();
  auto *regsArrTy = cpuStructTy->getStructElementType(0);
  llvm::Value *cpuStatePtr = getCpuStatePtr();

  llvm::Value *regsPtr = CreateStructGEP(cpuStructTy, cpuStatePtr, 0);

  llvm::Value *rs1Ptr = CreateInBoundsGEP(regsArrTy, regsPtr,
                                          {getInt32(0), getInt32(insn.rs1())});
  llvm::Value *rs1Val = CreateLoad(getInt32Ty(), rs1Ptr);

  llvm::Value *addrVal = CreateAdd(rs1Val, getInt32(insn.imm()));

  llvm::Value *rs2Ptr = CreateInBoundsGEP(regsArrTy, regsPtr,
                                          {getInt32(0), getInt32(insn.rs2())});
  auto *valTy = getIntNTy([&] {
    switch (insn.opcode()) {
      using enum isa::Opcode;
    case kSB:
      return sizeofBits<isa::Byte>();
    case kSH:
      return sizeofBits<isa::Half>();
    case kSW:
      return sizeofBits<isa::Word>();
    default:
      throw std::invalid_argument{"Bad store insn"};
    }
  }());

  llvm::Value *memBase =
      CreateLoad(llvm::PointerType::get(getContext(), 0),
                 CreateStructGEP(cpuStructTy, cpuStatePtr, 6));
  auto *storeAddr =
      CreateGEP(getInt8Ty(), memBase, CreateZExt(addrVal, getInt64Ty()));

  CreateStore(CreateLoad(valTy, rs2Ptr), storeAddr);
}

void LUIbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  isa::Imm imm = insn.imm();
  isa::Operand rd = insn.rd();
  if (rd == 0) {

    return;
  }
  auto *cpuStructTy = Data.getCPUStateType();
  auto *regsArrTy =
      llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.getContext()), 32);
  auto *cpuArg = Data.getCpuStatePtr();

  llvm::Value *regsPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 0);
  llvm::Value *rdPtr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rd)});
  Data.CreateStore(Data.getInt32(imm), rdPtr);
}

void AUIPCbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  isa::Imm imm = insn.imm();
  isa::Operand rd = insn.rd();

  auto *cpuStructTy = Data.getCPUStateType();
  auto *regsArrTy =
      llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.getContext()), 32);
  auto *cpuArg = Data.getCpuStatePtr();

  llvm::Value *regsPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 0);
  llvm::Value *rdPtr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rd)});

  llvm::Value *pcVal = Data.getInt32(Data.getCurPC());
  if (rd != 0) {
    Data.CreateStore(Data.CreateAdd(Data.getInt32(imm), pcVal), rdPtr);
  }
}

void JALbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  isa::Operand rd = insn.rd();
  isa::Imm offset = insn.imm();

  auto *cpuStructTy = Data.getCPUStateType();
  auto *regsArrTy = cpuStructTy->getStructElementType(0);

  auto *cpuArg = Data.getCpuStatePtr();

  llvm::Value *regsPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 0);
  llvm::Value *rdPtr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rd)});

  llvm::Value *pcVal = Data.getInt32(Data.getCurPC());
  Data.setOutPC(Data.CreateAdd(Data.getInt32(offset), pcVal));
  if (rd != 0) {
    Data.CreateStore(Data.CreateAdd(Data.getInt32(4), pcVal), rdPtr);
  };
}

void JALRbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  isa::Operand rd = insn.rd();
  isa::Operand rs1 = insn.rs1();
  isa::Imm offset = insn.imm();

  auto *cpuStructTy = Data.getCPUStateType();
  auto *regsArrTy = cpuStructTy->getStructElementType(0);
  auto *cpuArg = Data.getCpuStatePtr();

  llvm::Value *regsPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 0);
  llvm::Value *rs1Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs1)});
  llvm::Value *rs1Val = Data.CreateLoad(Data.getInt32Ty(), rs1Ptr);

  llvm::Value *pcVal = Data.getInt32(Data.getCurPC());

  llvm::Value *rdPtr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rd)});
  if (rd != 0)
    Data.CreateStore(Data.CreateAdd(pcVal, Data.getInt32(4)), rdPtr);

  llvm::Value *target = Data.CreateAdd(rs1Val, Data.getInt32(offset));
  llvm::Value *targetAligned = Data.CreateAnd(target, Data.getInt32(~1U));
  Data.setOutPC(targetAligned);
}

void BEQbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  isa::Operand rs11 = insn.rs1();
  isa::Operand rs12 = insn.rs2();
  isa::Imm offset = insn.imm();

  auto *cpuStructTy = Data.getCPUStateType();
  auto *regsArrTy =
      llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.getContext()), 32);
  auto *cpuArg = Data.getCpuStatePtr();

  llvm::Value *regsPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 0);
  llvm::Value *reg1Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs11)});
  llvm::Value *reg2Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs12)});

  llvm::Value *reg1Val = Data.CreateLoad(Data.getInt32Ty(), reg1Ptr);
  llvm::Value *reg2Val = Data.CreateLoad(Data.getInt32Ty(), reg2Ptr);

  llvm::Value *pcVal = Data.getInt32(Data.getCurPC());

  llvm::Value *cond = Data.CreateICmpEQ(reg1Val, reg2Val);
  llvm::Value *pcPlusoffset = Data.CreateAdd(pcVal, Data.getInt32(offset));
  llvm::Value *pcNext = Data.CreateSelect(
      cond, pcPlusoffset, Data.CreateAdd(pcVal, Data.getInt32(4)));
  Data.setOutPC(pcNext);
}

void BNEbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  isa::Operand rs11 = insn.rs1();
  isa::Operand rs12 = insn.rs2();
  isa::Imm offset = insn.imm();

  auto *cpuStructTy = Data.getCPUStateType();
  auto *regsArrTy =
      llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.getContext()), 32);
  auto *cpuArg = Data.getCpuStatePtr();

  llvm::Value *regsPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 0);
  llvm::Value *reg1Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs11)});
  llvm::Value *reg2Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs12)});

  llvm::Value *reg1Val = Data.CreateLoad(Data.getInt32Ty(), reg1Ptr);
  llvm::Value *reg2Val = Data.CreateLoad(Data.getInt32Ty(), reg2Ptr);

  llvm::Value *pcVal = Data.getInt32(Data.getCurPC());

  llvm::Value *cond = Data.CreateICmpNE(reg1Val, reg2Val);
  llvm::Value *pcPlusoffset = Data.CreateAdd(pcVal, Data.getInt32(offset));
  llvm::Value *pcNext = Data.CreateSelect(
      cond, pcPlusoffset, Data.CreateAdd(pcVal, Data.getInt32(4)));
  Data.setOutPC(pcNext);
}

void BLTbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  isa::Operand rs11 = insn.rs1();
  isa::Operand rs12 = insn.rs2();
  isa::Imm offset = insn.imm();

  auto *cpuStructTy = Data.getCPUStateType();
  auto *regsArrTy =
      llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.getContext()), 32);
  auto *cpuArg = Data.getCpuStatePtr();

  llvm::Value *regsPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 0);
  llvm::Value *reg1Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs11)});
  llvm::Value *reg2Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs12)});

  llvm::Value *reg1Val = Data.CreateLoad(Data.getInt32Ty(), reg1Ptr);
  llvm::Value *reg2Val = Data.CreateLoad(Data.getInt32Ty(), reg2Ptr);

  llvm::Value *pcVal = Data.getInt32(Data.getCurPC());

  llvm::Value *cond = Data.CreateICmpSLT(reg1Val, reg2Val);
  llvm::Value *pcPlusoffset = Data.CreateAdd(pcVal, Data.getInt32(offset));
  llvm::Value *pcNext = Data.CreateSelect(
      cond, pcPlusoffset, Data.CreateAdd(pcVal, Data.getInt32(4)));
  Data.setOutPC(pcNext);
}

void BGEbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  isa::Operand rs11 = insn.rs1();
  isa::Operand rs12 = insn.rs2();
  isa::Imm offset = insn.imm();

  auto *cpuStructTy = Data.getCPUStateType();
  auto *regsArrTy =
      llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.getContext()), 32);
  auto *cpuArg = Data.getCpuStatePtr();

  llvm::Value *regsPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 0);
  llvm::Value *reg1Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs11)});
  llvm::Value *reg2Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs12)});

  llvm::Value *reg1Val = Data.CreateLoad(Data.getInt32Ty(), reg1Ptr);
  llvm::Value *reg2Val = Data.CreateLoad(Data.getInt32Ty(), reg2Ptr);

  llvm::Value *pcVal = Data.getInt32(Data.getCurPC());

  llvm::Value *cond = Data.CreateICmpSGE(reg1Val, reg2Val);
  llvm::Value *pcPlusoffset = Data.CreateAdd(pcVal, Data.getInt32(offset));
  llvm::Value *pcNext = Data.CreateSelect(
      cond, pcPlusoffset, Data.CreateAdd(pcVal, Data.getInt32(4)));
  Data.setOutPC(pcNext);
}

void BLTUbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  isa::Operand rs11 = insn.rs1();
  isa::Operand rs12 = insn.rs2();
  isa::Imm offset = insn.imm();

  auto *cpuStructTy = Data.getCPUStateType();
  auto *regsArrTy =
      llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.getContext()), 32);
  auto *cpuArg = Data.getCpuStatePtr();

  llvm::Value *regsPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 0);
  llvm::Value *reg1Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs11)});
  llvm::Value *reg2Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs12)});

  llvm::Value *reg1Val = Data.CreateLoad(Data.getInt32Ty(), reg1Ptr);
  llvm::Value *reg2Val = Data.CreateLoad(Data.getInt32Ty(), reg2Ptr);

  llvm::Value *pcVal = Data.getInt32(Data.getCurPC());

  llvm::Value *cond = Data.CreateICmpULT(reg1Val, reg2Val);
  llvm::Value *pcPlusoffset = Data.CreateAdd(pcVal, Data.getInt32(offset));
  llvm::Value *pcNext = Data.CreateSelect(
      cond, pcPlusoffset, Data.CreateAdd(pcVal, Data.getInt32(4)));
  Data.setOutPC(pcNext);
}

void BGEUbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  isa::Operand rs11 = insn.rs1();
  isa::Operand rs12 = insn.rs2();
  isa::Imm offset = insn.imm();

  auto *cpuStructTy = Data.getCPUStateType();
  auto *regsArrTy =
      llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.getContext()), 32);
  auto *cpuArg = Data.getCpuStatePtr();

  llvm::Value *regsPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 0);
  llvm::Value *reg1Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs11)});
  llvm::Value *reg2Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs12)});

  llvm::Value *reg1Val = Data.CreateLoad(Data.getInt32Ty(), reg1Ptr);
  llvm::Value *reg2Val = Data.CreateLoad(Data.getInt32Ty(), reg2Ptr);

  llvm::Value *pcVal = Data.getInt32(Data.getCurPC());

  llvm::Value *cond = Data.CreateICmpUGE(reg1Val, reg2Val);
  llvm::Value *pcPlusoffset = Data.CreateAdd(pcVal, Data.getInt32(offset));
  llvm::Value *pcNext = Data.CreateSelect(
      cond, pcPlusoffset, Data.CreateAdd(pcVal, Data.getInt32(4)));
  Data.setOutPC(pcNext);
}

void LBbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  Data.generateLoad(insn);
}

void LHbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  Data.generateLoad(insn);
}

void LWbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  Data.generateLoad(insn);
}

void LBUbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  Data.generateLoad(insn);
}

void LHUbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  Data.generateLoad(insn);
}

void SBbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  Data.generateStore(insn);
}

void SHbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  Data.generateStore(insn);
}

void SWbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  Data.generateStore(insn);
}

void ADDIbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  isa::Operand rd = insn.rd();
  isa::Operand rs1 = insn.rs1();
  isa::Imm imm = insn.imm();

  auto *cpuStructTy = Data.getCPUStateType();
  auto *regsArrTy =
      llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.getContext()), 32);
  auto *cpuArg = Data.getCpuStatePtr();

  llvm::Value *regsPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *rs1Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs1)});
  llvm::Value *rs1Val = Data.CreateLoad(Data.getInt32Ty(), rs1Ptr);

  llvm::Value *result = Data.CreateAdd(rs1Val, Data.getInt32(imm));

  llvm::Value *rdPtr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rd)});
  if (rd != 0)
    Data.CreateStore(result, rdPtr);
}

void SLTIbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  isa::Operand rd = insn.rd();
  isa::Operand rs1 = insn.rs1();
  isa::Imm imm = insn.imm();

  auto *cpuStructTy = Data.getCPUStateType();
  auto *regsArrTy =
      llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.getContext()), 32);
  auto *cpuArg = Data.getCpuStatePtr();
  llvm::Value *regsPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *rdPtr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rd)});
  llvm::Value *rs1Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs1)});
  llvm::Value *srcValue = Data.CreateLoad(Data.getInt32Ty(), rs1Ptr);
  llvm::Value *cond = Data.CreateICmpSLT(srcValue, Data.getInt32(imm));
  if (rd != 0)
    Data.CreateStore(Data.CreateZExt(cond, Data.getInt32Ty()), rdPtr);
}

void SLTIUbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  isa::Operand rd = insn.rd();
  isa::Operand rs1 = insn.rs1();
  isa::Imm imm = insn.imm();

  auto *cpuStructTy = Data.getCPUStateType();
  auto *regsArrTy =
      llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.getContext()), 32);
  auto *cpuArg = Data.getCpuStatePtr();
  llvm::Value *regsPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *rdPtr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rd)});
  llvm::Value *rs1Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs1)});
  llvm::Value *srcValue = Data.CreateLoad(Data.getInt32Ty(), rs1Ptr);
  llvm::Value *cond = Data.CreateICmpULT(srcValue, Data.getInt32(imm));
  if (rd != 0)
    Data.CreateStore(Data.CreateZExt(cond, Data.getInt32Ty()), rdPtr);
}

void XORIbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  isa::Operand rd = insn.rd();
  isa::Operand rs1 = insn.rs1();
  isa::Imm imm = insn.imm();

  auto *cpuStructTy = Data.getCPUStateType();
  auto *regsArrTy =
      llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.getContext()), 32);
  auto *cpuArg = Data.getCpuStatePtr();

  llvm::Value *regsPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *rs1Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs1)});
  llvm::Value *rs1Val = Data.CreateLoad(Data.getInt32Ty(), rs1Ptr);

  llvm::Value *result = Data.CreateXor(rs1Val, Data.getInt32(imm));

  llvm::Value *rdPtr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rd)});
  if (rd != 0)
    Data.CreateStore(result, rdPtr);
}

void ORIbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  isa::Operand rd = insn.rd();
  isa::Operand rs1 = insn.rs1();
  isa::Imm imm = insn.imm();

  auto *cpuStructTy = Data.getCPUStateType();
  auto *regsArrTy =
      llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.getContext()), 32);
  auto *cpuArg = Data.getCpuStatePtr();

  llvm::Value *regsPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *rs1Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs1)});
  llvm::Value *rs1Val = Data.CreateLoad(Data.getInt32Ty(), rs1Ptr);

  llvm::Value *result = Data.CreateOr(rs1Val, Data.getInt32(imm));

  llvm::Value *rdPtr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rd)});
  if (rd != 0)
    Data.CreateStore(result, rdPtr);
}

void ANDIbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  isa::Operand rd = insn.rd();
  isa::Operand rs1 = insn.rs1();
  isa::Imm imm = insn.imm();

  auto *cpuStructTy = Data.getCPUStateType();
  auto *regsArrTy =
      llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.getContext()), 32);
  auto *cpuArg = Data.getCpuStatePtr();
  llvm::Value *regsPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *rs1Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs1)});
  llvm::Value *rs1Val = Data.CreateLoad(Data.getInt32Ty(), rs1Ptr);

  llvm::Value *result = Data.CreateAnd(rs1Val, Data.getInt32(imm));

  llvm::Value *rdPtr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rd)});
  if (rd != 0)
    Data.CreateStore(result, rdPtr);
}

void SLLIbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  isa::Operand rd = insn.rd();
  isa::Operand rs1 = insn.rs1();
  isa::Imm imm = insn.imm();

  auto *cpuStructTy = Data.getCPUStateType();
  auto *regsArrTy =
      llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.getContext()), 32);
  auto *cpuArg = Data.getCpuStatePtr();
  llvm::Value *regsPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *rs1Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs1)});
  llvm::Value *rs1Val = Data.CreateLoad(Data.getInt32Ty(), rs1Ptr);

  llvm::Value *result = Data.CreateShl(rs1Val, Data.getInt32(imm));

  llvm::Value *rdPtr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rd)});
  if (rd != 0)
    Data.CreateStore(result, rdPtr);
}

void SRLIbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  isa::Operand rd = insn.rd();
  isa::Operand rs1 = insn.rs1();
  isa::Imm imm = insn.imm();

  auto *cpuStructTy = Data.getCPUStateType();
  auto *regsArrTy =
      llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.getContext()), 32);
  auto *cpuArg = Data.getCpuStatePtr();
  llvm::Value *regsPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *rs1Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs1)});
  llvm::Value *rs1Val = Data.CreateLoad(Data.getInt32Ty(), rs1Ptr);

  llvm::Value *result = Data.CreateLShr(rs1Val, Data.getInt32(imm));

  llvm::Value *rdPtr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rd)});
  if (rd != 0)
    Data.CreateStore(result, rdPtr);
}

void SRAIbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  isa::Operand rd = insn.rd();
  isa::Operand rs1 = insn.rs1();
  isa::Imm imm = insn.imm();

  auto *cpuStructTy = Data.getCPUStateType();
  auto *regsArrTy =
      llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.getContext()), 32);
  auto *cpuArg = Data.getCpuStatePtr();
  llvm::Value *regsPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *rs1Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs1)});
  llvm::Value *rs1Val = Data.CreateLoad(Data.getInt32Ty(), rs1Ptr);

  llvm::Value *result = Data.CreateAShr(rs1Val, Data.getInt32(imm));

  llvm::Value *rdPtr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rd)});
  if (rd != 0)
    Data.CreateStore(result, rdPtr);
}

void ADDbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  isa::Operand rd = insn.rd();
  isa::Operand rs11 = insn.rs1();
  isa::Operand rs12 = insn.rs2();

  auto *cpuStructTy = Data.getCPUStateType();
  auto *regsArrTy =
      llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.getContext()), 32);
  auto *cpuArg = Data.getCpuStatePtr();

  llvm::Value *regsPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *reg1Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs11)});
  llvm::Value *reg2Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs12)});
  llvm::Value *reg1Val = Data.CreateLoad(Data.getInt32Ty(), reg1Ptr);
  llvm::Value *reg2Val = Data.CreateLoad(Data.getInt32Ty(), reg2Ptr);

  llvm::Value *result = Data.CreateAdd(reg1Val, reg2Val);

  llvm::Value *rdPtr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rd)});
  if (rd != 0)
    Data.CreateStore(result, rdPtr);
}

void SUBbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  isa::Operand rd = insn.rd();
  isa::Operand rs11 = insn.rs1();
  isa::Operand rs12 = insn.rs2();

  auto *cpuStructTy = Data.getCPUStateType();
  auto *regsArrTy =
      llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.getContext()), 32);
  auto *cpuArg = Data.getCpuStatePtr();

  llvm::Value *regsPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *reg1Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs11)});
  llvm::Value *reg2Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs12)});
  llvm::Value *reg1Val = Data.CreateLoad(Data.getInt32Ty(), reg1Ptr);
  llvm::Value *reg2Val = Data.CreateLoad(Data.getInt32Ty(), reg2Ptr);

  llvm::Value *result = Data.CreateSub(reg1Val, reg2Val);

  llvm::Value *rdPtr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rd)});
  if (rd != 0)
    Data.CreateStore(result, rdPtr);
}

void SLLbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  isa::Operand rd = insn.rd();
  isa::Operand rs11 = insn.rs1();
  isa::Operand rs12 = insn.rs2();

  auto *cpuStructTy = Data.getCPUStateType();
  auto *regsArrTy =
      llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.getContext()), 32);
  auto *cpuArg = Data.getCpuStatePtr();
  llvm::Value *regsPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *rs11Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs11)});
  llvm::Value *rs11Val = Data.CreateLoad(Data.getInt32Ty(), rs11Ptr);
  llvm::Value *rs12Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs12)});
  llvm::Value *rs12Val = Data.CreateLoad(Data.getInt32Ty(), rs12Ptr);

  llvm::Value *result = Data.CreateShl(rs11Val, rs12Val);

  llvm::Value *rdPtr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rd)});
  if (rd != 0)
    Data.CreateStore(result, rdPtr);
}

void SRLbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  isa::Operand rd = insn.rd();
  isa::Operand rs11 = insn.rs1();
  isa::Operand rs12 = insn.rs2();

  auto *cpuStructTy = Data.getCPUStateType();
  auto *regsArrTy =
      llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.getContext()), 32);
  auto *cpuArg = Data.getCpuStatePtr();
  llvm::Value *regsPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *rs11Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs11)});
  llvm::Value *rs11Val = Data.CreateLoad(Data.getInt32Ty(), rs11Ptr);
  llvm::Value *rs12Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs12)});
  llvm::Value *rs12Val = Data.CreateLoad(Data.getInt32Ty(), rs12Ptr);

  llvm::Value *result = Data.CreateLShr(rs11Val, rs12Val);

  llvm::Value *rdPtr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rd)});
  if (rd != 0)
    Data.CreateStore(result, rdPtr);
}

void SRAbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  isa::Operand rd = insn.rd();
  isa::Operand rs11 = insn.rs1();
  isa::Operand rs12 = insn.rs2();

  auto *cpuStructTy = Data.getCPUStateType();
  auto *regsArrTy =
      llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.getContext()), 32);
  auto *cpuArg = Data.getCpuStatePtr();
  llvm::Value *regsPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *rs11Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs11)});
  llvm::Value *rs11Val = Data.CreateLoad(Data.getInt32Ty(), rs11Ptr);
  llvm::Value *rs12Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs12)});
  llvm::Value *rs12Val = Data.CreateLoad(Data.getInt32Ty(), rs12Ptr);

  llvm::Value *result = Data.CreateAShr(rs11Val, rs12Val);

  llvm::Value *rdPtr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rd)});
  if (rd != 0)
    Data.CreateStore(result, rdPtr);
}

void XORbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  isa::Operand rd = insn.rd();
  isa::Operand rs11 = insn.rs1();
  isa::Operand rs12 = insn.rs2();

  auto *cpuStructTy = Data.getCPUStateType();
  auto *regsArrTy =
      llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.getContext()), 32);
  auto *cpuArg = Data.getCpuStatePtr();

  llvm::Value *regsPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *reg1Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs11)});
  llvm::Value *reg2Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs12)});
  llvm::Value *reg1Val = Data.CreateLoad(Data.getInt32Ty(), reg1Ptr);
  llvm::Value *reg2Val = Data.CreateLoad(Data.getInt32Ty(), reg2Ptr);

  llvm::Value *result = Data.CreateXor(reg1Val, reg2Val);

  llvm::Value *rdPtr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rd)});
  if (rd != 0)
    Data.CreateStore(result, rdPtr);
}

void ORbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  isa::Operand rd = insn.rd();
  isa::Operand rs11 = insn.rs1();
  isa::Operand rs12 = insn.rs2();

  auto *cpuStructTy = Data.getCPUStateType();
  auto *regsArrTy =
      llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.getContext()), 32);
  auto *cpuArg = Data.getCpuStatePtr();

  llvm::Value *regsPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *reg1Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs11)});
  llvm::Value *reg2Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs12)});
  llvm::Value *reg1Val = Data.CreateLoad(Data.getInt32Ty(), reg1Ptr);
  llvm::Value *reg2Val = Data.CreateLoad(Data.getInt32Ty(), reg2Ptr);

  llvm::Value *result = Data.CreateOr(reg1Val, reg2Val);

  llvm::Value *rdPtr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rd)});
  if (rd != 0)
    Data.CreateStore(result, rdPtr);
}

void ANDbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  isa::Operand rd = insn.rd();
  isa::Operand rs11 = insn.rs1();
  isa::Operand rs12 = insn.rs2();

  auto *cpuStructTy = Data.getCPUStateType();
  auto *regsArrTy =
      llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.getContext()), 32);
  auto *cpuArg = Data.getCpuStatePtr();

  llvm::Value *regsPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *reg1Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs11)});
  llvm::Value *reg2Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs12)});
  llvm::Value *reg1Val = Data.CreateLoad(Data.getInt32Ty(), reg1Ptr);
  llvm::Value *reg2Val = Data.CreateLoad(Data.getInt32Ty(), reg2Ptr);

  llvm::Value *result = Data.CreateAnd(reg1Val, reg2Val);

  llvm::Value *rdPtr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rd)});
  if (rd != 0)
    Data.CreateStore(result, rdPtr);
}

void SLTbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  isa::Operand rd = insn.rd();
  isa::Operand rs11 = insn.rs1();
  isa::Operand rs12 = insn.rs2();

  auto *cpuStructTy = Data.getCPUStateType();
  auto *regsArrTy =
      llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.getContext()), 32);
  auto *cpuArg = Data.getCpuStatePtr();
  llvm::Value *regsPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *rdPtr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rd)});
  llvm::Value *rs11Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs11)});
  llvm::Value *src1Value = Data.CreateLoad(Data.getInt32Ty(), rs11Ptr);
  llvm::Value *rs12Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs12)});
  llvm::Value *src2Value = Data.CreateLoad(Data.getInt32Ty(), rs12Ptr);
  llvm::Value *cond = Data.CreateICmpSLT(src1Value, src2Value);
  if (rd != 0)
    Data.CreateStore(Data.CreateZExt(cond, Data.getInt32Ty()), rdPtr);
}

void SLTUbuildIR(InsnIRBuilder &Data, const isa::Instruction &insn) {
  isa::Operand rd = insn.rd();
  isa::Operand rs11 = insn.rs1();
  isa::Operand rs12 = insn.rs2();

  auto *cpuStructTy = Data.getCPUStateType();
  auto *regsArrTy =
      llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.getContext()), 32);
  auto *cpuArg = Data.getCpuStatePtr();
  llvm::Value *regsPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *rdPtr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rd)});
  llvm::Value *rs11Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs11)});
  llvm::Value *src1Value = Data.CreateLoad(Data.getInt32Ty(), rs11Ptr);
  llvm::Value *rs12Ptr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rs12)});
  llvm::Value *src2Value = Data.CreateLoad(Data.getInt32Ty(), rs12Ptr);
  llvm::Value *cond = Data.CreateICmpULT(src1Value, src2Value);
  if (rd != 0)
    Data.CreateStore(Data.CreateZExt(cond, Data.getInt32Ty()), rdPtr);
}

void FENCEbuildIR(InsnIRBuilder & /*unused*/,
                  const isa::Instruction & /*unused*/) {}

void PAUSEbuildIR(InsnIRBuilder & /*unused*/,
                  const isa::Instruction & /*unused*/) {}

void ECALLbuildIR(InsnIRBuilder &Data, const isa::Instruction & /*unused*/) {
  llvm::Value *cpuStatePtr = Data.getCpuStatePtr();
  Data.CreateCall(getSpecialFunc<&doSyscall>()(*Data.getModule()),
                  {cpuStatePtr});
}

void EBREAKbuildIR(InsnIRBuilder & /*unused*/,
                   const isa::Instruction & /*unused*/) {}

// Inline TB-cache probe + guaranteed tail call to the successor block.
// Layout constants MUST stay in sync with prot::engine::TbCacheEntry and the
// kTbCache* constants in prot/jit/base.hh (mirrored here to avoid pulling the
// engine header into the low-level builder, same approach as getCPUStateType).
constexpr std::uint32_t kTbGranularityLog2 = 2;
constexpr std::uint32_t kTbMask = (1U << 22) - 1U;
constexpr std::uint64_t kTbEntrySize = 16; // sizeof(TbCacheEntry)
constexpr std::uint64_t kTbGpaOffset = 8;  // offsetof(TbCacheEntry, gpa)

void emitChainTail(InsnIRBuilder &data, bool hasEcall, llvm::Value *outPC) {
  auto &ctx = data.getContext();
  auto *fn = data.getFn();
  auto *cpuStructTy = data.getCPUStateType();
  auto *cpuArg = data.getCpuStatePtr();
  auto *ptrTy = llvm::PointerType::get(ctx, 0);
  auto *i32 = data.getInt32Ty();

  llvm::BasicBlock *retBB = llvm::BasicBlock::Create(ctx, "chain.ret", fn);
  llvm::IRBuilder<>{retBB}.CreateStore(outPC,
      data.CreateStructGEP(cpuStructTy, cpuArg, 1));
  llvm::IRBuilder<>{retBB}.CreateRetVoid();

  if (hasEcall) {
    llvm::Value *finPtr = data.CreateStructGEP(cpuStructTy, cpuArg, 2);
    llvm::Value *finVal = data.CreateLoad(data.getInt1Ty(), finPtr);
    llvm::BasicBlock *contBB = llvm::BasicBlock::Create(ctx, "chain.cont", fn);
    data.CreateCondBr(finVal, retBB, contBB);
    data.SetInsertPoint(contBB);
  }

  llvm::Value *basePtr = data.CreateStructGEP(cpuStructTy, cpuArg, 7);
  llvm::Value *base = data.CreateLoad(ptrTy, basePtr);

  llvm::Value *hash =
      data.CreateAnd(data.CreateLShr(outPC, data.getInt32(kTbGranularityLog2)),
                     data.getInt32(kTbMask));
  llvm::Value *off = data.CreateMul(data.CreateZExt(hash, data.getInt64Ty()),
                                    data.getInt64(kTbEntrySize));
  llvm::Value *entry = data.CreateGEP(data.getInt8Ty(), base, off);
  llvm::Value *gpaPtr =
      data.CreateGEP(data.getInt8Ty(), entry, data.getInt64(kTbGpaOffset));
  llvm::Value *gpa = data.CreateLoad(i32, gpaPtr);
  llvm::Value *func = data.CreateLoad(ptrTy, entry);

  llvm::Value *hit = data.CreateICmpEQ(gpa, outPC);
  llvm::BasicBlock *hitBB = llvm::BasicBlock::Create(ctx, "chain.hit", fn);
  data.CreateCondBr(hit, hitBB, retBB);

  data.SetInsertPoint(hitBB);
  llvm::CallInst *call = data.CreateCall(fn->getFunctionType(), func, {cpuArg});
  call->setCallingConv(fn->getCallingConv());
  call->setTailCallKind(llvm::CallInst::TCK_MustTail);
  data.CreateRetVoid();
}

} // namespace
std::pair<std::unique_ptr<llvm::LLVMContext>, std::unique_ptr<llvm::Module>>
translate(const std::string &name, const std::vector<isa::Instruction> &insns,
          isa::Addr startPC, ChainMode chainMode) {
  auto ctxPtr = std::make_unique<llvm::LLVMContext>();
  auto modulePtr = std::make_unique<llvm::Module>(name, *ctxPtr);

  auto *fnTy =
      llvm::FunctionType::get(llvm::Type::getVoidTy(*ctxPtr),
                              {llvm::PointerType::getUnqual(*ctxPtr)}, false);
  auto *fn = llvm::Function::Create(fnTy, llvm::Function::ExternalLinkage, name,
                                    *modulePtr);

  InsnIRBuilder data{*modulePtr};

  llvm::BasicBlock *entryBB = llvm::BasicBlock::Create(*ctxPtr, "entry", fn);
  data.SetInsertPoint(entryBB);

  isa::Addr curPC = startPC;
  bool hasEcall = false;
  for (const auto &insn : insns) {
    data.setCurPC(curPC);
    data.build(insn);
    if (insn.opcode() == isa::Opcode::kECALL) {
      hasEcall = true;
    }
    curPC += isa::kWordSize;
  }

  auto *cpuStructTy = data.getCPUStateType();
  auto *cpuArg = data.getCpuStatePtr();

  if (insns.empty() || !isa::changesPC(insns.back().opcode())) {
    data.setOutPC(data.getInt32(static_cast<std::uint32_t>(curPC)));
  }

  auto *icountType =
      llvm::IntegerType::get(*ctxPtr, sizeofBits<std::uint64_t>());

  llvm::Value *icPtr = data.CreateStructGEP(cpuStructTy, cpuArg, 4);
  auto *icVal = data.CreateLoad(icountType, icPtr);
  auto *newVal = data.CreateAdd(icVal, data.getInt64(insns.size()));
  data.CreateStore(newVal, icPtr);

  const bool lastIsJalr =
      !insns.empty() && insns.back().opcode() == isa::Opcode::kJALR;
  llvm::Value *outPC = data.getOutPC();
  if (chainMode == ChainMode::MustTail && !lastIsJalr) {
    emitChainTail(data, hasEcall, outPC);
  } else {
    llvm::Value *pcPtr = data.CreateStructGEP(cpuStructTy, cpuArg, 1);
    data.CreateStore(outPC, pcPtr);
    data.CreateRetVoid();
  }

  return {std::move(ctxPtr), std::move(modulePtr)};
}

const std::unordered_map<std::string_view, void *> &getFuncMapper() {
  static const auto map = [] {
    std::unordered_map<std::string_view, void *> res;
    forExtFunc(
        [&](const auto &extInfo) { res[extInfo.name()] = extInfo.addr(); });
    return res;
  }();

  return map;
}

void InsnIRBuilder::build(const isa::Instruction &insn) {
  using enum prot::isa::Opcode;
  switch (insn.opcode()) {
#define PROT_JIT_CASE(Insn)                                                    \
  case k##Insn:                                                                \
    return Insn##buildIR(*this, insn);

    PROT_JIT_CASE(LUI)
    PROT_JIT_CASE(AUIPC)
    PROT_JIT_CASE(JAL)
    PROT_JIT_CASE(JALR)
    PROT_JIT_CASE(BEQ)
    PROT_JIT_CASE(BNE)
    PROT_JIT_CASE(BLT)
    PROT_JIT_CASE(BGE)
    PROT_JIT_CASE(BLTU)
    PROT_JIT_CASE(BGEU)
    PROT_JIT_CASE(LB)
    PROT_JIT_CASE(LH)
    PROT_JIT_CASE(LW)
    PROT_JIT_CASE(LBU)
    PROT_JIT_CASE(LHU)
    PROT_JIT_CASE(SB)
    PROT_JIT_CASE(SH)
    PROT_JIT_CASE(SW)
    PROT_JIT_CASE(ADDI)
    PROT_JIT_CASE(SLTI)
    PROT_JIT_CASE(SLTIU)
    PROT_JIT_CASE(XORI)
    PROT_JIT_CASE(ORI)
    PROT_JIT_CASE(ANDI)
    PROT_JIT_CASE(SLLI)
    PROT_JIT_CASE(SRLI)
    PROT_JIT_CASE(SRAI)
    PROT_JIT_CASE(ADD)
    PROT_JIT_CASE(SUB)
    PROT_JIT_CASE(SLL)
    PROT_JIT_CASE(SLT)
    PROT_JIT_CASE(SLTU)
    PROT_JIT_CASE(XOR)
    PROT_JIT_CASE(SRL)
    PROT_JIT_CASE(SRA)
    PROT_JIT_CASE(OR)
    PROT_JIT_CASE(AND)
    PROT_JIT_CASE(FENCE)
    PROT_JIT_CASE(PAUSE)
    PROT_JIT_CASE(ECALL)
    PROT_JIT_CASE(EBREAK)
#undef PROT_JIT_CASE
  default:
    assert(false);
  }
}

} // namespace prot::ll
