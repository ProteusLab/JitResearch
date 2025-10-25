#include <asmjit/asmjit.h>

#include "prot/jit/base.hh"

#include <cstdint>
#include <functional>

#include <cassert>
#include <span>

#include <sys/mman.h>

#include <fmt/core.h>
#include <iostream>

#define PROT_ASMJIT_R_OP(OP, ASMJIT_OP)                                        \
  case k##OP: {                                                                \
    loadReg(rs1, insn.rs1());                                                  \
    loadReg(rs2, insn.rs2());                                                  \
    cc.ASMJIT_OP(rs1, rs2);                                                    \
    setDst(insn.rd(), rs1);                                                    \
    break;                                                                     \
  }

#define PROT_ASMJIT_I_OP(OP, ASMJIT_OP)                                        \
  case k##OP##I: {                                                             \
    loadReg(rs1, insn.rs1());                                                  \
    cc.ASMJIT_OP(rs1, insn.imm());                                             \
    setDst(insn.rd(), rs1);                                                    \
    break;                                                                     \
  }

#define PROT_ASMJIT_ALU_OP(OP, ASMJIT_OP)                                      \
  PROT_ASMJIT_R_OP(OP, ASMJIT_OP)                                              \
  PROT_ASMJIT_I_OP(OP, ASMJIT_OP)

#define PROT_ASMJIT_I_SHIFT_OP(OP, ASMJIT_OP)                                  \
  case k##OP##I: {                                                             \
    loadReg(rs1, insn.rs1());                                                  \
    cc.ASMJIT_OP(rs1, insn.imm());                                             \
    setDst(insn.rd(), rs1);                                                    \
    break;                                                                     \
  }

#define PROT_ASMJIT_SHIFT_OP(OP, ASMJIT_OP)                                    \
  PROT_ASMJIT_R_OP(OP, ASMJIT_OP)                                              \
  PROT_ASMJIT_I_SHIFT_OP(OP, ASMJIT_OP)

#define PROT_ASMJIT_R_CMP_OP(OP, ASMJIT_OP)                                    \
  case k##OP: {                                                                \
    loadReg(rs1, insn.rs1());                                                  \
    loadReg(rs2, insn.rs2());                                                  \
    cc.cmp(rs2, rs1);                                                          \
    cc.ASMJIT_OP(rd);                                                          \
    cc.movzx(rd, rd.r8());                                                     \
    setDst(insn.rd(), rd);                                                     \
    break;                                                                     \
  }

#define PROT_ASMJIT_I_CMP_OP(OP, ASMJIT_OP)                                    \
  case k##OP: {                                                                \
    loadReg(rs1, insn.rs1());                                                  \
    cc.cmp(rs1, insn.imm());                                                   \
    cc.ASMJIT_OP(rd.r8());                                                     \
    cc.movzx(rd, rd.r8());                                                     \
    setDst(insn.rd(), rd);                                                     \
    break;                                                                     \
  }

#define PROT_ASMJIT_B_COND_OP(OP, COND)                                        \
  case k##OP: {                                                                \
    loadReg(rs1, insn.rs1());                                                  \
    loadReg(rs2, insn.rs2());                                                  \
    cc.cmp(rs1, rs2);                                                          \
    cc.mov(rs1, isa::kWordSize);                                               \
    cc.mov(rs2, insn.imm());                                                   \
    cc.cmov(COND, rs1, rs2);                                                   \
    cc.add(getPC(), rs1);                                                      \
    break;                                                                     \
  }

#define PROT_ASMJIT_S_OP(OP, DATA_TYPE)                                        \
  case k##OP: {                                                                \
    loadReg(rs1, insn.rs1());                                                  \
    cc.add(rs1, insn.imm());                                                   \
    loadReg(rs2, insn.rs2());                                                  \
    asmjit::InvokeNode *invoke{};                                              \
    cc.invoke(&invoke, reinterpret_cast<size_t>(storeHelper<DATA_TYPE>),       \
              asmjit::FuncSignature::build<void, CPUState &, isa::Addr,        \
                                           DATA_TYPE>());                      \
    invoke->setArg(0, state_ptr);                                              \
    invoke->setArg(1, rs1);                                                    \
    invoke->setArg(2, rs2);                                                    \
    break;                                                                     \
  }

#define PROT_ASMJIT_L_OP(OP, DATA_TYPE)                                        \
  case k##OP: {                                                                \
    loadReg(rs1, insn.rs1());                                                  \
    cc.add(rs1, insn.imm());                                                   \
    asmjit::InvokeNode *invoke = nullptr;                                      \
    cc.invoke(                                                                 \
        &invoke, reinterpret_cast<size_t>(loadHelper<DATA_TYPE>),              \
        asmjit::FuncSignature::build<DATA_TYPE, CPUState &, isa::Addr>());     \
    invoke->setArg(0, state_ptr);                                              \
    invoke->setArg(1, rs1);                                                    \
    invoke->setRet(0, rd);                                                     \
    setDst(insn.rd(), rd);                                                     \
    break;                                                                     \
  }

namespace prot::engine {
namespace {

using JitFunction = void (*)(CPUState &);

class AsmJit : public JitEngine {
public:
  AsmJit() = default;

private:
  bool doJIT(CPUState &state) override {
    const auto pc = state.getPC();
    auto found = m_cacheTB.find(pc);
    if (found != m_cacheTB.end()) {
      found->second(state);
      return true;
    }

    const auto *bbInfo = getBBInfo(pc);
    if (bbInfo == nullptr) {
      // No such bb yet
      return false;
    }

    auto holder = translate(*bbInfo);

    auto [it, wasNew] = m_cacheTB.try_emplace(pc, std::move(holder));
    assert(wasNew);

    it->second(state);

    return true;
  }

  [[nodiscard]] JitFunction translate(const BBInfo &info);

  asmjit::JitRuntime runtime;
  std::unordered_map<isa::Addr, JitFunction> m_cacheTB;
};

template <typename T> void storeHelper(CPUState &state, isa::Addr addr, T val) {
  state.memory->write(addr, val);
}

template <typename T> T loadHelper(CPUState &state, isa::Addr addr) {
  return state.memory->read<T>(addr);
}

void syscallHelper(CPUState &state) { state.emulateSysCall(); }

JitFunction AsmJit::translate(const BBInfo &info) {
  asmjit::CodeHolder code;
  code.init(runtime.environment());

  asmjit::x86::Compiler cc(&code);
  auto signature = asmjit::FuncSignature::build<void, CPUState *>();
  auto *func_node = cc.addFunc(signature);

  auto state_ptr = cc.newUIntPtr();
  func_node->setArg(0, state_ptr);

  auto getReg = [&state_ptr](auto regId) {
    return asmjit::x86::dword_ptr(state_ptr, offsetof(CPUState, regs) +
                                                 isa::kWordSize * regId);
  };

  auto loadReg = [&cc, getReg](auto reg, auto regId) {
    if (regId == 0)
      cc.mov(reg, 0);
    else
      cc.mov(reg, getReg(regId));
  };

  auto getPC = [&state_ptr]() {
    return asmjit::x86::dword_ptr(state_ptr, offsetof(CPUState, pc));
  };

  auto setDst = [&cc, getReg](auto dstId, auto dst) {
    if (dstId != 0)
      cc.mov(getReg(dstId), dst);
  };

  auto pc = cc.newGpd();
  auto rs1 = cc.newGpd();
  auto rs2 = cc.newGpd();
  auto rd = cc.newGpd();

  for (const auto &insn : info.insns) {
    switch (insn.opcode()) {
      using enum isa::Opcode;
      using enum asmjit::x86::CondCode;
      PROT_ASMJIT_ALU_OP(ADD, add)
      PROT_ASMJIT_ALU_OP(AND, and_)
      PROT_ASMJIT_ALU_OP(OR, or_)
      PROT_ASMJIT_ALU_OP(XOR, xor_)

      PROT_ASMJIT_SHIFT_OP(SLL, shl)
      PROT_ASMJIT_SHIFT_OP(SRA, sar)
      PROT_ASMJIT_SHIFT_OP(SRL, shr)

      PROT_ASMJIT_R_OP(SUB, sub)

      PROT_ASMJIT_R_CMP_OP(SLT, setg)
      PROT_ASMJIT_R_CMP_OP(SLTU, seta)

      PROT_ASMJIT_I_CMP_OP(SLTI, setl)
      PROT_ASMJIT_I_CMP_OP(SLTIU, setb)

      PROT_ASMJIT_B_COND_OP(BEQ, kEqual)
      PROT_ASMJIT_B_COND_OP(BNE, kNotEqual)
      PROT_ASMJIT_B_COND_OP(BLT, kSignedLT)
      PROT_ASMJIT_B_COND_OP(BGE, kSignedGE)
      PROT_ASMJIT_B_COND_OP(BLTU, kUnsignedLT)
      PROT_ASMJIT_B_COND_OP(BGEU, kUnsignedGE)

      PROT_ASMJIT_L_OP(LB, prot::isa::Byte)
      PROT_ASMJIT_L_OP(LH, prot::isa::Half)
      PROT_ASMJIT_L_OP(LBU, prot::isa::Byte)
      PROT_ASMJIT_L_OP(LHU, prot::isa::Half)
      PROT_ASMJIT_L_OP(LW, prot::isa::Word)

      PROT_ASMJIT_S_OP(SB, prot::isa::Byte)
      PROT_ASMJIT_S_OP(SH, prot::isa::Half)
      PROT_ASMJIT_S_OP(SW, prot::isa::Word)

    // PROT_ASMJIT_J_OP
    case kJAL: {
      cc.mov(rd, getPC());
      cc.add(rd, isa::kWordSize);
      setDst(insn.rd(), rd);
      cc.mov(pc, getPC());
      cc.add(pc, insn.imm());
      cc.mov(getPC(), pc);
      break;
    }
    case kJALR: {
      cc.mov(rd, getPC());
      cc.add(rd, isa::kWordSize);

      loadReg(pc, insn.rs1());
      cc.add(pc, insn.imm());
      cc.and_(pc, ~0b1);

      setDst(insn.rd(), rd);

      cc.mov(getPC(), pc);
      break;
    }

    // PROT_ASMJIT_U_OP
    case kLUI: {
      cc.mov(rs1, insn.imm());
      setDst(insn.rd(), rs1);
      break;
    }

    case kAUIPC: {
      cc.mov(rs1, getPC());
      cc.add(rs1, insn.imm());
      setDst(insn.rd(), rs1);
      break;
    }

    case kECALL: {
      asmjit::InvokeNode *invoke{};
      cc.invoke(&invoke, reinterpret_cast<size_t>(syscallHelper),
                asmjit::FuncSignature::build<void, CPUState &>());
      invoke->setArg(0, state_ptr);
      break;
    }

    case kFENCE:
    case kEBREAK:
    case kPAUSE:
    case kSBREAK:
    case kSCALL: {
      break;
    }

    case kNumOpcodes:
      throw std::invalid_argument{"Unexpected insn id"};
    }

    if (!isa::changesPC(insn.opcode())) {
      cc.mov(pc, getPC());
      cc.add(pc, isa::kWordSize);
      cc.mov(getPC(), pc);
    }
  }
  cc.mov(rd, info.insns.size());
  cc.add(asmjit::x86::dword_ptr(state_ptr, offsetof(CPUState, icount)), rd);
  cc.endFunc();
  cc.finalize();

  JitFunction func;
  auto err = runtime.add(&func, &code);
  if (err)
    throw std::runtime_error{"Failed to generate code"};

  return func;
}
} // namespace

std::unique_ptr<ExecEngine> makeAsmJit() { return std::make_unique<AsmJit>(); }
} // namespace prot::engine
