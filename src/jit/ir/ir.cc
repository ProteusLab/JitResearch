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
    pc = ir_COND_U32(                                                          \
        COND(rs1, rs2),                                                        \
        ir_CONST_U32(static_cast<uint32_t>(curPC + insn.imm())),               \
        ir_CONST_U32(static_cast<uint32_t>(curPC + isa::kWordSize)));          \
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
  ir_ref proto_ecall = ir_proto_1(ctx, IR_CC_DEFAULT, IR_VOID, IR_ADDR);
  m_func_proto_map["syscallHelper"] = proto_ecall;
  m_func_proto_map["syscallHelper_func"] = ir_const_func_addr(
      ctx, reinterpret_cast<uintptr_t>(syscallHelper), proto_ecall);
}

void IRJit::run(ir_ctx *ctx, const BBInfo &info) {
  ir_START();
  ir_ref state_ptr = ir_PARAM(IR_ADDR, "state", 1);

  ir_ref pc = IR_UNUSED;
  isa::Addr curPC = info.startPC;
  ir_ref mem_base =
      ir_LOAD_U64(ir_ADD_OFFSET(state_ptr, offsetof(CPUState, mem_base)));

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

  auto getHostAddr = [&](ir_ref guest_addr) {
    return ir_ADD_U64(mem_base, ir_ZEXT_U64(guest_addr));
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

    case kLB: {
      ir_ref rs1 = loadReg(insn.rs1());
      ir_ref guest_addr = ir_ADD_U32(rs1, ir_CONST_I32(insn.imm()));
      ir_ref host_addr = getHostAddr(guest_addr);
      ir_ref val = ir_LOAD_U8(host_addr);
      setDst(insn.rd(), ir_SEXT_U32(val));
      break;
    }
    case kLH: {
      ir_ref rs1 = loadReg(insn.rs1());
      ir_ref guest_addr = ir_ADD_U32(rs1, ir_CONST_I32(insn.imm()));
      ir_ref host_addr = getHostAddr(guest_addr);
      ir_ref val = ir_LOAD_U16(host_addr);
      setDst(insn.rd(), ir_SEXT_U32(val));
      break;
    }
    case kLW: {
      ir_ref rs1 = loadReg(insn.rs1());
      ir_ref guest_addr = ir_ADD_U32(rs1, ir_CONST_I32(insn.imm()));
      ir_ref host_addr = getHostAddr(guest_addr);
      ir_ref val = ir_LOAD_U32(host_addr);
      setDst(insn.rd(), val);
      break;
    }
    case kLBU: {
      ir_ref rs1 = loadReg(insn.rs1());
      ir_ref guest_addr = ir_ADD_U32(rs1, ir_CONST_I32(insn.imm()));
      ir_ref host_addr = getHostAddr(guest_addr);
      ir_ref val = ir_LOAD_U8(host_addr);
      setDst(insn.rd(), ir_ZEXT_U32(val));
      break;
    }
    case kLHU: {
      ir_ref rs1 = loadReg(insn.rs1());
      ir_ref guest_addr = ir_ADD_U32(rs1, ir_CONST_I32(insn.imm()));
      ir_ref host_addr = getHostAddr(guest_addr);
      ir_ref val = ir_LOAD_U16(host_addr);
      setDst(insn.rd(), ir_ZEXT_U32(val));
      break;
    }
    case kSW: {
      ir_ref rs1 = loadReg(insn.rs1());
      ir_ref guest_addr = ir_ADD_U32(rs1, ir_CONST_I32(insn.imm()));
      ir_ref host_addr = getHostAddr(guest_addr);
      ir_ref val = loadReg(insn.rs2());
      ir_STORE(host_addr, val);
      break;
    }
    case kSB: {
      ir_ref rs1 = loadReg(insn.rs1());
      ir_ref guest_addr = ir_ADD_U32(rs1, ir_CONST_I32(insn.imm()));
      ir_ref host_addr = getHostAddr(guest_addr);
      ir_ref val = loadReg(insn.rs2());
      ir_STORE(host_addr, ir_TRUNC_I8(val));
      break;
    }
    case kSH: {
      ir_ref rs1 = loadReg(insn.rs1());
      ir_ref guest_addr = ir_ADD_U32(rs1, ir_CONST_I32(insn.imm()));
      ir_ref host_addr = getHostAddr(guest_addr);
      ir_ref val = loadReg(insn.rs2());
      ir_STORE(host_addr, ir_TRUNC_I16(val));
      break;
    }

    case kJAL: {
      setDst(insn.rd(),
             ir_CONST_U32(static_cast<uint32_t>(curPC + isa::kWordSize)));
      pc = ir_CONST_U32(static_cast<uint32_t>(curPC + insn.imm()));
      break;
    }
    case kJALR: {
      setDst(insn.rd(),
             ir_CONST_U32(static_cast<uint32_t>(curPC + isa::kWordSize)));
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
      setDst(insn.rd(),
             ir_CONST_U32(static_cast<uint32_t>(curPC + insn.imm())));
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
      curPC += isa::kWordSize;
    }
  }

  if (info.insns.empty() || !isa::changesPC(info.insns.back().opcode())) {
    pc = ir_CONST_U32(static_cast<uint32_t>(curPC));
  }

  ir_ref icount =
      ir_LOAD_U64(ir_ADD_OFFSET(state_ptr, offsetof(CPUState, icount)));
  icount = ir_ADD_U64(icount, ir_CONST_U32(info.insns.size()));
  ir_STORE(ir_ADD_OFFSET(state_ptr, offsetof(CPUState, icount)), icount);

  // Block chaining
  const bool lastIsJalr =
      !info.insns.empty() && info.insns.back().opcode() == isa::Opcode::kJALR;
  if (lastIsJalr) {
    ir_STORE(ir_ADD_OFFSET(state_ptr, offsetof(CPUState, pc)), pc);
    ir_RETURN(IR_UNUSED);
    return;
  }

  // if (hasEcall) {
  //   // A syscall may have requested program exit; never chain past it.
  //   ir_ref fin =
  //       ir_LOAD_U8(ir_ADD_OFFSET(state_ptr, offsetof(CPUState, finished)));
  //   ir_ref if_fin = ir_IF(fin);
  //   ir_IF_TRUE(if_fin);
  //   ir_STORE(ir_ADD_OFFSET(state_ptr, offsetof(CPUState, pc)), pc);
  //   ir_RETURN(IR_UNUSED);
  //   ir_IF_FALSE(if_fin);
  // }

  ir_ref cache_base =
      ir_LOAD_U64(ir_ADD_OFFSET(state_ptr, offsetof(CPUState, tb_cache_base)));
  ir_ref hash =
      ir_AND_U32(ir_SHR_U32(pc, ir_CONST_U32(kTbCacheGranularityLog2)),
                 ir_CONST_U32(kTbCacheMask));
  ir_ref entry =
      ir_ADD_U64(cache_base, ir_MUL_U64(ir_ZEXT_U64(hash),
                                        ir_CONST_U64(sizeof(TbCacheEntry))));
  ir_ref gpa =
      ir_LOAD_U32(ir_ADD_U64(entry, ir_CONST_U64(offsetof(TbCacheEntry, gpa))));
  ir_ref next = ir_LOAD_A(entry);

  ir_ref if_hit = ir_IF(ir_EQ(gpa, pc));
  ir_IF_TRUE(if_hit);
  ir_TAILCALL_1(IR_VOID, next, state_ptr);
  ir_IF_FALSE(if_hit);
  ir_STORE(ir_ADD_OFFSET(state_ptr, offsetof(CPUState, pc)), pc);
  ir_RETURN(IR_UNUSED);
}

JitFunction IRJit::translate(const BBInfo &info) {
  ir_ctx ctx;

  ir_init(&ctx,
          IR_FUNCTION | IR_OPT_FOLDING | IR_OPT_CFG | IR_OPT_CODEGEN |
              IR_OPT_MEM2SSA,
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
