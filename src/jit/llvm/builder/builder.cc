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

    return llvm::StructType::create(Ctx,
                                    {regsArrayType, pcType, finishedType,
                                     memoryPtrType, icountType, wordType},
                                    "CPUState", /*IsPacked=*/false);
  }

  void generateLoad(const isa::Instruction &insn);
  void generateStore(const isa::Instruction &insn);

  template <typename T> llvm::Function *getLoadFn();
  template <typename T> llvm::Function *getStoreFn();

  void advancePC();
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

template <typename T> T doLoad(CPUState &cpu, isa::Imm addr) {
  return cpu.memory->read<T>(addr);
}

void doStore(CPUState &cpu, isa::Imm addr, auto val) {
  cpu.memory->write(addr, val);
}

void doSyscall(CPUState &state) { state.emulateSysCall(); }

#define PROT_GEN_LOAD(Tpy, Size)                                               \
  ExtFunctionInfo<&doLoad<isa::Tpy>> {                                         \
    "doLoad" #Tpy, [](llvm::Module &Mod) {                                     \
      auto &Ctx = Mod.getContext();                                            \
      return CpuStateMethInfo{.OutTy = llvm::Type::getInt##Size##Ty(Ctx),      \
                              .OtherArgs = {llvm::Type::getInt32Ty(Ctx)}};     \
    }                                                                          \
  }
#define PROT_GEN_STORE(Tpy, Size)                                              \
  ExtFunctionInfo<&doStore<isa::Tpy>> {                                        \
    "doStore" #Tpy, [](llvm::Module &Mod) {                                    \
      auto &Ctx = Mod.getContext();                                            \
      return CpuStateMethInfo{                                                 \
          .OutTy = llvm::Type::getVoidTy(Ctx),                                 \
          .OtherArgs = {llvm::Type::getInt32Ty(Ctx),                           \
                        llvm::Type::getInt##Size##Ty(Ctx)}};                   \
    }                                                                          \
  }

constexpr auto kExtTable = std::make_tuple(
    PROT_GEN_LOAD(Byte, 8), PROT_GEN_LOAD(Half, 16), PROT_GEN_LOAD(Word, 32),
    PROT_GEN_STORE(Byte, 8), PROT_GEN_STORE(Half, 16), PROT_GEN_STORE(Word, 32),
    ExtFunctionInfo<&doSyscall>{"doSyscall", [](llvm::Module &Mod) {
                                  auto &Ctx = Mod.getContext();
                                  return CpuStateMethInfo{
                                      .OutTy = llvm::Type::getVoidTy(Ctx),
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
template <typename T> llvm::Function *InsnIRBuilder::getLoadFn() {
  return getSpecialFunc<&doLoad<T>>()(*getModule());
}

template <typename T> llvm::Function *InsnIRBuilder::getStoreFn() {
  return getSpecialFunc<&doStore<T>>()(*getModule());
}

void InsnIRBuilder::generateLoad(const isa::Instruction &insn) {
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

  auto [func, do_sext] = [&] {
    switch (insn.opcode()) {
      using enum isa::Opcode;
    case kLB:
      return std::pair{getLoadFn<isa::Byte>(), true};
    case kLBU:
      return std::pair{getLoadFn<isa::Byte>(), false};
    case kLH:
      return std::pair{getLoadFn<isa::Half>(), true};
    case kLHU:
      return std::pair{getLoadFn<isa::Half>(), false};
    case kLW:
      return std::pair{getLoadFn<isa::Word>(), false};
    default:
      throw std::invalid_argument{"Bad opcode"};
    }
  }();

  llvm::Value *loaded = CreateCall(func, {cpuStatePtr, addrVal});

  if (insn.rd() != 0) {
    CreateStore(do_sext
                    ? CreateSExt(loaded, llvm::Type::getInt32Ty(getContext()))
                    : CreateZExt(loaded, llvm::Type::getInt32Ty(getContext())),
                rdPtr);
  }
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
  auto [valTy, func] = [&] {
    switch (insn.opcode()) {
      using enum isa::Opcode;
    case kSB:
      return std::pair(getInt8Ty(), getStoreFn<isa::Byte>());
    case kSH:
      return std::pair(getInt16Ty(), getStoreFn<isa::Half>());
    case kSW:
      return std::pair(getInt32Ty(), getStoreFn<isa::Word>());
    default:
      throw std::invalid_argument{"Bad store insn"};
    }
  }();
  auto *rs2Val = CreateLoad(valTy, rs2Ptr);

  CreateCall(func, {cpuStatePtr, addrVal, rs2Val});
}

void InsnIRBuilder::advancePC() {
  auto *cpuStructTy = getCPUStateType();
  auto *cpuArg = getCpuStatePtr();
  llvm::Value *pcPtr = CreateStructGEP(cpuStructTy, cpuArg, 1);
  llvm::Value *pcVal = CreateLoad(getInt32Ty(), pcPtr);
  llvm::Value *newPCVal = CreateAdd(pcVal, getInt32(4));
  CreateStore(newPCVal, pcPtr);
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

  llvm::Value *pcPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 1);
  llvm::Value *pcVal = Data.CreateLoad(Data.getInt32Ty(), pcPtr);
  if (rd != 0) {
    Data.CreateStore(Data.CreateAdd(Data.getInt32(imm), pcVal), rdPtr);
  }

  llvm::Value *newPCVal = Data.CreateAdd(pcVal, Data.getInt32(4));
  Data.CreateStore(newPCVal, pcPtr);
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

  llvm::Value *pcPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 1);
  llvm::Value *pcVal = Data.CreateLoad(Data.getInt32Ty(), pcPtr);
  Data.CreateStore(Data.CreateAdd(Data.getInt32(offset), pcVal), pcPtr);
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

  llvm::Value *pcPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 1);
  llvm::Value *pcVal = Data.CreateLoad(Data.getInt32Ty(), pcPtr);

  llvm::Value *rdPtr = Data.CreateInBoundsGEP(
      regsArrTy, regsPtr, {Data.getInt32(0), Data.getInt32(rd)});
  if (rd != 0)
    Data.CreateStore(Data.CreateAdd(pcVal, Data.getInt32(4)), rdPtr);

  llvm::Value *target = Data.CreateAdd(rs1Val, Data.getInt32(offset));
  llvm::Value *targetAligned = Data.CreateAnd(target, Data.getInt32(~1U));
  Data.CreateStore(targetAligned, pcPtr);
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

  llvm::Value *pcPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 1);
  llvm::Value *pcVal = Data.CreateLoad(Data.getInt32Ty(), pcPtr);

  llvm::Value *cond = Data.CreateICmpEQ(reg1Val, reg2Val);
  llvm::Value *pcPlusoffset = Data.CreateAdd(pcVal, Data.getInt32(offset));
  llvm::Value *pcNext = Data.CreateSelect(
      cond, pcPlusoffset, Data.CreateAdd(pcVal, Data.getInt32(4)));
  Data.CreateStore(pcNext, pcPtr);
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

  llvm::Value *pcPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 1);
  llvm::Value *pcVal = Data.CreateLoad(Data.getInt32Ty(), pcPtr);

  llvm::Value *cond = Data.CreateICmpNE(reg1Val, reg2Val);
  llvm::Value *pcPlusoffset = Data.CreateAdd(pcVal, Data.getInt32(offset));
  llvm::Value *pcNext = Data.CreateSelect(
      cond, pcPlusoffset, Data.CreateAdd(pcVal, Data.getInt32(4)));
  Data.CreateStore(pcNext, pcPtr);
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

  llvm::Value *pcPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 1);
  llvm::Value *pcVal = Data.CreateLoad(Data.getInt32Ty(), pcPtr);

  llvm::Value *cond = Data.CreateICmpSLT(reg1Val, reg2Val);
  llvm::Value *pcPlusoffset = Data.CreateAdd(pcVal, Data.getInt32(offset));
  llvm::Value *pcNext = Data.CreateSelect(
      cond, pcPlusoffset, Data.CreateAdd(pcVal, Data.getInt32(4)));
  Data.CreateStore(pcNext, pcPtr);
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

  llvm::Value *pcPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 1);
  llvm::Value *pcVal = Data.CreateLoad(Data.getInt32Ty(), pcPtr);

  llvm::Value *cond = Data.CreateICmpSGE(reg1Val, reg2Val);
  llvm::Value *pcPlusoffset = Data.CreateAdd(pcVal, Data.getInt32(offset));
  llvm::Value *pcNext = Data.CreateSelect(
      cond, pcPlusoffset, Data.CreateAdd(pcVal, Data.getInt32(4)));
  Data.CreateStore(pcNext, pcPtr);
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

  llvm::Value *pcPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 1);
  llvm::Value *pcVal = Data.CreateLoad(Data.getInt32Ty(), pcPtr);

  llvm::Value *cond = Data.CreateICmpULT(reg1Val, reg2Val);
  llvm::Value *pcPlusoffset = Data.CreateAdd(pcVal, Data.getInt32(offset));
  llvm::Value *pcNext = Data.CreateSelect(
      cond, pcPlusoffset, Data.CreateAdd(pcVal, Data.getInt32(4)));
  Data.CreateStore(pcNext, pcPtr);
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

  llvm::Value *pcPtr = Data.CreateStructGEP(cpuStructTy, cpuArg, 1);
  llvm::Value *pcVal = Data.CreateLoad(Data.getInt32Ty(), pcPtr);

  llvm::Value *cond = Data.CreateICmpUGE(reg1Val, reg2Val);
  llvm::Value *pcPlusoffset = Data.CreateAdd(pcVal, Data.getInt32(offset));
  llvm::Value *pcNext = Data.CreateSelect(
      cond, pcPlusoffset, Data.CreateAdd(pcVal, Data.getInt32(4)));
  Data.CreateStore(pcNext, pcPtr);
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

} // namespace
std::pair<std::unique_ptr<llvm::LLVMContext>, std::unique_ptr<llvm::Module>>
translate(const std::string &name, const std::vector<isa::Instruction> &insns) {
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

  for (const auto &insn : insns) {
    data.build(insn);
    if (!isa::changesPC(insn.opcode())) {
      data.advancePC();
    }
  }

  auto *icountType =
      llvm::IntegerType::get(*ctxPtr, sizeofBits<std::uint64_t>());
  auto *cpuStructTy = data.getCPUStateType();

  auto *cpuArg = data.getCpuStatePtr();

  llvm::Value *icPtr = data.CreateStructGEP(cpuStructTy, cpuArg, 4);
  auto *icVal = data.CreateLoad(icountType, icPtr);
  auto *newVal = data.CreateAdd(icVal, data.getInt64(insns.size()));
  data.CreateStore(newVal, icPtr);

  data.CreateRetVoid();

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
