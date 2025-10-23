#include "prot/jit/lightning.hh"
#include "prot/jit/base.hh"

#include <cassert>
#include <functional>
#include <lightning.h>

namespace prot::engine {

namespace {

using JitFunction = void (*)(CPUState *);
struct Lightning : public JitEngine {
  Lightning() {
    init_jit(nullptr);
    _jit = jit_new_state();
  }

  bool doJIT(CPUState &state) override {
    const auto pc = state.getPC();
    auto found = m_cacheTB.find(pc);
    if (found != m_cacheTB.end()) {
      found->second(&state);
      return true;
    }

    const auto *bbInfo = getBBInfo(pc);
    if (bbInfo == nullptr) {
      // No such bb yet
      return false;
    }

    JitFunction func = translate(*bbInfo);
    assert(func);
    auto [it, wasNew] = m_cacheTB.try_emplace(pc, func);
    assert(wasNew);

    func(&state);

    return true;
  }

  [[nodiscard]] JitFunction translate(const BBInfo &info);

  ~Lightning() override {
    jit_destroy_state();
    finish_jit();
  }

private:
  jit_state_t *_jit{}; // Naming is dictated by library...
  std::unordered_map<isa::Addr, JitFunction> m_cacheTB;
};

JitFunction Lightning::translate(const BBInfo &info) {
  jit_prolog();
  jit_node_t *in{jit_arg()}; // CPU_STATE

  // Put cpu state to V0
  jit_getarg(JIT_V0, in);

  using enum isa::Opcode;
  auto getRegOff = [](std::size_t rid) constexpr {
    return offsetof(CPUState, regs) +
           (sizeof(decltype(CPUState::regs)::value_type) * rid);
  };

  auto loadReg = [&](std::size_t rid, int reg) {
    assert(reg < JIT_R_NUM);
    if (rid == 0) {
      // simply put 0 to R1
      jit_movi(JIT_R(reg), 0);
      return;
    }
    jit_ldxr_ui(JIT_R(reg), JIT_V0, getRegOff(rid));
  };

  auto storeReg = [&](std::size_t rid, int reg) {
    if (rid == 0) {
      return;
    }
    assert(reg < JIT_R_NUM);
    jit_stxr_i(JIT_V0, getRegOff(rid), JIT_R(reg));
  };

  auto loadPC = [&](int reg) {
    assert(reg < JIT_R_NUM);
    jit_ldxr_ui(JIT_R(reg), JIT_V0, offsetof(CPUState, pc));
  };
  auto storePC = [&](int reg) {
    assert(reg < JIT_R_NUM);
    jit_stxr_i(JIT_V0, offsetof(CPUState, pc), JIT_R(reg));
  };

  for (const auto &insn : info.insns) {
    auto loadRS1 = std::bind_front(loadReg, insn.rs1());
    auto loadRS2 = std::bind_front(loadReg, insn.rs2());
    auto storeRd = std::bind_front(storeReg, insn.rd());

    switch (insn.opcode()) {
#define PROT_MAKE_IMPL(OP, op1, op2)                                           \
  case k##OP:                                                                  \
    loadRS1(0);                                                                \
    loadRS2(1);                                                                \
    jit_##op1(JIT_R0, JIT_R0, JIT_R1);                                         \
    storeRd(0);                                                                \
    break;                                                                     \
  case k##OP##I:                                                               \
    loadRS1(0);                                                                \
    jit_##op2(JIT_R0, JIT_R0, insn.imm());                                     \
    storeRd(0);                                                                \
    break;

#define PROT_MAKE_IMPL_SIMPLE(OP, op) PROT_MAKE_IMPL(OP, op##r, op##i)

      PROT_MAKE_IMPL_SIMPLE(ADD, add)
      PROT_MAKE_IMPL_SIMPLE(AND, and)
      PROT_MAKE_IMPL_SIMPLE(OR, or)
      PROT_MAKE_IMPL_SIMPLE(XOR, xor)
      PROT_MAKE_IMPL_SIMPLE(SLL, lsh)
      PROT_MAKE_IMPL_SIMPLE(SRA, rsh)
#undef PROT_MAKE_IMPL_SIMPLE
      PROT_MAKE_IMPL(SRL, rshr_u, rshi_u)

#undef PROT_MAKE_IMPL
    case kAUIPC:
      loadPC(0);
      jit_addi(JIT_R0, JIT_R0, insn.imm());
      storeRd(0);
      break;
#define PROT_MAKE_BR(OP, cond)                                                 \
  case kB##OP:                                                                 \
    jit_movi(JIT_R2, insn.imm());                                              \
    loadRS1(0);                                                                \
    loadRS2(1);                                                                \
    jit_##cond(JIT_R0, JIT_R0, JIT_R1);                                        \
    jit_movzr(JIT_R2, sizeof(isa::Word), JIT_R0);                              \
    loadPC(0);                                                                 \
    jit_addr(JIT_R0, JIT_R0, JIT_R2);                                          \
    storePC(0);                                                                \
    break;

      PROT_MAKE_BR(EQ, eqr)
      PROT_MAKE_BR(GE, ger)
      PROT_MAKE_BR(GEU, ger_u)
      PROT_MAKE_BR(LT, ltr)
      PROT_MAKE_BR(LTU, ltr_u)
      PROT_MAKE_BR(NE, ner)
#undef PROT_MAKE_BR

    case kEBREAK:
    case kECALL:
    case kFENCE:

    case kJAL:
      loadPC(0);
      jit_addi(JIT_R1, JIT_R0, sizeof(isa::Word));
      storeRd(1);
      jit_addi(JIT_R0, JIT_R0, insn.imm());
      storePC(0);
      break;
    case kJALR:
      loadPC(0);
      jit_addi(JIT_R0, JIT_R0, sizeof(isa::Word));
      loadRS1(1);
      storeRd(0);
      jit_addi(JIT_R1, JIT_R1, insn.imm());
      jit_andi(JIT_R1, JIT_R1, ~std::uint32_t{1});
      storePC(1);
      break;
    case kLB:
    case kLBU:
    case kLH:
    case kLHU:
    case kLUI:
    case kLW:
    case kPAUSE:
    case kSB:
    case kSBREAK:
    case kSCALL:
    case kSH:
    case kSLT:
    case kSLTI:
    case kSLTIU:
    case kSLTU:
    case kSUB:
      loadRS1(0);
      loadRS2(1);
      jit_subr(JIT_R0, JIT_R0, JIT_R1);
      storeRd(0);
      break;
    case kSW:
    case kNumOpcodes:
      break;
    }
  }
  auto func = reinterpret_cast<JitFunction>(jit_emit());

  jit_clear_state();

  return func;
}
} // namespace

std::unique_ptr<ExecEngine> makeLightning() {
  return std::make_unique<Lightning>();
}
} // namespace prot::engine
