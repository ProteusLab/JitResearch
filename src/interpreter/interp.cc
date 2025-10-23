#include "prot/interpreter.hh"

#include <cassert>
#include <concepts>
#include <functional>

namespace prot::engine {
namespace {
template <std::regular_invocable<isa::Word, isa::Word> Func>
void executeRegisterRegisterOp(const isa::Instruction &inst, CPUState &state,
                               Func op) {
  auto rs1 = state.getReg(inst.rs1());
  auto rs2 = state.getReg(inst.rs2());
  state.setReg(inst.rd(), op(rs1, rs2));
}

template <std::regular_invocable<isa::Word, isa::Operand> Func>
void executeRegisterImmOp(const isa::Instruction &inst, CPUState &state,
                          Func op) {
  auto rs1 = state.getReg(inst.rs1());
  auto imm = inst.imm();
  state.setReg(inst.rd(), op(rs1, imm));
}

template <std::regular_invocable<isa::Word, isa::Operand> Func>
void executeRegisterImmRegOp(const isa::Instruction &inst, CPUState &state,
                             Func op) {
  auto rs1 = state.getReg(inst.rs1());
  auto imm = inst.rs2();
  state.setReg(inst.rd(), op(rs1, imm));
}

void doADD(const isa::Instruction &inst, CPUState &state) {
  executeRegisterRegisterOp(inst, state, std::plus<>{});
}

void doADDI(const isa::Instruction &inst, CPUState &state) {
  executeRegisterImmOp(inst, state, std::plus<>{});
}

void doAND(const isa::Instruction &inst, CPUState &state) {
  executeRegisterRegisterOp(inst, state, std::bit_and<>{});
}
void doANDI(const isa::Instruction &inst, CPUState &state) {
  executeRegisterImmOp(inst, state, std::bit_and<>{});
}

void doAUIPC(const isa::Instruction &inst, CPUState &state) {
  state.setReg(inst.rd(), state.getPC() + inst.imm());
}

template <std::regular_invocable<isa::Word, isa::Word> Cond>
void brHelper(const isa::Instruction &inst, CPUState &state, Cond cond) {
  auto lhs = state.getReg(inst.rs1());
  auto rhs = state.getReg(inst.rs2());
  const isa::Addr offset = cond(lhs, rhs) ? inst.imm() : isa::kWordSize;

  state.setPC(state.getPC() + offset);
}

void doBEQ(const isa::Instruction &inst, CPUState &state) {
  brHelper(inst, state, std::equal_to<>{});
}
void doBGE(const isa::Instruction &inst, CPUState &state) {
  brHelper(inst, state, std::not_fn(&isa::signedLess));
}
void doBGEU(const isa::Instruction &inst, CPUState &state) {
  brHelper(inst, state, std::greater_equal<>{});
}
void doBLT(const isa::Instruction &inst, CPUState &state) {
  brHelper(inst, state, isa::signedLess);
}
void doBLTU(const isa::Instruction &inst, CPUState &state) {
  brHelper(inst, state, std::less<>{});
}
void doBNE(const isa::Instruction &inst, CPUState &state) {
  brHelper(inst, state, std::not_equal_to<>{});
}

void doEBREAK(const isa::Instruction & /*unused*/, CPUState & /*unused*/) {}

void doECALL(const isa::Instruction & /*unused*/, CPUState &state) {
  state.emulateSysCall();
}

void doFENCE(const isa::Instruction & /*unused*/, CPUState & /*unused*/) {
  // do nothing
}

void doJAL(const isa::Instruction &inst, CPUState &state) {
  state.setReg(inst.rd(), state.getPC() + isa::kWordSize);

  state.setPC(state.getPC() + inst.imm());
}
void doJALR(const isa::Instruction &inst, CPUState &state) {
  auto retAddr = state.getPC() + isa::kWordSize;
  auto target = state.getReg(inst.rs1()) + inst.imm();

  target >>= 1;
  target <<= 1;
  state.setPC(target);

  state.setReg(inst.rd(), retAddr);
}

template <typename T, bool Signed = true>
void loadHelper(const isa::Instruction &inst, CPUState &state) {
  auto rs = state.getReg(inst.rs1());
  auto addr = rs + inst.imm();
  // NOLINTNEXTLINE
  isa::Word loaded = state.memory->read<T>(addr);

  if constexpr (Signed) {
    loaded = isa::signExtend<sizeofBits<isa::Word>(), sizeofBits<T>()>(loaded);
  }

  state.setReg(inst.rd(), loaded);
}

void doLB(const isa::Instruction &inst, CPUState &state) {
  loadHelper<isa::Byte>(inst, state);
}
void doLBU(const isa::Instruction &inst, CPUState &state) {
  loadHelper<isa::Byte, false>(inst, state);
}
void doLH(const isa::Instruction &inst, CPUState &state) {
  loadHelper<isa::Half>(inst, state);
}
void doLHU(const isa::Instruction &inst, CPUState &state) {
  loadHelper<isa::Half, false>(inst, state);
}
void doLW(const isa::Instruction &inst, CPUState &state) {
  loadHelper<isa::Word, false>(inst, state);
}

void doLUI(const isa::Instruction &inst, CPUState &state) {
  state.setReg(inst.rd(), inst.imm());
}

void doOR(const isa::Instruction &inst, CPUState &state) {
  executeRegisterRegisterOp(inst, state, std::bit_or<>{});
}
void doORI(const isa::Instruction &inst, CPUState &state) {
  executeRegisterImmOp(inst, state, std::bit_or<>{});
}

void doPAUSE(const isa::Instruction & /*unused*/, CPUState & /*unused*/) {
  // Do nothing
}

template <typename T>
void storeHelper(const isa::Instruction &inst, CPUState &state) {
  auto addr = state.getReg(inst.rs1()) + inst.imm();
  T val = state.getReg(inst.rs2());

  state.memory->write(addr, val);
}

void doSB(const isa::Instruction &inst, CPUState &state) {
  storeHelper<isa::Byte>(inst, state);
}
void doSH(const isa::Instruction &inst, CPUState &state) {
  storeHelper<isa::Half>(inst, state);
}
void doSW(const isa::Instruction &inst, CPUState &state) {
  storeHelper<isa::Word>(inst, state);
}

void doSBREAK(const isa::Instruction & /*unused*/, CPUState & /*unused*/) {}
void doSCALL(const isa::Instruction & /*unused*/, CPUState & /*unused*/) {}

isa::Word sllHelper(isa::Word lhs, isa::Word rhs) {
  return lhs << isa::slice<4, 0>(rhs);
}

void doSLL(const isa::Instruction &inst, CPUState &state) {
  executeRegisterRegisterOp(inst, state, sllHelper);
}
void doSLLI(const isa::Instruction &inst, CPUState &state) {
  executeRegisterImmRegOp(inst, state, sllHelper);
}

void doSLT(const isa::Instruction &inst, CPUState &state) {
  executeRegisterRegisterOp(inst, state, isa::signedLess);
}
void doSLTI(const isa::Instruction &inst, CPUState &state) {
  executeRegisterImmOp(inst, state, isa::signedLess);
}

void doSLTIU(const isa::Instruction &inst, CPUState &state) {
  executeRegisterImmOp(inst, state, std::less<>{});
}
void doSLTU(const isa::Instruction &inst, CPUState &state) {
  executeRegisterRegisterOp(inst, state, std::less<>{});
}

isa::Word sraHelper(isa::Word lhs, isa::Word rhs) {
  return static_cast<std::make_signed_t<isa::Word>>(lhs) >>
         isa::slice<4, 0>(rhs);
}

void doSRA(const isa::Instruction &inst, CPUState &state) {
  executeRegisterRegisterOp(inst, state, sraHelper);
}
void doSRAI(const isa::Instruction &inst, CPUState &state) {
  executeRegisterImmRegOp(inst, state, sraHelper);
}

isa::Word srlHelper(isa::Word lhs, isa::Word rhs) {
  return lhs >> isa::slice<4, 0>(rhs);
}

void doSRL(const isa::Instruction &inst, CPUState &state) {
  executeRegisterRegisterOp(inst, state, srlHelper);
}
void doSRLI(const isa::Instruction &inst, CPUState &state) {
  executeRegisterImmRegOp(inst, state, srlHelper);
}

void doSUB(const isa::Instruction &inst, CPUState &state) {
  executeRegisterRegisterOp(inst, state, std::minus<>{});
}

void doXOR(const isa::Instruction &inst, CPUState &state) {
  executeRegisterRegisterOp(inst, state, std::bit_xor<>{});
}
void doXORI(const isa::Instruction &inst, CPUState &state) {
  executeRegisterImmOp(inst, state, std::bit_xor<>{});
}

struct ExecHandlersMap final {
  using Handler = void (*)(const isa::Instruction &, CPUState &);

  constexpr ExecHandlersMap() {
    using enum isa::Opcode;
#define PROT_SET_HANDLER(name) m_handlers[toUnderlying(k##name)] = &do##name;
    PROT_SET_HANDLER(ADD)
    PROT_SET_HANDLER(ADDI)
    PROT_SET_HANDLER(AND)
    PROT_SET_HANDLER(ANDI)
    PROT_SET_HANDLER(AUIPC)
    PROT_SET_HANDLER(BEQ)
    PROT_SET_HANDLER(BGE)
    PROT_SET_HANDLER(BGEU)
    PROT_SET_HANDLER(BLT)
    PROT_SET_HANDLER(BLTU)
    PROT_SET_HANDLER(BNE)
    PROT_SET_HANDLER(EBREAK)
    PROT_SET_HANDLER(ECALL)
    PROT_SET_HANDLER(FENCE)
    PROT_SET_HANDLER(JAL)
    PROT_SET_HANDLER(JALR)
    PROT_SET_HANDLER(LB)
    PROT_SET_HANDLER(LBU)
    PROT_SET_HANDLER(LH)
    PROT_SET_HANDLER(LHU)
    PROT_SET_HANDLER(LUI)
    PROT_SET_HANDLER(LW)
    PROT_SET_HANDLER(OR)
    PROT_SET_HANDLER(ORI)
    PROT_SET_HANDLER(PAUSE)
    PROT_SET_HANDLER(SB)
    PROT_SET_HANDLER(SBREAK)
    PROT_SET_HANDLER(SCALL)
    PROT_SET_HANDLER(SH)
    PROT_SET_HANDLER(SLL)
    PROT_SET_HANDLER(SLLI)
    PROT_SET_HANDLER(SLT)
    PROT_SET_HANDLER(SLTI)
    PROT_SET_HANDLER(SLTIU)
    PROT_SET_HANDLER(SLTU)
    PROT_SET_HANDLER(SRA)
    PROT_SET_HANDLER(SRAI)
    PROT_SET_HANDLER(SRL)
    PROT_SET_HANDLER(SRLI)
    PROT_SET_HANDLER(SUB)
    PROT_SET_HANDLER(SW)
    PROT_SET_HANDLER(XOR)
    PROT_SET_HANDLER(XORI)
#undef PROT_SET_HANDLER
  }

  [[nodiscard]] Handler get(isa::Opcode opcode) const {
    assert(toUnderlying(opcode) < m_handlers.size());
    auto toRet = m_handlers[toUnderlying(opcode)];
    assert(toRet != nullptr);
    return toRet;
  }

private:
  std::array<Handler, toUnderlying(isa::Opcode::kNumOpcodes)> m_handlers{};
};

constexpr ExecHandlersMap kExecHandlers{};
} // namespace

void Interpreter::execute(CPUState &cpu, const isa::Instruction &insn) {
  const auto handler = kExecHandlers.get(insn.opcode());

  auto oldPC = cpu.getPC();

  handler(insn, cpu);

  if (!isa::changesPC(insn.opcode())) {
    cpu.setPC(oldPC + isa::kWordSize);
  }
}
} // namespace prot::engine
