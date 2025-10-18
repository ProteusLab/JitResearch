#include "prot/jit/llvmbasedjitbuilder.hh"
#include "prot/isa.hh"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Type.h"
#include <cstdint>
#include <vector>

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

void updatePC(IRData& Data) {
  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *cpuArg = Data.CurrentFunction->getArg(0);
  llvm::Value *pcPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 1);
  llvm::Value *pcVal = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), pcPtr);  
  llvm::Value *newPCVal = Data.Builder.CreateAdd(pcVal, Data.Builder.getInt32(4));
  Data.Builder.CreateStore(newPCVal, pcPtr);
}

struct Instruction {
  isa::Instruction m_insn;
  Instruction(isa::Instruction Instr) : m_insn(Instr) {}

  virtual void buildIR(IRData& data) = 0;
};

struct LUIInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct AUIPCInstruction : Instruction {
  using Instruction::Instruction;
  
  void buildIR(IRData& data) override;
};

struct JALInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct JALRInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct BEQInstruction : Instruction {
  using Instruction::Instruction;
  
  void buildIR(IRData& data) override;
};

struct BNEInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct BLTInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct BGEInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct BLTUInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct BGEUInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct LBInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct LHInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct LWInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct LBUInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct LHUInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct SBInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct SHInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct SWInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct ADDIInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct SLTIInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct SLTIUInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct XORIInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct ORIInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct ANDIInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct SLLIInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct SRLIInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct SRAIInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct ADDInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct SUBInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct SLLInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct SLTInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct SLTUInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct XORInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct SRLInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct SRAInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct ORInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct ANDInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct FENCEInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct FENCETSOInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct PAUSEInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct ECALLInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};

struct EBREAKInstruction : Instruction {
  using Instruction::Instruction;

  void buildIR(IRData& data) override;
};
  
void LUIInstruction::buildIR(IRData& Data) {
  isa::Imm imm = m_insn.imm();
  isa::Operand rd = m_insn.rd();

  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg = Data.CurrentFunction->getArg(0);

  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);
  llvm::Value *rdPtr  = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                        {Data.Builder.getInt32(0), Data.Builder.getInt32(rd)});
  Data.Builder.CreateStore(Data.Builder.getInt32(imm), rdPtr);

  updatePC(Data);
}

void AUIPCInstruction::buildIR(IRData& Data) {
  isa::Imm imm = m_insn.imm();
  isa::Operand rd = m_insn.rd();

  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg = Data.CurrentFunction->getArg(0);

  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);
  llvm::Value *rdPtr  = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                        {Data.Builder.getInt32(0), Data.Builder.getInt32(rd)});

  llvm::Value *pcPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 1);
  llvm::Value *pcVal = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), pcPtr);                                      
  Data.Builder.CreateStore(Data.Builder.CreateAdd(Data.Builder.getInt32(imm), pcVal), rdPtr);
  
  llvm::Value *newPCVal = Data.Builder.CreateAdd(pcVal, Data.Builder.getInt32(4));
  Data.Builder.CreateStore(newPCVal, pcPtr);
}

void JALInstruction::buildIR(IRData& Data) {
  isa::Operand rd = m_insn.rd();
  isa::Imm offset = m_insn.imm();

  if ((offset & 0x80000) != 0) {
    offset |= 0xFFF00000;
  }

  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);

  auto *cpuArg = Data.CurrentFunction->getArg(0);

  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);
  llvm::Value *rdPtr  = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                        {Data.Builder.getInt32(0), Data.Builder.getInt32(rd)});

  llvm::Value *pcPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 1);
  llvm::Value *pcVal = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), pcPtr);                                      
  Data.Builder.CreateStore(Data.Builder.CreateAdd(Data.Builder.getInt32(4), pcVal), rdPtr);
  Data.Builder.CreateStore(Data.Builder.CreateAdd(Data.Builder.getInt32(offset), pcVal), pcPtr);
}

void JALRInstruction::buildIR(IRData& Data) {
  isa::Operand rd = m_insn.rd();
  isa::Operand rs1  = m_insn.rs1();
  isa::Imm offset  = m_insn.imm();

  if ((offset & 0x800) != 0) {
    offset |= 0xFFFFF000;
  }

  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);

  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);
  llvm::Value *rs1Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs1)});
  llvm::Value *rs1Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), rs1Ptr);

  llvm::Value *pcPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 1);
  llvm::Value *pcVal = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), pcPtr);

  llvm::Value *rdPtr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rd)});
  Data.Builder.CreateStore(Data.Builder.CreateAdd(pcVal, Data.Builder.getInt32(4)), rdPtr);

  llvm::Value *target = Data.Builder.CreateAdd(rs1Val, Data.Builder.getInt32(offset));
  llvm::Value *targetAligned = Data.Builder.CreateAnd(target, Data.Builder.getInt32(~1U));
  Data.Builder.CreateStore(targetAligned, pcPtr);
}

void BEQInstruction::buildIR(IRData& Data) {
  isa::Operand rs11 = m_insn.rs1();
  isa::Operand rs12 = m_insn.rs2();
  isa::Imm offset = m_insn.imm();

  if ((offset & 0x1000) != 0) {
    offset |= 0xFFFFE000;
  }

  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);

  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);
  llvm::Value *reg1Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs11)});
  llvm::Value *reg2Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs12)});
  
  llvm::Value *reg1Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), reg1Ptr);
  llvm::Value *reg2Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), reg2Ptr);

  llvm::Value *pcPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 1);
  llvm::Value *pcVal = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), pcPtr);

  llvm::Value *cond = Data.Builder.CreateICmpEQ(reg1Val, reg2Val);
  llvm::Value *pcPlusoffset = Data.Builder.CreateAdd(pcVal, Data.Builder.getInt32(offset));
  llvm::Value *pcNext = Data.Builder.CreateSelect(cond, pcPlusoffset,
                                                  Data.Builder.CreateAdd(pcVal, Data.Builder.getInt32(4)));
  Data.Builder.CreateStore(pcNext, pcPtr);
}

void BNEInstruction::buildIR(IRData& Data) {
  isa::Operand rs11 = m_insn.rs1();
  isa::Operand rs12 = m_insn.rs2();
  isa::Imm offset = m_insn.imm();

  if ((offset & 0x1000) != 0) {
    offset |= 0xFFFFE000;
  }

  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);

  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);
  llvm::Value *reg1Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs11)});
  llvm::Value *reg2Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs12)});
  
  llvm::Value *reg1Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), reg1Ptr);
  llvm::Value *reg2Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), reg2Ptr);

  llvm::Value *pcPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 1);
  llvm::Value *pcVal = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), pcPtr);

  llvm::Value *cond = Data.Builder.CreateICmpNE(reg1Val, reg2Val);
  llvm::Value *pcPlusoffset = Data.Builder.CreateAdd(pcVal, Data.Builder.getInt32(offset));
  llvm::Value *pcNext = Data.Builder.CreateSelect(cond, pcPlusoffset,
                                                  Data.Builder.CreateAdd(pcVal, Data.Builder.getInt32(4)));
  Data.Builder.CreateStore(pcNext, pcPtr);
}

void BLTInstruction::buildIR(IRData& Data) {
  isa::Operand rs11 = m_insn.rs1();
  isa::Operand rs12 = m_insn.rs2();
  isa::Imm offset = m_insn.imm();

  if ((offset & 0x1000) != 0) {
    offset |= 0xFFFFE000;
  }

  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);

  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);
  llvm::Value *reg1Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs11)});
  llvm::Value *reg2Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs12)});
  
  llvm::Value *reg1Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), reg1Ptr);
  llvm::Value *reg2Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), reg2Ptr);

  llvm::Value *pcPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 1);
  llvm::Value *pcVal = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), pcPtr);

  llvm::Value *cond = Data.Builder.CreateICmpSLT(reg1Val, reg2Val);
  llvm::Value *pcPlusoffset = Data.Builder.CreateAdd(pcVal, Data.Builder.getInt32(offset));
  llvm::Value *pcNext = Data.Builder.CreateSelect(cond, pcPlusoffset,
                                                  Data.Builder.CreateAdd(pcVal, Data.Builder.getInt32(4)));
  Data.Builder.CreateStore(pcNext, pcPtr);
}

void BGEInstruction::buildIR(IRData& Data) {
  isa::Operand rs11 = m_insn.rs1();
  isa::Operand rs12 = m_insn.rs2();
  isa::Imm offset = m_insn.imm();

  if ((offset & 0x1000) != 0) {
    offset |= 0xFFFFE000;
  }

  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);

  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);
  llvm::Value *reg1Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs11)});
  llvm::Value *reg2Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs12)});
  
  llvm::Value *reg1Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), reg1Ptr);
  llvm::Value *reg2Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), reg2Ptr);

  llvm::Value *pcPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 1);
  llvm::Value *pcVal = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), pcPtr);

  llvm::Value *cond = Data.Builder.CreateICmpSGE(reg1Val, reg2Val);
  llvm::Value *pcPlusoffset = Data.Builder.CreateAdd(pcVal, Data.Builder.getInt32(offset));
  llvm::Value *pcNext = Data.Builder.CreateSelect(cond, pcPlusoffset,
                                                  Data.Builder.CreateAdd(pcVal, Data.Builder.getInt32(4)));
  Data.Builder.CreateStore(pcNext, pcPtr);
}

void BLTUInstruction::buildIR(IRData& Data) {
  isa::Operand rs11 = m_insn.rs1();
  isa::Operand rs12 = m_insn.rs2();
  isa::Imm offset = m_insn.imm();

  if ((offset & 0x1000) != 0) {
    offset |= 0xFFFFE000;
  }

  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);

  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);
  llvm::Value *reg1Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs11)});
  llvm::Value *reg2Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs12)});
  
  llvm::Value *reg1Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), reg1Ptr);
  llvm::Value *reg2Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), reg2Ptr);

  llvm::Value *pcPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 1);
  llvm::Value *pcVal = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), pcPtr);

  llvm::Value *cond = Data.Builder.CreateICmpULT(reg1Val, reg2Val);
  llvm::Value *pcPlusoffset = Data.Builder.CreateAdd(pcVal, Data.Builder.getInt32(offset));
  llvm::Value *pcNext = Data.Builder.CreateSelect(cond, pcPlusoffset,
                                                  Data.Builder.CreateAdd(pcVal, Data.Builder.getInt32(4)));
  Data.Builder.CreateStore(pcNext, pcPtr);
} 

void BGEUInstruction::buildIR(IRData& Data) {
  isa::Operand rs11 = m_insn.rs1();
  isa::Operand rs12 = m_insn.rs2();
  isa::Imm offset = m_insn.imm();

  if ((offset & 0x1000) != 0) {
    offset |= 0xFFFFE000;
  }

  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);

  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);
  llvm::Value *reg1Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs11)});
  llvm::Value *reg2Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs12)});
  
  llvm::Value *reg1Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), reg1Ptr);
  llvm::Value *reg2Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), reg2Ptr);

  llvm::Value *pcPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 1);
  llvm::Value *pcVal = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), pcPtr);

  llvm::Value *cond = Data.Builder.CreateICmpUGE(reg1Val, reg2Val);
  llvm::Value *pcPlusoffset = Data.Builder.CreateAdd(pcVal, Data.Builder.getInt32(offset));
  llvm::Value *pcNext = Data.Builder.CreateSelect(cond, pcPlusoffset,
                                                  Data.Builder.CreateAdd(pcVal, Data.Builder.getInt32(4)));
  Data.Builder.CreateStore(pcNext, pcPtr);
}

void LBInstruction::buildIR(IRData& Data) {
  llvm::Value *instPtr = Data.Builder.CreatePointerCast(
      Data.Builder.getInt64(reinterpret_cast<uintptr_t>(&m_insn)),
      llvm::PointerType::get(Data.Builder.getContext(), 0)
  );
  llvm::Value *cpuStatePtr = Data.CurrentFunction->getArg(0);

  Data.Builder.CreateCall(Data.MemoryFunctions[0], {instPtr, cpuStatePtr});
  
  updatePC(Data);
}

void LHInstruction::buildIR(IRData& Data) {
  llvm::Value *instPtr = Data.Builder.CreatePointerCast(
      Data.Builder.getInt64(reinterpret_cast<uintptr_t>(&m_insn)),
      llvm::PointerType::get(Data.Builder.getContext(), 0)
  );
  llvm::Value *cpuStatePtr = Data.CurrentFunction->getArg(0);

  Data.Builder.CreateCall(Data.MemoryFunctions[1], {instPtr, cpuStatePtr});
  
  updatePC(Data);
}

void LWInstruction::buildIR(IRData& Data) {
  llvm::Value *instPtr = Data.Builder.CreatePointerCast(
      Data.Builder.getInt64(reinterpret_cast<uintptr_t>(&m_insn)),
      llvm::PointerType::get(Data.Builder.getContext(), 0)
  );
  llvm::Value *cpuStatePtr = Data.CurrentFunction->getArg(0);

  Data.Builder.CreateCall(Data.MemoryFunctions[2], {instPtr, cpuStatePtr});
  
  updatePC(Data);
}

void LBUInstruction::buildIR(IRData& Data) {
  llvm::Value *instPtr = Data.Builder.CreatePointerCast(
      Data.Builder.getInt64(reinterpret_cast<uintptr_t>(&m_insn)),
      llvm::PointerType::get(Data.Builder.getContext(), 0)
  );
  llvm::Value *cpuStatePtr = Data.CurrentFunction->getArg(0);

  Data.Builder.CreateCall(Data.MemoryFunctions[3], {instPtr, cpuStatePtr});
  
  updatePC(Data);
}

void LHUInstruction::buildIR(IRData& Data) {
  llvm::Value *instPtr = Data.Builder.CreatePointerCast(
      Data.Builder.getInt64(reinterpret_cast<uintptr_t>(&m_insn)),
      llvm::PointerType::get(Data.Builder.getContext(), 0)
  );
  llvm::Value *cpuStatePtr = Data.CurrentFunction->getArg(0);

  Data.Builder.CreateCall(Data.MemoryFunctions[4], {instPtr, cpuStatePtr});
  
  updatePC(Data);
}

void SBInstruction::buildIR(IRData& Data) {
  llvm::Value *instPtr = Data.Builder.CreatePointerCast(
      Data.Builder.getInt64(reinterpret_cast<uintptr_t>(&m_insn)),
      llvm::PointerType::get(Data.Builder.getContext(), 0)
  );
  llvm::Value *cpuStatePtr = Data.CurrentFunction->getArg(0);

  Data.Builder.CreateCall(Data.MemoryFunctions[5], {instPtr, cpuStatePtr});
  
  updatePC(Data);
}

void SHInstruction::buildIR(IRData& Data) {
  llvm::Value *instPtr = Data.Builder.CreatePointerCast(
      Data.Builder.getInt64(reinterpret_cast<uintptr_t>(&m_insn)),
      llvm::PointerType::get(Data.Builder.getContext(), 0)
  );
  llvm::Value *cpuStatePtr = Data.CurrentFunction->getArg(0);

  Data.Builder.CreateCall(Data.MemoryFunctions[6], {instPtr, cpuStatePtr});
  
  updatePC(Data);
}

void SWInstruction::buildIR(IRData& Data) {
  llvm::Value *instPtr = Data.Builder.CreatePointerCast(
      Data.Builder.getInt64(reinterpret_cast<uintptr_t>(&m_insn)),
      llvm::PointerType::get(Data.Builder.getContext(), 0)
  );
  llvm::Value *cpuStatePtr = Data.CurrentFunction->getArg(0);

  Data.Builder.CreateCall(Data.MemoryFunctions[7], {instPtr, cpuStatePtr});
  
  updatePC(Data);
}

void ADDIInstruction::buildIR(IRData& Data) {
  isa::Operand rd = m_insn.rd();
  isa::Operand rs1  = m_insn.rs1();
  isa::Imm imm   = m_insn.imm();

  if ((imm & 0x800) != 0) {
    imm |= 0xFFFFF000;
  }

  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);

  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *rs1Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs1)});
  llvm::Value *rs1Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), rs1Ptr);

  llvm::Value *result = Data.Builder.CreateAdd(rs1Val, Data.Builder.getInt32(imm));

  llvm::Value *rdPtr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rd)});
  Data.Builder.CreateStore(result, rdPtr);

  updatePC(Data);
}

void SLTIInstruction::buildIR(IRData& Data) {
  isa::Operand rd = m_insn.rd();
  isa::Operand rs1  = m_insn.rs1();
  isa::Imm imm   = m_insn.imm();

  if ((imm & 0x800) != 0) {
    imm |= 0xFFFFF000;
  }

  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);
  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *rdPtr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rd)});
  llvm::Value *rs1Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs1)});
  llvm::Value *srcValue = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), rs1Ptr);  
  llvm::Value *cond = Data.Builder.CreateICmpSLT(srcValue, Data.Builder.getInt32(imm));
  Data.Builder.CreateStore(Data.Builder.CreateZExt(cond, Data.Builder.getInt32Ty()), rdPtr);

  updatePC(Data);
}

void SLTIUInstruction::buildIR(IRData& Data) {
  isa::Operand rd = m_insn.rd();
  isa::Operand rs1  = m_insn.rs1();
  isa::Imm imm   = m_insn.imm();

  if ((imm & 0x800) != 0) {
    imm |= 0xFFFFF000;
  }

  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);
  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *rdPtr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rd)});
  llvm::Value *rs1Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs1)});
  llvm::Value *srcValue = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), rs1Ptr);  
  llvm::Value *cond = Data.Builder.CreateICmpULT(srcValue, Data.Builder.getInt32(imm));
  Data.Builder.CreateStore(Data.Builder.CreateZExt(cond, Data.Builder.getInt32Ty()), rdPtr);

  updatePC(Data);
}

void XORIInstruction::buildIR(IRData& Data) {
  isa::Operand rd = m_insn.rd();
  isa::Operand rs1  = m_insn.rs1();
  isa::Imm imm   = m_insn.imm();

  if ((imm & 0x800) != 0) {
    imm |= 0xFFFFF000;
  }

  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);

  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *rs1Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs1)});
  llvm::Value *rs1Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), rs1Ptr);

  llvm::Value *result = Data.Builder.CreateXor(rs1Val, Data.Builder.getInt32(imm));

  llvm::Value *rdPtr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rd)});
  Data.Builder.CreateStore(result, rdPtr);

  updatePC(Data);
}

void ORIInstruction::buildIR(IRData& Data) {
  isa::Operand rd = m_insn.rd();
  isa::Operand rs1  = m_insn.rs1();
  isa::Imm imm   = m_insn.imm();

  if ((imm & 0x800) != 0) {
    imm |= 0xFFFFF000;
  }

  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);

  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *rs1Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs1)});
  llvm::Value *rs1Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), rs1Ptr);

  llvm::Value *result = Data.Builder.CreateOr(rs1Val, Data.Builder.getInt32(imm));

  llvm::Value *rdPtr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rd)});
  Data.Builder.CreateStore(result, rdPtr);

  updatePC(Data);
}

void ANDIInstruction::buildIR(IRData& Data) {
  isa::Operand rd = m_insn.rd();
  isa::Operand rs1  = m_insn.rs1();
  isa::Imm imm   = m_insn.imm();

  if ((imm & 0x800) != 0) {
    imm |= 0xFFFFF000;
  }

  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);
  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *rs1Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs1)});
  llvm::Value *rs1Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), rs1Ptr);

  llvm::Value *result = Data.Builder.CreateAnd(rs1Val, Data.Builder.getInt32(imm));

  llvm::Value *rdPtr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rd)});
  Data.Builder.CreateStore(result, rdPtr);

  updatePC(Data);
}

void SLLIInstruction::buildIR(IRData& Data) {
  isa::Operand rd = m_insn.rd();
  isa::Operand rs1  = m_insn.rs1();
  isa::Imm imm   = m_insn.imm();

  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);
  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *rs1Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs1)});
  llvm::Value *rs1Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), rs1Ptr);

  llvm::Value *result = Data.Builder.CreateShl(rs1Val, Data.Builder.getInt32(imm));

  llvm::Value *rdPtr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rd)});
  Data.Builder.CreateStore(result, rdPtr);

  updatePC(Data);
}

void SRLIInstruction::buildIR(IRData& Data) {
  isa::Operand rd = m_insn.rd();
  isa::Operand rs1  = m_insn.rs1();
  isa::Imm imm   = m_insn.imm();

  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);
  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *rs1Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs1)});
  llvm::Value *rs1Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), rs1Ptr);

  llvm::Value *result = Data.Builder.CreateLShr(rs1Val, Data.Builder.getInt32(imm));

  llvm::Value *rdPtr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rd)});
  Data.Builder.CreateStore(result, rdPtr);

  updatePC(Data);
}

void SRAIInstruction::buildIR(IRData& Data) {
  isa::Operand rd = m_insn.rd();
  isa::Operand rs1  = m_insn.rs1();
  isa::Imm imm   = m_insn.imm();

  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);
  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *rs1Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs1)});
  llvm::Value *rs1Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), rs1Ptr);

  llvm::Value *result = Data.Builder.CreateAShr(rs1Val, Data.Builder.getInt32(imm));

  llvm::Value *rdPtr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rd)});
  Data.Builder.CreateStore(result, rdPtr);

  updatePC(Data);
}

void ADDInstruction::buildIR(IRData& Data) {
  isa::Operand rd = m_insn.rd();
  isa::Operand rs11 = m_insn.rs1();
  isa::Operand rs12 = m_insn.rs2();

  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);

  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *reg1Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs11)});
  llvm::Value *reg2Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs12)});
  llvm::Value *reg1Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), reg1Ptr);
  llvm::Value *reg2Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), reg2Ptr);

  llvm::Value *result = Data.Builder.CreateAdd(reg1Val, reg2Val);

  llvm::Value *rdPtr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rd)});
  Data.Builder.CreateStore(result, rdPtr);

  updatePC(Data);
}

void SUBInstruction::buildIR(IRData& Data) {
  isa::Operand rd = m_insn.rd();
  isa::Operand rs11 = m_insn.rs1();
  isa::Operand rs12 = m_insn.rs2();

  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);

  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *reg1Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs11)});
  llvm::Value *reg2Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs12)});
  llvm::Value *reg1Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), reg1Ptr);
  llvm::Value *reg2Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), reg2Ptr);

  llvm::Value *result = Data.Builder.CreateSub(reg1Val, reg2Val);

  llvm::Value *rdPtr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rd)});
  Data.Builder.CreateStore(result, rdPtr);

  updatePC(Data);
}

void SLLInstruction::buildIR(IRData& Data) {
  isa::Operand rd = m_insn.rd();
  isa::Operand rs11 = m_insn.rs1();
  isa::Operand rs12 = m_insn.rs2();

  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);
  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *rs11Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs11)});
  llvm::Value *rs11Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), rs11Ptr);
  llvm::Value *rs12Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs12)});
  llvm::Value *rs12Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), rs12Ptr);

  llvm::Value *result = Data.Builder.CreateShl(rs11Val, rs12Val);

  llvm::Value *rdPtr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rd)});
  Data.Builder.CreateStore(result, rdPtr);

  updatePC(Data);
}

void SRLInstruction::buildIR(IRData& Data) {
  isa::Operand rd = m_insn.rd();
  isa::Operand rs11 = m_insn.rs1();
  isa::Operand rs12 = m_insn.rs2();

  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);
  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *rs11Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs11)});
  llvm::Value *rs11Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), rs11Ptr);
  llvm::Value *rs12Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs12)});
  llvm::Value *rs12Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), rs12Ptr);

  llvm::Value *result = Data.Builder.CreateLShr(rs11Val, rs12Val);

  llvm::Value *rdPtr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rd)});
  Data.Builder.CreateStore(result, rdPtr);

  updatePC(Data);
}

void SRAInstruction::buildIR(IRData& Data) {
  isa::Operand rd = m_insn.rd();
  isa::Operand rs11 = m_insn.rs1();
  isa::Operand rs12 = m_insn.rs2();

  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);
  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *rs11Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs11)});
  llvm::Value *rs11Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), rs11Ptr);
  llvm::Value *rs12Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs12)});
  llvm::Value *rs12Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), rs12Ptr);

  llvm::Value *result = Data.Builder.CreateAShr(rs11Val, rs12Val);

  llvm::Value *rdPtr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rd)});
  Data.Builder.CreateStore(result, rdPtr);

  updatePC(Data);
}

void XORInstruction::buildIR(IRData& Data) {
  isa::Operand rd = m_insn.rd();
  isa::Operand rs11 = m_insn.rs1();
  isa::Operand rs12 = m_insn.rs2();

  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);

  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *reg1Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs11)});
  llvm::Value *reg2Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs12)});
  llvm::Value *reg1Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), reg1Ptr);
  llvm::Value *reg2Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), reg2Ptr);

  llvm::Value *result = Data.Builder.CreateXor(reg1Val, reg2Val);

  llvm::Value *rdPtr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rd)});
  Data.Builder.CreateStore(result, rdPtr);

  updatePC(Data);
}

void ORInstruction::buildIR(IRData& Data) {
  isa::Operand rd = m_insn.rd();
  isa::Operand rs11 = m_insn.rs1();
  isa::Operand rs12 = m_insn.rs2();

  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);

  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *reg1Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs11)});
  llvm::Value *reg2Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs12)});
  llvm::Value *reg1Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), reg1Ptr);
  llvm::Value *reg2Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), reg2Ptr);

  llvm::Value *result = Data.Builder.CreateOr(reg1Val, reg2Val);

  llvm::Value *rdPtr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rd)});
  Data.Builder.CreateStore(result, rdPtr);

  updatePC(Data);
}

void ANDInstruction::buildIR(IRData& Data) {
  isa::Operand rd = m_insn.rd();
  isa::Operand rs11 = m_insn.rs1();
  isa::Operand rs12 = m_insn.rs2();

  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);

  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *reg1Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs11)});
  llvm::Value *reg2Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs12)});
  llvm::Value *reg1Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), reg1Ptr);
  llvm::Value *reg2Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), reg2Ptr);

  llvm::Value *result = Data.Builder.CreateAnd(reg1Val, reg2Val);

  llvm::Value *rdPtr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rd)});
  Data.Builder.CreateStore(result, rdPtr);

  updatePC(Data);
}

void SLTInstruction::buildIR(IRData& Data) {
  isa::Operand rd = m_insn.rd();
  isa::Operand rs11 = m_insn.rs1();
  isa::Operand rs12 = m_insn.rs2();

  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);
  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *rdPtr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rd)});
  llvm::Value *rs11Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs11)});
  llvm::Value *src1Value = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), rs11Ptr);  
  llvm::Value *rs12Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs12)});
  llvm::Value *src2Value = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), rs12Ptr); 
  llvm::Value *cond = Data.Builder.CreateICmpSLT(src1Value, src2Value);
  Data.Builder.CreateStore(Data.Builder.CreateZExt(cond, Data.Builder.getInt32Ty()), rdPtr);

  updatePC(Data); 
}

void SLTUInstruction::buildIR(IRData& Data) {
  isa::Operand rd = m_insn.rd();
  isa::Operand rs11 = m_insn.rs1();
  isa::Operand rs12 = m_insn.rs2();

  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);
  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);

  llvm::Value *rdPtr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rd)});
  llvm::Value *rs11Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs11)});
  llvm::Value *src1Value = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), rs11Ptr);  
  llvm::Value *rs12Ptr = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                      {Data.Builder.getInt32(0), Data.Builder.getInt32(rs12)});
  llvm::Value *src2Value = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), rs12Ptr); 
  llvm::Value *cond = Data.Builder.CreateICmpULT(src1Value, src2Value);
  Data.Builder.CreateStore(Data.Builder.CreateZExt(cond, Data.Builder.getInt32Ty()), rdPtr);

  updatePC(Data);
}

void FENCEInstruction::buildIR(IRData& Data) {
  updatePC(Data);
}

void FENCETSOInstruction::buildIR(IRData& Data) {
  updatePC(Data); 
}

void PAUSEInstruction::buildIR(IRData& Data) {
  updatePC(Data);
}

void ECALLInstruction::buildIR(IRData& Data) {
  updatePC(Data);
}

void EBREAKInstruction::buildIR(IRData& Data) {
  updatePC(Data);
}

} // end anonymouse namespace

void buildInstruction(IRData& Data, isa::Instruction insn) {
  switch (insn.opcode()) {
    case prot::isa::Opcode::kLUI:
      LUIInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kAUIPC:
      AUIPCInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kJAL:
      JALInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kJALR:
      JALRInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kBEQ:
      BEQInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kBNE:
      BNEInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kBLT:
      BLTInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kBGE:
      BGEInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kBLTU:
      BLTUInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kBGEU:
      BGEUInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kLB:
      LBInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kLH:
      LHInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kLW:
      LWInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kLBU:
      LBUInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kLHU:
      LHUInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kSB:
      SBInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kSH:
      SHInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kSW:
      SWInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kADDI:
      ADDIInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kSLTI:
      SLTIInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kSLTIU:
      SLTIUInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kXORI:
      XORIInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kORI:
      ORIInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kANDI:
      ANDIInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kSLLI:
      SLLIInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kSRLI:
      SRLIInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kSRAI:
      SRAIInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kADD:
      ADDInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kSUB:
      SUBInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kSLL:
      SLLInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kSLT:
      SLTInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kSLTU:
      SLTUInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kXOR:
      XORInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kSRL:
      SRLInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kSRA:
      SRAInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kOR:
      ORInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kAND:
      ANDInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kFENCE:
      FENCEInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kPAUSE:
      PAUSEInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kECALL:
      ECALLInstruction{insn}.buildIR(Data);
      return;
    case prot::isa::Opcode::kEBREAK:
      EBREAKInstruction{insn}.buildIR(Data);
      return;
    case isa::Opcode::kSBREAK:
      EBREAKInstruction{insn}.buildIR(Data);
      return;
    case isa::Opcode::kSCALL:
      ECALLInstruction{insn}.buildIR(Data);
      return;
    case isa::Opcode::kNumOpcodes:
      break;
  }
}


} // end namespace prot::engine

