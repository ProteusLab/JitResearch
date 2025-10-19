#include <asmjit/asmjit.h>

#include "prot/jit/base.hh"

#include <functional>

#include <cassert>
#include <span>

#include <sys/mman.h>

#include <fmt/core.h>

#define PROT_ASMJIT_R_OP(OP, ASMJIT_OP)                                        \
  case prot::isa::Opcode::k##OP: {                                             \
    cc.mov(rs1, getReg(insn.rs1()));                                           \
    cc.mov(rs2, getReg(insn.rs2()));                                           \
    cc.ASMJIT_OP(rs1, rs2);                                                    \
    if (insn.rd() != 0)                                                        \
      cc.mov(getReg(insn.rd()), rs1);                                          \
    break;                                                                     \
  }

#define PROT_ASMJIT_I_OP(OP, ASMJIT_OP)                                        \
  case prot::isa::Opcode::k##OP##I: {                                          \
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
  case prot::isa::Opcode::k##OP: {                                             \
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
  case prot::isa::Opcode::k##OP: {                                             \
    cc.mov(rs1, getReg(insn.rs1()));                                           \
    cc.cmp(rs1, insn.imm());                                                   \
    cc.ASMJIT_OP(rd);                                                          \
    cc.movzx(rd, rd.r8());                                                     \
    if (insn.rd() != 0)                                                        \
      cc.mov(getReg(insn.rd()), rd);                                           \
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

void storeHelper(CPUState &state, isa::Addr addr,
                 std::unsigned_integral auto val) {
  state.memory->write(addr, val);
}

void loadHelper(CPUState &state, isa::Addr addr,
                std::unsigned_integral auto val) {
  state.memory->read(addr, val);
}

JitFunction AsmJit::translate(const BBInfo &info) {
  asmjit::CodeHolder code;
  code.init(runtime.environment());

  asmjit::x86::Compiler cc(&code);

  cc.addFunc(asmjit::FuncSignature::build<void, CPUState *>());

  asmjit::x86::Gp state_ptr = cc.newUIntPtr();
  cc.setArg(0, state_ptr);

  constexpr size_t regs_offset = offsetof(CPUState, regs);
  constexpr size_t pc_offset = offsetof(CPUState, pc);

  auto getReg = [&state_ptr](std::size_t regId) {
    return asmjit::x86::dword_ptr(state_ptr,
                                  regs_offset + sizeof(isa::Word) * regId);
  };

  auto getPC = [&state_ptr]() {
    return asmjit::x86::dword_ptr(state_ptr, pc_offset);
  };

  auto pc = cc.newGpd();
  auto rs1 = cc.newGpd();
  auto rs2 = cc.newGpd();
  auto rd = cc.newGpd();

  for (const auto &insn : info.insns) {
    switch (insn.opcode()) {
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

    case prot::isa::Opcode::kLUI: {
      cc.mov(rs1, insn.imm());
      if (insn.rd() != 0)
        cc.mov(getReg(insn.rd()), rs1);
      break;
    }

    case prot::isa::Opcode::kAUIPC: {
      cc.mov(rs1, getPC());
      cc.add(rs1, insn.imm());
      if (insn.rd() != 0)
        cc.mov(getReg(insn.rd()), rs1);
      break;
    }
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
