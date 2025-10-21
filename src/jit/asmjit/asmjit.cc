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
    cc.mov(rs1, getReg(insn.rs1()));                                           \
    cc.mov(rs2, getReg(insn.rs2()));                                           \
    cc.ASMJIT_OP(rs1, rs2);                                                    \
    if (insn.rd() != 0)                                                        \
      cc.mov(getReg(insn.rd()), rs1);                                          \
    break;                                                                     \
  }

#define PROT_ASMJIT_I_OP(OP, ASMJIT_OP)                                        \
  case k##OP##I: {                                                             \
    cc.mov(rs1, getReg(insn.rs1()));                                           \
    cc.ASMJIT_OP(rs1, insn.imm());                                             \
    if (insn.rd() != 0)                                                        \
      cc.mov(getReg(insn.rd()), rs1);                                          \
    break;                                                                     \
  }

#define PROT_ASMJIT_ALU_OP(OP, ASMJIT_OP)                                      \
  PROT_ASMJIT_R_OP(OP, ASMJIT_OP)                                              \
  PROT_ASMJIT_I_OP(OP, ASMJIT_OP)

#define PROT_ASMJIT_R_CMP_OP(OP, ASMJIT_OP)                                    \
  case k##OP: {                                                                \
    cc.mov(rs1, getReg(insn.rs1()));                                           \
    cc.mov(rs2, getReg(insn.rs2()));                                           \
    cc.cmp(rs2, rs1);                                                          \
    cc.ASMJIT_OP(rd);                                                          \
    cc.movzx(rd, rd.r8());                                                     \
    if (insn.rd() != 0)                                                        \
      cc.mov(getReg(insn.rd()), rd);                                           \
    break;                                                                     \
  }

#define PROT_ASMJIT_I_CMP_OP(OP, ASMJIT_OP)                                    \
  case k##OP: {                                                                \
    cc.mov(rs1, getReg(insn.rs1()));                                           \
    cc.cmp(rs1, insn.imm());                                                   \
    cc.ASMJIT_OP(rd);                                                          \
    cc.movzx(rd, rd.r8());                                                     \
    if (insn.rd() != 0)                                                        \
      cc.mov(getReg(insn.rd()), rd);                                           \
    break;                                                                     \
  }

#define PROT_ASMJIT_B_COND_OP(OP, ASMJIT_OP)                                   \
  case k##OP: {                                                                \
    asmjit::Label l_end = cc.newLabel();                                       \
    asmjit::Label lf = cc.newLabel();                                          \
    cc.mov(rs1, getReg(insn.rs1()));                                           \
    cc.mov(rs2, getReg(insn.rs2()));                                           \
    cc.cmp(rs1, rs2);                                                          \
    cc.ASMJIT_OP(lf);                                                          \
    cc.mov(pc, getPC());                                                       \
    cc.add(pc, insn.imm());                                                    \
    cc.jmp(l_end);                                                             \
    cc.bind(lf);                                                               \
    cc.mov(pc, getPC());                                                       \
    cc.add(pc, sizeof(isa::Word));                                             \
    cc.bind(l_end);                                                            \
    cc.mov(getPC(), pc);                                                       \
    break;                                                                     \
  }

#define ANOTHER_PROT_ASMJIT_B_COND_OP(OP, COND)                                \
  case k##OP: {                                                                \
    cc.mov(rs1, getReg(insn.rs1()));                                           \
    cc.mov(rs2, getReg(insn.rs2()));                                           \
    cc.mov(pc, getPC());                                                       \
    cc.mov(rd, pc);                                                            \
    cc.add(rd, sizeof(isa::Word));                                             \
    cc.add(pc, insn.imm());                                                    \
    cc.cmp(rs1, rs2);                                                          \
    cc.cmov(COND, pc, rd);                                                     \
    cc.mov(getPC(), pc);                                                       \
    break;                                                                     \
  }

#define PROT_ASMJIT_S_OP(OP, DATA_TYPE)                                        \
  case k##OP: {                                                                \
    cc.mov(rs1, getReg(insn.rs1()));                                           \
    cc.add(rs1, insn.imm());                                                   \
    cc.mov(rs2, getReg(insn.rs2()));                                           \
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
    cc.mov(rs1, getReg(insn.rs1()));                                           \
    cc.add(rs1, insn.imm());                                                   \
    asmjit::InvokeNode *invoke = nullptr;                                      \
    cc.invoke(&invoke, reinterpret_cast<size_t>(loadHelper<DATA_TYPE>),        \
              asmjit::FuncSignatureT<DATA_TYPE, CPUState &, isa::Addr>());     \
    invoke->setArg(0, &state);                                                 \
    invoke->setArg(1, rs1);                                                    \
    invoke->setRet(0, rd);                                                     \
    if (insn.rd() != 0) {                                                      \
      cc.shl(rd, (sizeof(isa::Addr) - sizeof(DATA_TYPE)) * CHAR_BIT);          \
      cc.sar(rd, (sizeof(isa::Addr) - sizeof(DATA_TYPE)) * CHAR_BIT);          \
      cc.mov(getReg(insn.rd()), rd);                                           \
    }                                                                          \
    break;                                                                     \
  }

#define PROT_ASMJIT_LU_OP(OP, DATA_TYPE)                                       \
  case k##OP: {                                                                \
    cc.mov(rs1, getReg(insn.rs1()));                                           \
    cc.add(rs1, insn.imm());                                                   \
    asmjit::InvokeNode *invoke = nullptr;                                      \
    cc.invoke(&invoke, reinterpret_cast<size_t>(loadHelper<DATA_TYPE>),        \
              asmjit::FuncSignatureT<DATA_TYPE, CPUState &, isa::Addr>());     \
    invoke->setArg(0, &state);                                                 \
    invoke->setArg(1, rs1);                                                    \
    invoke->setRet(0, rd);                                                     \
    if (insn.rd() != 0)                                                        \
      cc.mov(getReg(insn.rd()), rd);                                           \
                                                                               \
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

    m_state_ptr = &state;
    auto holder = translate(state, *bbInfo);

    auto [it, wasNew] = m_cacheTB.try_emplace(pc, holder);
    assert(wasNew);

    it->second(state);

    return true;
  }

  [[nodiscard]] JitFunction translate(CPUState &state, const BBInfo &info);

  asmjit::JitRuntime runtime;
  std::unordered_map<isa::Addr, JitFunction> m_cacheTB;
  std::uint32_t test_count = 0;
  CPUState *m_state_ptr{nullptr};
};

template <typename T> void storeHelper(CPUState &state, isa::Addr addr, T val) {
  state.memory->write(addr, val);
}

template <typename T> T loadHelper(CPUState &state, isa::Addr addr) {
  return state.memory->read<T>(addr);
}

void syscallHelper(CPUState &state) { state.emulateSysCall(); }

JitFunction AsmJit::translate(CPUState &state, const BBInfo &info) {
  asmjit::CodeHolder code;
  // asmjit::FileLogger logger(stdout);
  // code.setLogger(&logger);
  code.init(runtime.environment());

  asmjit::x86::Compiler cc(&code);

  asmjit::FuncSignature signature =
      asmjit::FuncSignature::build<void, CPUState *>();
  asmjit::FuncNode *func_node = cc.addFunc(signature);

  auto *state_ptr = &state;
  // cc.mov(state_ptr, asmjit::x86::dword_ptr((size_t)(&state)));
  // func_node->setArg(0, state_ptr);

  constexpr isa::Word regs_offset = offsetof(CPUState, regs);
  constexpr isa::Word pc_offset = offsetof(CPUState, pc);

  auto getReg = [&state_ptr](std::size_t regId) {
    return asmjit::x86::dword_ptr(reinterpret_cast<size_t>(state_ptr) +
                                  regs_offset + sizeof(isa::Word) * regId);
    // так есть segfault
    // return asmjit::x86::dword_ptr(
    //     (size_t)(&state.regs[regId])); // так программа завершается корректно
  };

  auto getPC = [&state_ptr]() {
    return asmjit::x86::dword_ptr(reinterpret_cast<size_t>(state_ptr) +
                                  pc_offset); // так есть segfault
    // return asmjit::x86::dword_ptr(
    //     (size_t)(&state.pc)); // так программа завершается корректно
  };

  auto pc = cc.newGpd();
  auto rs1 = cc.newGpd();
  auto rs2 = cc.newGpd();
  auto rd = cc.newGpd();

  for (const auto &insn : info.insns) {
    switch (insn.opcode()) {
      using enum isa::Opcode;
      PROT_ASMJIT_ALU_OP(ADD, add)
      PROT_ASMJIT_ALU_OP(AND, and_)
      PROT_ASMJIT_ALU_OP(OR, or_)
      PROT_ASMJIT_ALU_OP(XOR, xor_)

      PROT_ASMJIT_ALU_OP(SLL, shl)
      PROT_ASMJIT_ALU_OP(SRA, sar)
      PROT_ASMJIT_ALU_OP(SRL, shr)

      PROT_ASMJIT_R_OP(SUB, sub)

      PROT_ASMJIT_R_CMP_OP(SLT, setg)
      PROT_ASMJIT_R_CMP_OP(SLTU, seta)

      PROT_ASMJIT_I_CMP_OP(SLTI, setg)
      PROT_ASMJIT_I_CMP_OP(SLTIU, seta)

      // ANOTHER_PROT_ASMJIT_B_COND_OP(BEQ, asmjit::x86::CondCode::kEqual)
      // ANOTHER_PROT_ASMJIT_B_COND_OP(BNE, asmjit::x86::CondCode::kNotEqual)
      // ANOTHER_PROT_ASMJIT_B_COND_OP(BLT, asmjit::x86::CondCode::kSignedLT)
      // ANOTHER_PROT_ASMJIT_B_COND_OP(BGE, asmjit::x86::CondCode::kSignedGE)
      // ANOTHER_PROT_ASMJIT_B_COND_OP(BLTU, asmjit::x86::CondCode::kUnsignedLT)
      // ANOTHER_PROT_ASMJIT_B_COND_OP(BGEU, asmjit::x86::CondCode::kUnsignedGE)

      PROT_ASMJIT_B_COND_OP(BEQ, jne)
      PROT_ASMJIT_B_COND_OP(BNE, je)
      PROT_ASMJIT_B_COND_OP(BGE, jl)
      PROT_ASMJIT_B_COND_OP(BGEU, jb)
      PROT_ASMJIT_B_COND_OP(BLT, jge)
      PROT_ASMJIT_B_COND_OP(BLTU, jae)

      PROT_ASMJIT_L_OP(LB, prot::isa::Byte)
      PROT_ASMJIT_L_OP(LH, prot::isa::Half)
      PROT_ASMJIT_LU_OP(LBU, prot::isa::Byte)
      PROT_ASMJIT_LU_OP(LHU, prot::isa::Half)

      PROT_ASMJIT_L_OP(LW, prot::isa::Word)

      PROT_ASMJIT_S_OP(SB, prot::isa::Byte)
      PROT_ASMJIT_S_OP(SH, prot::isa::Half)
      PROT_ASMJIT_S_OP(SW, prot::isa::Word)

    // PROT_ASMJIT_J_OP
    case kJAL: {
      cc.mov(rd, getPC());
      cc.add(rd, sizeof(isa::Word));

      if (insn.rd() != 0)
        cc.mov(getReg(insn.rd()), rd);

      cc.mov(pc, getPC());
      cc.add(pc, insn.imm());
      cc.mov(getPC(), pc);
      break;
    }
    case kJALR: {
      cc.mov(rd, getPC());
      cc.add(rd, sizeof(isa::Word));

      cc.mov(pc, getReg(insn.rs1()));
      cc.add(pc, insn.imm());
      cc.and_(pc, ~0b1);

      if (insn.rd() != 0)
        cc.mov(getReg(insn.rd()), rd);

      cc.mov(getPC(), pc);
      break;
    }

    // PROT_ASMJIT_U_OP
    case kLUI: {
      cc.mov(rs1, insn.imm());
      if (insn.rd() != 0)
        cc.mov(getReg(insn.rd()), rs1);
      break;
    }

    case kAUIPC: {
      cc.mov(rs1, getPC());
      cc.add(rs1, insn.imm());
      if (insn.rd() != 0)
        cc.mov(getReg(insn.rd()), rs1);
      break;
    }

    case kECALL: {
      asmjit::InvokeNode *invoke{};
      cc.invoke(&invoke, reinterpret_cast<size_t>(syscallHelper),
                asmjit::FuncSignature::build<void, CPUState &>());
      invoke->setArg(0, state_ptr);
      break;

      // auto helper = reinterpret_cast<void (*)(CPUState *)>(&syscallHelper);
      // cc.push(state_ptr);
      // cc.mov(asmjit::x86::rax, reinterpret_cast<uintptr_t>(helper));
      // cc.call(asmjit::x86::rax);
      // cc.pop(state_ptr);
      // break;
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
      cc.add(pc, sizeof(isa::Word));
      cc.mov(getPC(), pc);
    }
  }

  cc.endFunc();
  cc.finalize();

  JitFunction func;
  auto err = runtime.add(&func, &code);
  if (err) {
    throw std::runtime_error{"Failed to generate code"};
  }

  return func;
}
} // namespace

std::unique_ptr<ExecEngine> makeAsmJit() { return std::make_unique<AsmJit>(); }
} // namespace prot::engine
