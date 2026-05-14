extern "C" {
#include "ir.h"
#include "ir_builder.h"
}

#include "prot/jit/base.hh"
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <unordered_map>

namespace prot::engine {
namespace {

#define PROT_IR_COMP_R_OP(OP, IR_OP)                                           \
  case k##OP: {                                                                \
    ir_ref rs1 = loadReg(insn.rs1());                                          \
    ir_ref rs2 = loadReg(insn.rs2());                                          \
    setDst(insn.rd(), ir_ZEXT_U32(IR_OP(rs1, rs2)));                           \
    break;                                                                     \
  }

#define PROT_IR_COMP_I_OP(OP, IR_OP)                                           \
  case k##OP: {                                                                \
    ir_ref rs1 = loadReg(insn.rs1());                                          \
    ir_ref imm = ir_CONST_I32(insn.imm());                                     \
    setDst(insn.rd(), ir_ZEXT_U32(IR_OP(rs1, imm)));                           \
    break;                                                                     \
  }

#define PROT_IR_SHIFT_R_OP(OP, IR_OP)                                          \
  case k##OP: {                                                                \
    ir_ref rs1 = loadReg(insn.rs1());                                          \
    ir_ref shamt = ir_AND_U32(loadReg(insn.rs2()), ir_CONST_U32(0x1F));        \
    setDst(insn.rd(), IR_OP(rs1, shamt));                                      \
    break;                                                                     \
  }

#define PROT_IR_SHIFT_I_OP(OP, IR_OP)                                          \
  case k##OP: {                                                                \
    ir_ref rs1 = loadReg(insn.rs1());                                          \
    ir_ref shamt = ir_CONST_U32(insn.imm());                                   \
    setDst(insn.rd(), IR_OP(rs1, shamt));                                      \
    break;                                                                     \
  }

#define PROT_IR_SHIFT_OP(OP, IR_OP)                                            \
  PROT_IR_R_OP(OP, IR_OP)                                                      \
  PROT_IR_I_OP(OP##I, IR_OP)

#define PROT_IR_R_OP(OP, IR_OP)                                                \
  case k##OP: {                                                                \
    ir_ref rs1 = loadReg(insn.rs1());                                          \
    ir_ref rs2 = loadReg(insn.rs2());                                          \
    setDst(insn.rd(), IR_OP(rs1, rs2));                                        \
    break;                                                                     \
  }

#define PROT_IR_I_OP(OP, IR_OP)                                                \
  case k##OP: {                                                                \
    ir_ref rs1 = loadReg(insn.rs1());                                          \
    ir_ref imm = ir_CONST_I32(insn.imm());                                     \
    setDst(insn.rd(), IR_OP(rs1, imm));                                        \
    break;                                                                     \
  }

#define PROT_IR_STORE_OP(OP, FUNC, IR_OP)                                      \
  case k##OP: {                                                                \
    ir_ref rs1 = loadReg(insn.rs1());                                          \
    ir_ref addr = ir_ADD_U32(rs1, ir_CONST_I32(insn.imm()));                   \
    ir_ref val = loadReg(insn.rs2());                                          \
    ir_CALL_3(IR_VOID, m_func_proto_map[FUNC], state_ptr, addr, IR_OP(val));   \
    break;                                                                     \
  }

#define PROT_IR_LOAD_OP(OP, FUNC, TYPE, IR_OP)                                 \
  case k##OP: {                                                                \
    ir_ref rs1 = loadReg(insn.rs1());                                          \
    ir_ref addr = ir_ADD_U32(rs1, ir_CONST_I32(insn.imm()));                   \
    ir_ref val =                                                               \
        IR_OP(ir_CALL_2(TYPE, m_func_proto_map[FUNC], state_ptr, addr));       \
    setDst(insn.rd(), val);                                                    \
    break;                                                                     \
  }

#define PROT_IR_ALU_OP(OP, MIR_OP)                                             \
  PROT_IR_R_OP(OP, MIR_OP)                                                     \
  PROT_IR_I_OP(OP##I, MIR_OP)

#define PROT_IR_B_COND_OP(OP, COND)                                            \
  case k##OP: {                                                                \
    ir_ref rs1 = loadReg(insn.rs1());                                          \
    ir_ref rs2 = loadReg(insn.rs2());                                          \
    pc = ir_COND_U32(COND(rs1, rs2), ir_ADD_U32(pc, ir_CONST_U32(insn.imm())), \
                     ir_ADD_U32(pc, ir_CONST_U32(isa::kWordSize)));            \
    break;                                                                     \
  }

using JitFunction = void (*)(CPUState &);

template <typename T> void storeHelper(CPUState &state, isa::Addr addr, T val) {
  state.memory->write(addr, val);
}
template <typename T> T loadHelper(CPUState &state, isa::Addr addr) {
  return state.memory->read<T>(addr);
}
void syscallHelper(CPUState &state) { state.emulateSysCall(); }

class IRJit : public Translator {
public:
  static constexpr std::size_t kOptLevel = 2;
  static constexpr std::size_t kConstsLimit = 1024;
  static constexpr std::size_t kInsnsLimit = 4096;

private:
  [[nodiscard]] JitFunction translate(const BBInfo &info) override;
  void run(ir_ctx *ctx, const BBInfo &info);

  void registerHelpers(ir_ctx *ctx);

  std::unordered_map<std::string, ir_ref> m_func_proto_map;
};

void IRJit::registerHelpers(ir_ctx *ctx) {

  ir_ref proto_lw = ir_proto_2(ctx, IR_CC_DEFAULT, IR_U32, IR_ADDR, IR_U32);
  m_func_proto_map["loadHelperWord"] = proto_lw;
  m_func_proto_map["loadHelperWord_func"] = ir_const_func_addr(
      ctx, reinterpret_cast<uintptr_t>(loadHelper<isa::Word>), proto_lw);

  ir_ref proto_lh = ir_proto_2(ctx, IR_CC_DEFAULT, IR_I16, IR_ADDR, IR_U32);
  m_func_proto_map["loadHelperHalf"] = proto_lh;
  m_func_proto_map["loadHelperHalf_func"] = ir_const_func_addr(
      ctx, reinterpret_cast<uintptr_t>(loadHelper<isa::Half>), proto_lh);

  ir_ref proto_lb = ir_proto_2(ctx, IR_CC_DEFAULT, IR_I8, IR_ADDR, IR_U32);
  m_func_proto_map["loadHelperByte"] = proto_lb;
  m_func_proto_map["loadHelperByte_func"] = ir_const_func_addr(
      ctx, reinterpret_cast<uintptr_t>(loadHelper<isa::Byte>), proto_lb);

  ir_ref proto_lhu = ir_proto_2(ctx, IR_CC_DEFAULT, IR_U16, IR_ADDR, IR_U32);
  m_func_proto_map["loadHelperUHalf"] = proto_lhu;
  m_func_proto_map["loadHelperUHalf_func"] = ir_const_func_addr(
      ctx, reinterpret_cast<uintptr_t>(loadHelper<isa::Half>), proto_lhu);

  ir_ref proto_lbu = ir_proto_2(ctx, IR_CC_DEFAULT, IR_U8, IR_ADDR, IR_U32);
  m_func_proto_map["loadHelperUByte"] = proto_lbu;
  m_func_proto_map["loadHelperUByte_func"] = ir_const_func_addr(
      ctx, reinterpret_cast<uintptr_t>(loadHelper<isa::Byte>), proto_lbu);

  ir_ref proto_sw =
      ir_proto_3(ctx, IR_CC_DEFAULT, IR_VOID, IR_ADDR, IR_U32, IR_U32);
  m_func_proto_map["storeHelperWord"] = proto_sw;
  m_func_proto_map["storeHelperWord_func"] = ir_const_func_addr(
      ctx, reinterpret_cast<uintptr_t>(storeHelper<isa::Word>), proto_sw);

  ir_ref proto_sh =
      ir_proto_3(ctx, IR_CC_DEFAULT, IR_VOID, IR_ADDR, IR_U32, IR_I16);
  m_func_proto_map["storeHelperHalf"] = proto_sh;
  m_func_proto_map["storeHelperHalf_func"] = ir_const_func_addr(
      ctx, reinterpret_cast<uintptr_t>(storeHelper<isa::Half>), proto_sh);

  ir_ref proto_sb =
      ir_proto_3(ctx, IR_CC_DEFAULT, IR_VOID, IR_ADDR, IR_U32, IR_I8);
  m_func_proto_map["storeHelperByte"] = proto_sb;
  m_func_proto_map["storeHelperByte_func"] = ir_const_func_addr(
      ctx, reinterpret_cast<uintptr_t>(storeHelper<isa::Byte>), proto_sb);

  ir_ref proto_ecall = ir_proto_1(ctx, IR_CC_DEFAULT, IR_VOID, IR_ADDR);
  m_func_proto_map["syscallHelper"] = proto_ecall;
  m_func_proto_map["syscallHelper_func"] = ir_const_func_addr(
      ctx, reinterpret_cast<uintptr_t>(syscallHelper), proto_ecall);
}

void IRJit::run(ir_ctx *ctx, const BBInfo &info) {
  ir_START();
  ir_ref state_ptr = ir_PARAM(IR_ADDR, "state", 1);

  ir_ref pc = ir_LOAD_U32(ir_ADD_OFFSET(state_ptr, offsetof(CPUState, pc)));

  auto regAddr = [&](uint32_t reg) {
    return ir_ADD_OFFSET(state_ptr,
                         offsetof(CPUState, regs) + isa::kWordSize * reg);
  };
  auto loadReg = [&](uint32_t reg) -> ir_ref {
    if (reg == 0)
      return ir_CONST_U32(0);
    return ir_LOAD_U32(regAddr(reg));
  };
  auto setDst = [&](uint32_t rd, ir_ref val) {
    if (rd != 0)
      ir_STORE(regAddr(rd), val);
  };

  for (const auto &insn : info.insns) {
    switch (insn.opcode()) {
      using enum isa::Opcode;

      PROT_IR_ALU_OP(ADD, ir_ADD_U32)
      PROT_IR_ALU_OP(AND, ir_AND_U32)
      PROT_IR_ALU_OP(OR, ir_OR_U32)
      PROT_IR_ALU_OP(XOR, ir_XOR_U32)

      PROT_IR_R_OP(SUB, ir_SUB_U32)

      PROT_IR_SHIFT_OP(SLL, ir_SHL_U32)
      PROT_IR_SHIFT_OP(SRL, ir_SHR_U32)
      PROT_IR_SHIFT_OP(SRA, ir_SAR_U32)

      PROT_IR_COMP_R_OP(SLT, ir_LT)
      PROT_IR_COMP_R_OP(SLTU, ir_ULT)

      PROT_IR_COMP_I_OP(SLTI, ir_LT)
      PROT_IR_COMP_I_OP(SLTIU, ir_ULT)

      PROT_IR_B_COND_OP(BEQ, ir_EQ)
      PROT_IR_B_COND_OP(BNE, ir_NE)
      PROT_IR_B_COND_OP(BLT, ir_LT)
      PROT_IR_B_COND_OP(BGE, ir_GE)
      PROT_IR_B_COND_OP(BLTU, ir_ULT)
      PROT_IR_B_COND_OP(BGEU, ir_UGE)

      PROT_IR_LOAD_OP(LB, "loadHelperByte_func", IR_I8, ir_SEXT_U32)
      PROT_IR_LOAD_OP(LH, "loadHelperHalf_func", IR_I16, ir_SEXT_U32)
      PROT_IR_LOAD_OP(LW, "loadHelperWord_func", IR_I32, ir_SEXT_U32)
      PROT_IR_LOAD_OP(LBU, "loadHelperUByte_func", IR_U8, ir_ZEXT_U32)
      PROT_IR_LOAD_OP(LHU, "loadHelperUHalf_func", IR_U16, ir_ZEXT_U32)

      PROT_IR_STORE_OP(SB, "storeHelperByte_func", ir_TRUNC_I8)
      PROT_IR_STORE_OP(SH, "storeHelperHalf_func", ir_TRUNC_I16)
      PROT_IR_STORE_OP(SW, "storeHelperWord_func", ir_TRUNC_I32)

    case kJAL: {
      ir_ref ret_addr = ir_ADD_U32(pc, ir_CONST_U32(isa::kWordSize));
      setDst(insn.rd(), ret_addr);
      pc = ir_ADD_U32(pc, ir_CONST_U32(insn.imm()));
      break;
    }
    case kJALR: {
      ir_ref ret_addr = ir_ADD_U32(pc, ir_CONST_U32(isa::kWordSize));
      setDst(insn.rd(), ret_addr);
      ir_ref rs1 = loadReg(insn.rs1());
      ir_ref target = ir_ADD_U32(rs1, ir_CONST_I32(insn.imm()));
      pc = ir_AND_U32(target, ir_CONST_U32(~1U));
      break;
    }

    case kLUI: {
      setDst(insn.rd(), ir_CONST_U32(insn.imm()));
      break;
    }
    case kAUIPC: {
      ir_ref res = ir_ADD_U32(pc, ir_CONST_U32(insn.imm()));
      setDst(insn.rd(), res);
      break;
    }
    case kECALL: {
      ir_CALL_1(IR_VOID, m_func_proto_map["syscallHelper_func"], state_ptr);
      break;
    }

    case kFENCE:
    case kEBREAK:
    case kPAUSE:
    case kSBREAK:
    case kSCALL:
      break;
    case kNumOpcodes:
      throw std::invalid_argument("Unexpected insn id");
    default:
      break;
    }

    if (!isa::changesPC(insn.opcode())) {
      pc = ir_ADD_U32(pc, ir_CONST_U32(isa::kWordSize));
    }
  }

  ir_STORE(ir_ADD_OFFSET(state_ptr, offsetof(CPUState, pc)), pc);

  ir_ref icount =
      ir_LOAD_U64(ir_ADD_OFFSET(state_ptr, offsetof(CPUState, icount)));
  icount = ir_ADD_U64(icount, ir_CONST_U32(info.insns.size()));
  ir_STORE(ir_ADD_OFFSET(state_ptr, offsetof(CPUState, icount)), icount);

  ir_RETURN(IR_UNUSED);
}

JitFunction IRJit::translate(const BBInfo &info) {
  ir_ctx ctx;

  ir_init(&ctx, IR_FUNCTION | IR_OPT_FOLDING | IR_OPT_CFG | IR_OPT_CODEGEN,
          kConstsLimit, kInsnsLimit);

  registerHelpers(&ctx);

  run(&ctx, info);

  size_t codeSize;
  void *nativeCode = ir_jit_compile(&ctx, kOptLevel, &codeSize);
  if (!nativeCode) {
    throw std::runtime_error("IR JIT compilation failed");
  }

  ir_free(&ctx);

  return reinterpret_cast<JitFunction>(nativeCode);
}

} // namespace

std::unique_ptr<Translator> makeIrJit() { return std::make_unique<IRJit>(); }
} // namespace prot::engine
