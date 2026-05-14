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

namespace prot::engine {
namespace {

using JitFunction = void (*)(CPUState &);

class AsmJit : public Translator {
public:
  AsmJit() = default;

private:
  [[nodiscard]] JitFunction translate(const BBInfo &info) override;

  asmjit::JitRuntime runtime;
};

void syscallHelper(CPUState &state) { state.emulateSysCall(); }

JitFunction AsmJit::translate(const BBInfo &info) {
  asmjit::CodeHolder code;
  code.init(runtime.environment());

  asmjit::x86::Compiler cc(&code);
  auto signature = asmjit::FuncSignature::build<void, CPUState *>();
  auto *func_node = cc.addFunc(signature);

  auto state_ptr = cc.newUIntPtr();
  func_node->setArg(0, state_ptr);

  auto mem_base = cc.newUInt64();
  cc.mov(mem_base,
         asmjit::x86::qword_ptr(state_ptr, offsetof(CPUState, mem_base)));

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

  auto guest_addr = cc.newGpd();
  auto host_addr = cc.newUInt64();

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

    case kLW: {
      loadReg(guest_addr, insn.rs1());
      cc.add(guest_addr, insn.imm());

      cc.mov(host_addr, mem_base);
      cc.add(host_addr, guest_addr.r64());

      cc.mov(rd, asmjit::x86::dword_ptr(host_addr));
      setDst(insn.rd(), rd);
      break;
    }

    case kLH: {
      loadReg(guest_addr, insn.rs1());
      cc.add(guest_addr, insn.imm());

      cc.mov(host_addr, mem_base);
      cc.add(host_addr, guest_addr.r64());

      cc.movsx(rd, asmjit::x86::word_ptr(host_addr));
      setDst(insn.rd(), rd);
      break;
    }

    case kLHU: {
      loadReg(guest_addr, insn.rs1());
      cc.add(guest_addr, insn.imm());

      cc.mov(host_addr, mem_base);
      cc.add(host_addr, guest_addr.r64());

      cc.movzx(rd, asmjit::x86::word_ptr(host_addr));
      setDst(insn.rd(), rd);
      break;
    }

    case kLB: {
      loadReg(guest_addr, insn.rs1());
      cc.add(guest_addr, insn.imm());

      cc.mov(host_addr, mem_base);
      cc.add(host_addr, guest_addr.r64());

      cc.movsx(rd, asmjit::x86::byte_ptr(host_addr));
      setDst(insn.rd(), rd);
      break;
    }

    case kLBU: {
      loadReg(guest_addr, insn.rs1());
      cc.add(guest_addr, insn.imm());

      cc.mov(host_addr, mem_base);
      cc.add(host_addr, guest_addr.r64());

      cc.movzx(rd, asmjit::x86::byte_ptr(host_addr));
      setDst(insn.rd(), rd);
      break;
    }

    case kSW: {
      loadReg(guest_addr, insn.rs1());
      cc.add(guest_addr, insn.imm());
      loadReg(rd, insn.rs2());

      cc.mov(host_addr, mem_base);
      cc.add(host_addr, guest_addr.r64());

      cc.mov(asmjit::x86::dword_ptr(host_addr), rd);
      break;
    }

    case kSH: {
      loadReg(guest_addr, insn.rs1());
      cc.add(guest_addr, insn.imm());
      loadReg(rd, insn.rs2());

      cc.mov(host_addr, mem_base);
      cc.add(host_addr, guest_addr.r64());

      cc.mov(asmjit::x86::word_ptr(host_addr), rd.r16());
      break;
    }

    case kSB: {
      loadReg(guest_addr, insn.rs1());
      cc.add(guest_addr, insn.imm());
      loadReg(rd, insn.rs2());

      cc.mov(host_addr, mem_base);
      cc.add(host_addr, guest_addr.r64());

      cc.mov(asmjit::x86::byte_ptr(host_addr), rd.r8());
      break;
    }

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

std::unique_ptr<Translator> makeAsmJit() { return std::make_unique<AsmJit>(); }
} // namespace prot::engine
