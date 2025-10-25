#include "prot/jit/xbyak.hh"
#include "prot/jit/base.hh"

#include <functional>
#include <xbyak/xbyak.h>
#include <xbyak/xbyak_util.h>

#include <cassert>
#include <span>

#include <sys/mman.h>

#include <fmt/core.h>

namespace prot::engine {
namespace {
class XByakJit : public JitEngine, private Xbyak::CodeGenerator {
public:
  XByakJit()
      : Xbyak::CodeGenerator{Xbyak::DEFAULT_MAX_CODE_SIZE, Xbyak::AutoGrow} {}

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

  [[nodiscard]] CodeHolder translate(const BBInfo &info);
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

CodeHolder XByakJit::translate(const BBInfo &info) {
  reset(); // XByak specific (CodeGenerator is about a PAGE size!!, so reuse it)
  Xbyak::util::StackFrame frame{this, 3, 3 | Xbyak::util::UseRCX};

  [[maybe_unused]] auto temp1 = frame.t[0].cvt32();
  [[maybe_unused]] auto temp2 = frame.t[1].cvt32();
  [[maybe_unused]] auto temp3 = frame.t[2].cvt32();

  auto getReg = [&frame, this](std::size_t regId) {
    return dword[frame.p[0] + offsetof(CPUState, regs) +
                 isa::kWordSize * regId];
  };

  auto getPc = [&frame, this] {
    return dword[frame.p[0] + offsetof(CPUState, pc)];
  };

  for (const auto &insn : info.insns) {
    auto getRs1 = [&](Xbyak::Reg32 reg) { mov(reg, getReg(insn.rs1())); };
    auto getRs2 = [&](Xbyak::Reg32 reg) { mov(reg, getReg(insn.rs2())); };

    auto setRd = [&](Xbyak::Reg32 reg) {
      if (insn.rd() != 0)
        mov(getReg(insn.rd()), reg);
    };

    switch (insn.opcode()) {
      using enum isa::Opcode;
#define PROT_MAKE_IMPL(OP, op)                                                 \
  case k##OP: {                                                                \
    getRs1(temp1);                                                             \
    op(temp1, getReg(insn.rs2()));                                             \
    setRd(temp1);                                                              \
    break;                                                                     \
  }                                                                            \
  case k##OP##I: {                                                             \
    getRs1(temp1);                                                             \
    op(temp1, insn.imm());                                                     \
    setRd(temp1);                                                              \
    break;                                                                     \
  }

      PROT_MAKE_IMPL(ADD, add)
      PROT_MAKE_IMPL(AND, and_)
      PROT_MAKE_IMPL(OR, or_)
      PROT_MAKE_IMPL(XOR, xor_)

#undef PROT_MAKE_IMPL

    case kAUIPC: {
      mov(temp1, getPc());
      add(temp1, insn.imm());
      setRd(temp1);
      break;
    }

#define PROT_MAKE_IMPL(Op, cc)                                                 \
  case kB##Op: {                                                               \
    getRs1(temp1);                                                             \
    cmp(temp1, getReg(insn.rs2()));                                            \
    mov(temp1, isa::kWordSize);                                                \
    mov(temp2, insn.imm());                                                    \
    cmov##cc(temp1, temp2);                                                    \
                                                                               \
    add(getPc(), temp1);                                                       \
    break;                                                                     \
  }
      PROT_MAKE_IMPL(EQ, z)
      PROT_MAKE_IMPL(GE, ge)
      PROT_MAKE_IMPL(GEU, ae)
      PROT_MAKE_IMPL(LT, l)
      PROT_MAKE_IMPL(LTU, b)
      PROT_MAKE_IMPL(NE, ne)
#undef PROT_MAKE_IMPL

    case kEBREAK:
      break;
    case kECALL: {
      // set finished
      mov(frame.t[0], reinterpret_cast<std::uintptr_t>(&syscallHelper));
      push(frame.p[0]);
      call(frame.t[0]);
      pop(frame.p[0]);
      break;
    }
    case kFENCE: {
      break;
    }
    case kJAL: {
      mov(temp1, getPc());
      add(temp1, isa::kWordSize);
      setRd(temp1);

      add(getPc(), insn.imm());
      break;
    }
    case kJALR: {
      mov(temp1, getPc());
      add(temp1, isa::kWordSize);

      getRs1(temp2);
      add(temp2, insn.imm());
      and_(temp2, ~std::uint32_t{1});
      mov(getPc(), temp2);

      setRd(temp1);
      break;
    }
    case kLUI: {
      if (insn.rd() != 0) {
        mov(getReg(insn.rd()), insn.imm());
      }
      break;
    }
    case kLB:
    case kLBU:
    case kLH:
    case kLHU:
    case kLW: {
      auto addr = frame.p[1].cvt32();
      getRs1(addr);
      add(addr, insn.imm()); // calc addr

      const auto helper = [op = insn.opcode()] {
        switch (op) {
        case kLB:
        case kLBU:
          return reinterpret_cast<std::uintptr_t>(&loadHelper<isa::Byte>);
        case kLH:
        case kLHU:
          return reinterpret_cast<std::uintptr_t>(&loadHelper<isa::Half>);
        case kLW:
          return reinterpret_cast<std::uintptr_t>(&loadHelper<isa::Word>);
        default:
          return std::uintptr_t{};
        }
      }();

      push(frame.p[0]);
      mov(frame.t[0], helper);
      call(frame.t[0]);
      pop(frame.p[0]);
      switch (insn.opcode()) {
      case kLB:
        cbw();
        [[fallthrough]];
      case kLH:
        cwde();
        break;
      default:
        break;
      }

      setRd(eax);

      break;
    }
    case kPAUSE:
    case kSBREAK:
    case kSCALL: {
      break;
    }
#define PROT_MAKE_IMPL(Op, Op2, cc)                                            \
  case k##Op: {                                                                \
    getRs1(temp1);                                                             \
    cmp(temp1, getReg(insn.rs2()));                                            \
    mov(temp1, 0);                                                             \
    set##cc(temp1.cvt8());                                                     \
    setRd(temp1);                                                              \
    break;                                                                     \
  }                                                                            \
  case k##Op2: {                                                               \
    cmp(getReg(insn.rs1()), insn.imm());                                       \
    mov(temp1, 0);                                                             \
    set##cc(temp1.cvt8());                                                     \
    setRd(temp1);                                                              \
    break;                                                                     \
  }
      PROT_MAKE_IMPL(SLT, SLTI, l);
      PROT_MAKE_IMPL(SLTU, SLTIU, b);
#undef PROT_MAKE_IMPL

#define PROT_MAKE_IMPL(Op, op)                                                 \
  case kS##Op: {                                                               \
    getRs1(temp1);                                                             \
    getRs2(ecx);                                                               \
    op(temp1, cl);                                                             \
    setRd(temp1);                                                              \
    break;                                                                     \
  }                                                                            \
  case kS##Op##I: {                                                            \
    getRs1(temp1);                                                             \
    op(temp1, insn.imm());                                                     \
    setRd(temp1);                                                              \
    break;                                                                     \
  }

      PROT_MAKE_IMPL(LL, shl)
      PROT_MAKE_IMPL(RA, sar)
      PROT_MAKE_IMPL(RL, shr)
#undef PROT_MAKE_IMPL

    case kSUB: {
      getRs1(temp1);
      sub(temp1, getReg(insn.rs2()));
      setRd(temp1);
      break;
    }
    case kSB:
    case kSH:
    case kSW: {
      auto addr = frame.p[1].cvt32();
      getRs1(addr);
      add(addr, insn.imm()); // calc addr
      auto val = frame.p[2].cvt32();
      getRs2(val);

      const auto helper = [op = insn.opcode()] {
        switch (op) {
        case kSB:
          return reinterpret_cast<std::uintptr_t>(&storeHelper<isa::Byte>);
        case kSH:
          return reinterpret_cast<std::uintptr_t>(&storeHelper<isa::Half>);
        case kSW:
          return reinterpret_cast<std::uintptr_t>(&storeHelper<isa::Word>);
        default:
          return std::uintptr_t{};
        };
      }();

      push(frame.p[0]);
      mov(frame.t[0], helper);
      call(frame.t[0]);
      pop(frame.p[0]);
      break;
    }
    case kNumOpcodes:
      throw std::invalid_argument{"Unexpected insn id"};
    }
    if (!isa::changesPC(insn.opcode())) {
      add(getPc(), isa::kWordSize);
    }
    inc(qword[frame.p[0] + offsetof(CPUState, icount)]);
  }

  frame.close();
  ready();

  // Copy data to holder
  return CodeHolder{std::as_bytes(std::span{getCode(), getSize()})};
} // namespace
} // namespace

std::unique_ptr<ExecEngine> makeXbyak() { return std::make_unique<XByakJit>(); }
} // namespace prot::engine
