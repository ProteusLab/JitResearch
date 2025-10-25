#include "prot/jit/lightning.hh"
#include "prot/jit/base.hh"

#include <cassert>
#include <functional>

#include <fmt/core.h>

extern "C" {
#include <sys/mman.h>

#include <lightning.h>
}

namespace prot::engine {

namespace {
struct Lightning : public JitEngine {
  Lightning() { init_jit("JIT Research"); }

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

    auto code = translate(*bbInfo);
    auto [it, wasNew] = m_cacheTB.emplace(pc, std::move(code));
    assert(wasNew);

    it->second(state);

    return true;
  }

  [[nodiscard]] CodeHolder translate(const BBInfo &info);

  ~Lightning() override { finish_jit(); }

private:
  std::unordered_map<isa::Addr, CodeHolder> m_cacheTB;
};

void storeHelper(CPUState &state, isa::Addr addr,
                 std::unsigned_integral auto val) {
  state.memory->write(addr, val);
}

template <typename T> T loadHelper(CPUState &state, isa::Addr addr) {
  return state.memory->read<T>(addr);
}

void syscallHelper(CPUState &state) { state.emulateSysCall(); }

class JITStateHolder final {
public:
  JITStateHolder() : m_ptr(jit_new_state()) {}
  [[nodiscard]] jit_state_t *get() const { return m_ptr.get(); }

  [[nodiscard]] auto emit() const && {
    auto *_jit = get();
    jit_emit();
    jit_word_t size{};
    auto *code = static_cast<std::byte *>(jit_get_code(&size));
    assert(code);
    return CodeHolder{{code, static_cast<std::size_t>(size)}};
  }

private:
  struct Deleter {
    Deleter() noexcept = default;
    void operator()(jit_state_t *_jit) {
      jit_clear_state();
      jit_destroy_state();
    }
  };

  std::unique_ptr<jit_state_t, Deleter> m_ptr;
};

CodeHolder Lightning::translate(const BBInfo &info) {
  JITStateHolder holder;
  jit_state_t *_jit = holder.get();

  jit_prolog();
  jit_node_t *in{jit_arg()}; // CPU_STATE

  // Put cpu state to V0
  jit_getarg(JIT_V0, in);

  using enum isa::Opcode;
  auto getRegOff = [](std::size_t rid) constexpr {
    return offsetof(CPUState, regs) +
           (sizeof(decltype(CPUState::regs)::value_type) * rid);
  };

  auto loadReg = [&](std::size_t rid, int reg, bool sign = false) {
    assert(reg < JIT_R_NUM);
    if (rid == 0) {
      // simply put 0 to R1
      jit_movi(JIT_R(reg), 0);
      return;
    }
    if (sign) {
      jit_ldxi_i(JIT_R(reg), JIT_V0, getRegOff(rid));
    } else {
      jit_ldxi_ui(JIT_R(reg), JIT_V0, getRegOff(rid));
    }
  };

  auto storeReg = [&](std::size_t rid, int reg) {
    if (rid == 0) {
      return;
    }
    assert(reg < JIT_R_NUM);
    jit_stxi_i(getRegOff(rid), JIT_V0, JIT_R(reg));
  };

  auto loadPC = [&](int reg) {
    assert(reg < JIT_R_NUM);
    jit_ldxi_ui(JIT_R(reg), JIT_V0, offsetof(CPUState, pc));
  };
  auto storePC = [&](int reg) {
    assert(reg < JIT_R_NUM);
    jit_stxi_i(offsetof(CPUState, pc), JIT_V0, JIT_R(reg));
  };

  for (const auto &insn : info.insns) {
    // jit_note(insn.mnemonic().data(), i++);
    std::make_unsigned_t<jit_word_t> sextImm = insn.imm();
    sextImm = isa::signExtend<sizeofBits<decltype(sextImm)>(),
                              sizeofBits<isa::Word>()>(sextImm);

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
    case kSRA:
      loadRS1(0, true);
      loadRS2(1);
      jit_rshr(JIT_R0, JIT_R0, JIT_R1);
      storeRd(0);
      break;
    case kSRAI:
      loadRS1(0, true);
      jit_rshi(JIT_R0, JIT_R0, insn.imm());
      storeRd(0);
      break;

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
    loadRS1(0, true);                                                          \
    loadRS2(1, true);                                                          \
    jit_##cond(JIT_R0, JIT_R0, JIT_R1);                                        \
    jit_movi(JIT_R1, sizeof(isa::Word));                                       \
    jit_movi(JIT_R2, insn.imm());                                              \
    jit_movzr(JIT_R2, JIT_R1, JIT_R0);                                         \
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
    case kFENCE:
      break;
    case kECALL:
      jit_prepare();
      jit_pushargr(JIT_V0);
      jit_finishi(reinterpret_cast<void *>(&syscallHelper));
      break;

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
    case kLUI:
      jit_movi(JIT_R0, insn.imm());
      storeRd(0);
      break;
    case kPAUSE:
      break;

#define PROT_MAKE_IMPL(OP, type, sext)                                         \
  case k##OP:                                                                  \
    loadRS1(0);                                                                \
    jit_addi(JIT_R0, JIT_R0, insn.imm());                                      \
    jit_prepare();                                                             \
    jit_pushargr(JIT_V0);                                                      \
    jit_pushargr(JIT_R0);                                                      \
                                                                               \
    jit_finishi(reinterpret_cast<void *>(&loadHelper<isa::type>));             \
    jit_retval(JIT_R0);                                                        \
    if (sext) {                                                                \
      jit_extr(JIT_R0, JIT_R0, 0, sizeofBits<isa::type>());                    \
    }                                                                          \
    storeRd(0);                                                                \
    break;

      PROT_MAKE_IMPL(LB, Byte, true);
      PROT_MAKE_IMPL(LBU, Byte, false);
      PROT_MAKE_IMPL(LH, Half, true);
      PROT_MAKE_IMPL(LHU, Half, false);
      PROT_MAKE_IMPL(LW, Word, false);
#undef PROT_MAKE_IMPL

#define PROT_MAKE_IMPL(OP, type)                                               \
  case k##OP:                                                                  \
    loadRS1(0);                                                                \
    jit_addi(JIT_R0, JIT_R0, insn.imm());                                      \
    loadRS2(1);                                                                \
    jit_prepare();                                                             \
    jit_pushargr(JIT_V0);                                                      \
    jit_pushargr(JIT_R0);                                                      \
    jit_pushargr(JIT_R1);                                                      \
    jit_finishi(reinterpret_cast<void *>(&storeHelper<isa::type>));            \
    break;

      PROT_MAKE_IMPL(SB, Byte);
      PROT_MAKE_IMPL(SH, Half);
      PROT_MAKE_IMPL(SW, Word);

    case kSBREAK:
    case kSCALL:
      break;

#undef PROT_MAKE_IMPL

    case kSLT:
      loadRS1(0, true);
      loadRS2(1, true);
      jit_ltr(JIT_R0, JIT_R0, JIT_R1);
      storeRd(0);
      break;
    case kSLTU:
      loadRS1(0);
      loadRS2(1);
      jit_ltr(JIT_R0, JIT_R0, JIT_R1);
      storeRd(0);
      break;

    case kSLTI:
      loadRS1(0, true);
      jit_movi(JIT_R1, sextImm);
      jit_ltr(JIT_R0, JIT_R0, JIT_R1);
      storeRd(0);
      break;
    case kSLTIU:
      loadRS1(0);
      jit_lti_u(JIT_R0, JIT_R0, insn.imm());
      storeRd(0);
      break;

    case kSUB:
      loadRS1(0);
      loadRS2(1);
      jit_subr(JIT_R0, JIT_R0, JIT_R1);
      storeRd(0);
      break;
    case kNumOpcodes:
      break;
    }

    if (!isa::changesPC(insn.opcode())) {
      loadPC(0);
      jit_addi(JIT_R0, JIT_R0, sizeof(isa::Word));
      storePC(0);
    }
  }
  // update icount
  jit_ldxi_ui(JIT_R0, JIT_V0, offsetof(CPUState, icount));
  jit_addi(JIT_R0, JIT_R0, info.insns.size());
  jit_stxi_i(offsetof(CPUState, icount), JIT_V0, JIT_R0);
  jit_epilog();

  auto code = std::move(holder).emit();

  // fmt::println("CODE!!");
  // jit_disassemble();
  // fmt::println("CODE END");

  return code;
}
} // namespace

std::unique_ptr<ExecEngine> makeLightning() {
  return std::make_unique<Lightning>();
}
} // namespace prot::engine
