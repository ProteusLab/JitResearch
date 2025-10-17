#include "prot/jit/llvmbasedjitbuilder.hh"
#include "prot/isa.hh"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/Type.h"

namespace prot::engine {

namespace {

llvm::Type* getSegmentType(llvm::LLVMContext& Ctx) {
  if (auto* type = llvm::StructType::getTypeByName(Ctx, "SegmentManager")) {
    return type;
  }

  llvm::StructType *segTy = llvm::StructType::create(Ctx, "SegmentManager");
  llvm::Type *i8Ty = llvm::Type::getInt8Ty(Ctx);
  llvm::Type *i8PtrTy = llvm::PointerType::getUnqual(i8Ty);

  segTy->setBody({ i8PtrTy, llvm::Type::getInt32Ty(Ctx), llvm::Type::getInt32Ty(Ctx) });
  return segTy;
}

llvm::Type* getMemoryType(llvm::LLVMContext& Ctx) {
  if (auto* type = llvm::StructType::getTypeByName(Ctx, "MemoryManager")) {
    return type;
  }
  llvm::StructType *memTy = llvm::StructType::create(Ctx, "MemoryManager");

  memTy->setBody({ llvm::PointerType::getUnqual(llvm::PointerType::getUnqual(getSegmentType(Ctx))), llvm::Type::getInt32Ty(Ctx) });
  return memTy;
}

llvm::Type* getMemoryPointerType(llvm::LLVMContext& Ctx) {
  return llvm::PointerType::getUnqual(getMemoryType(Ctx));
}

llvm::Type* getCPUStateType(llvm::LLVMContext& Ctx) {
  if (auto* type = llvm::StructType::getTypeByName(Ctx, "CPUState")) {
    return type;
  }
  auto *cpuStructTy = llvm::StructType::create(Ctx, "CPUState");
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Ctx), 32);
  cpuStructTy->setBody({regsArrTy, llvm::Type::getInt32Ty(Ctx), getMemoryPointerType(Ctx)});
  return cpuStructTy;
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
  isa::Operand rd = m_insn.rd();
  isa::Operand rs1 = m_insn.rs1();
  isa::Imm offset = m_insn.imm();
  
  if ((offset & 0x800) != 0) {
    offset |= 0xFFFFF000;
  }
  
  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);

  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);
  llvm::Value *memoryManagerPtrPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 2);
  llvm::Value *memoryManagerPtr = Data.Builder.CreateLoad(Data.Builder.getPtrTy(), memoryManagerPtrPtr);
  llvm::Value *rdPtr  = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                        {Data.Builder.getInt32(0), Data.Builder.getInt32(rd)});
  llvm::Value *rs1Ptr  = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                        {Data.Builder.getInt32(0), Data.Builder.getInt32(rs1)});
  llvm::Value *rs1Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), rs1Ptr);
  llvm::Value *address = Data.Builder.CreateAdd(rs1Val, Data.Builder.getInt32(offset));
  llvm::Value *readMemory = Data.Builder.CreateCall(Data.MemoryFunctions[0], {memoryManagerPtr, address});
  
  Data.Builder.CreateStore(Data.Builder.CreateSExt(readMemory, Data.Builder.getInt32Ty()), rdPtr);
  
  updatePC(Data);
}

void LHInstruction::buildIR(IRData& Data) {
  isa::Operand rd = m_insn.rd();
  isa::Operand rs1 = m_insn.rs1();
  isa::Imm offset = m_insn.imm();
  
  if ((offset & 0x800) != 0) {
    offset |= 0xFFFFF000;
  }
  
  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);

  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);
  llvm::Value *memoryManagerPtrPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 2);
  llvm::Value *memoryManagerPtr = Data.Builder.CreateLoad(Data.Builder.getPtrTy(), memoryManagerPtrPtr);
  llvm::Value *rdPtr  = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                        {Data.Builder.getInt32(0), Data.Builder.getInt32(rd)});
  llvm::Value *rs1Ptr  = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                        {Data.Builder.getInt32(0), Data.Builder.getInt32(rs1)});
  llvm::Value *rs1Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), rs1Ptr);
  llvm::Value *address = Data.Builder.CreateAdd(rs1Val, Data.Builder.getInt32(offset));
  llvm::Value *readMemory = Data.Builder.CreateCall(Data.MemoryFunctions[1], {memoryManagerPtr, address});
  
  Data.Builder.CreateStore(Data.Builder.CreateSExt(readMemory, Data.Builder.getInt32Ty()), rdPtr);
  
  updatePC(Data);
}

void LWInstruction::buildIR(IRData& Data) {
  isa::Operand rd = m_insn.rd();
  isa::Operand rs1 = m_insn.rs1();
  isa::Imm offset = m_insn.imm();
  
  if ((offset & 0x800) != 0) {
    offset |= 0xFFFFF000;
  }
  
  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);

  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);
  llvm::Value *memoryManagerPtrPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 2);
  llvm::Value *memoryManagerPtr = Data.Builder.CreateLoad(Data.Builder.getPtrTy(), memoryManagerPtrPtr);
  llvm::Value *rdPtr  = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                        {Data.Builder.getInt32(0), Data.Builder.getInt32(rd)});
  llvm::Value *rs1Ptr  = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                        {Data.Builder.getInt32(0), Data.Builder.getInt32(rs1)});
  llvm::Value *rs1Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), rs1Ptr);
  llvm::Value *address = Data.Builder.CreateAdd(rs1Val, Data.Builder.getInt32(offset));
  llvm::Value *readMemory = Data.Builder.CreateCall(Data.MemoryFunctions[2], {memoryManagerPtr, address});
  
  Data.Builder.CreateStore(Data.Builder.CreateSExt(readMemory, Data.Builder.getInt32Ty()), rdPtr);
  
  updatePC(Data);
}

void LBUInstruction::buildIR(IRData& Data) {
  isa::Operand rd = m_insn.rd();
  isa::Operand rs1 = m_insn.rs1();
  isa::Imm offset = m_insn.imm();
  
  if ((offset & 0x800) != 0) {
    offset |= 0xFFFFF000;
  }

  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);

  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);
  llvm::Value *memoryManagerPtrPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 2);
  llvm::Value *memoryManagerPtr = Data.Builder.CreateLoad(Data.Builder.getPtrTy(), memoryManagerPtrPtr);
  llvm::Value *rdPtr  = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                        {Data.Builder.getInt32(0), Data.Builder.getInt32(rd)});
  llvm::Value *rs1Ptr  = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                        {Data.Builder.getInt32(0), Data.Builder.getInt32(rs1)});
  llvm::Value *rs1Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), rs1Ptr);
  llvm::Value *address = Data.Builder.CreateAdd(rs1Val, Data.Builder.getInt32(offset));
  llvm::Value *readMemory = Data.Builder.CreateCall(Data.MemoryFunctions[0], {memoryManagerPtr, address});
  
  Data.Builder.CreateStore(Data.Builder.CreateZExt(readMemory, Data.Builder.getInt32Ty()), rdPtr);
  
  updatePC(Data);
}

void LHUInstruction::buildIR(IRData& Data) {
  isa::Operand rd = m_insn.rd();
  isa::Operand rs1 = m_insn.rs1();
  isa::Imm offset = m_insn.imm();
  
  if ((offset & 0x800) != 0) {
    offset |= 0xFFFFF000;
  }
  
  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);

  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);
  llvm::Value *memoryManagerPtrPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 2);
  llvm::Value *memoryManagerPtr = Data.Builder.CreateLoad(Data.Builder.getPtrTy(), memoryManagerPtrPtr);
  llvm::Value *rdPtr  = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                        {Data.Builder.getInt32(0), Data.Builder.getInt32(rd)});
  llvm::Value *rs1Ptr  = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                        {Data.Builder.getInt32(0), Data.Builder.getInt32(rs1)});
  llvm::Value *rs1Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), rs1Ptr);
  llvm::Value *address = Data.Builder.CreateAdd(rs1Val, Data.Builder.getInt32(offset));
  llvm::Value *readMemory = Data.Builder.CreateCall(Data.MemoryFunctions[1], {memoryManagerPtr, address});
  
  Data.Builder.CreateStore(Data.Builder.CreateZExt(readMemory, Data.Builder.getInt32Ty()), rdPtr);
  
  updatePC(Data);
}

void SBInstruction::buildIR(IRData& Data) {
  isa::Operand rs11 = m_insn.rs1();
  isa::Operand rs12 = m_insn.rs2();
  isa::Imm offset = m_insn.imm();
  
  if ((offset & 0x800) != 0) {
    offset |= 0xFFFFF000;
  }
  
  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);

  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);
  llvm::Value *memoryManagerPtrPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 2);
  llvm::Value *rs11Ptr  = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                        {Data.Builder.getInt32(0), Data.Builder.getInt32(rs11)});
  llvm::Value *rs12Ptr  = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                        {Data.Builder.getInt32(0), Data.Builder.getInt32(rs12)});

                                          
  llvm::Value *rs11Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), rs11Ptr);
  llvm::Value *rs12Val = Data.Builder.CreateLoad(Data.Builder.getInt8Ty(), rs12Ptr);
  llvm::Value *memoryManagerPtr = Data.Builder.CreateLoad(Data.Builder.getPtrTy(), memoryManagerPtrPtr);
  llvm::Value *address = Data.Builder.CreateAdd(rs11Val, Data.Builder.getInt32(offset));

  Data.Builder.CreateCall(Data.MemoryFunctions[3], {memoryManagerPtr, address, rs12Val});

  updatePC(Data);
}

void SHInstruction::buildIR(IRData& Data) {
  isa::Operand rs11 = m_insn.rs1();
  isa::Operand rs12 = m_insn.rs2();
  isa::Imm offset = m_insn.imm();
  
  if ((offset & 0x800) != 0) {
    offset |= 0xFFFFF000;
  }
  
  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);

  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);
  llvm::Value *memoryManagerPtrPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 2);
  llvm::Value *rs11Ptr  = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                        {Data.Builder.getInt32(0), Data.Builder.getInt32(rs11)});
  llvm::Value *rs12Ptr  = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                        {Data.Builder.getInt32(0), Data.Builder.getInt32(rs12)});

                                          
  llvm::Value *rs11Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), rs11Ptr);
  llvm::Value *rs12Val = Data.Builder.CreateLoad(Data.Builder.getInt16Ty(), rs12Ptr);
  llvm::Value *memoryManagerPtr = Data.Builder.CreateLoad(Data.Builder.getPtrTy(), memoryManagerPtrPtr);
  llvm::Value *address = Data.Builder.CreateAdd(rs11Val, Data.Builder.getInt32(offset));

  Data.Builder.CreateCall(Data.MemoryFunctions[4], {memoryManagerPtr, address, rs12Val});

  updatePC(Data);
}

void SWInstruction::buildIR(IRData& Data) {
  isa::Operand rs11 = m_insn.rs1();
  isa::Operand rs12 = m_insn.rs2();
  isa::Imm offset = m_insn.imm();
  
  if ((offset & 0x800) != 0) {
    offset |= 0xFFFFF000;
  }

  auto *cpuStructTy = getCPUStateType(Data.Builder.getContext());
  auto *regsArrTy   = llvm::ArrayType::get(llvm::Type::getInt32Ty(Data.Builder.getContext()), 32);
  auto *cpuArg      = Data.CurrentFunction->getArg(0);

  llvm::Value *regsPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 0);
  llvm::Value *memoryManagerPtrPtr = Data.Builder.CreateStructGEP(cpuStructTy, cpuArg, 2);
  llvm::Value *rs11Ptr  = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                        {Data.Builder.getInt32(0), Data.Builder.getInt32(rs11)});
  llvm::Value *rs12Ptr  = Data.Builder.CreateInBoundsGEP(regsArrTy, regsPtr,
                                        {Data.Builder.getInt32(0), Data.Builder.getInt32(rs12)});

                                          
  llvm::Value *rs11Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), rs11Ptr);
  llvm::Value *rs12Val = Data.Builder.CreateLoad(Data.Builder.getInt32Ty(), rs12Ptr);
  llvm::Value *memoryManagerPtr = Data.Builder.CreateLoad(Data.Builder.getPtrTy(), memoryManagerPtrPtr);
  llvm::Value *address = Data.Builder.CreateAdd(rs11Val, Data.Builder.getInt32(offset));

  Data.Builder.CreateCall(Data.MemoryFunctions[5], {memoryManagerPtr, address, rs12Val});

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


} // end namespace prot::engine

