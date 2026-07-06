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
class XByakJit : public Translator, private Xbyak::CodeGenerator {
public:
  XByakJit()
      : Xbyak::CodeGenerator{Xbyak::DEFAULT_MAX_CODE_SIZE, Xbyak::AutoGrow} {}

private:
  [[nodiscard]] JitFunction translate(const BBInfo &info) override;

  std::vector<CodeHolder> m_holders;
};

void syscallHelper(CPUState &state) { state.emulateSysCall(); }

JitFunction XByakJit::translate(const BBInfo &info) {
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

  auto getMemBase = [&frame, this](Xbyak::Reg64 dst) {
    mov(dst, qword[frame.p[0] + offsetof(CPUState, mem_base)]);
  };

  auto getMemAccOp = [&](Xbyak::Reg32 base, Xbyak::Reg32 addr,
                         const Xbyak::AddressFrame &mem_type) {
    getMemBase(base.cvt64());
    return mem_type[base.cvt64() + addr.cvt64()];
  };

  isa::Addr curPC = info.startPC;

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
      mov(temp1, static_cast<std::uint32_t>(curPC + insn.imm()));
      setRd(temp1);
      break;
    }

#define PROT_MAKE_IMPL(Op, cc)                                                 \
  case kB##Op: {                                                               \
    getRs1(temp1);                                                             \
    cmp(temp1, getReg(insn.rs2()));                                            \
    mov(temp1, static_cast<std::uint32_t>(curPC + isa::kWordSize));            \
    mov(temp2, static_cast<std::uint32_t>(curPC + insn.imm()));                \
    cmov##cc(temp1, temp2);                                                    \
                                                                               \
    mov(getPc(), temp1);                                                       \
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
      mov(temp1, static_cast<std::uint32_t>(curPC + isa::kWordSize));
      setRd(temp1);

      mov(getPc(), static_cast<std::uint32_t>(curPC + insn.imm()));
      break;
    }
    case kJALR: {
      getRs1(temp2);
      add(temp2, insn.imm());
      and_(temp2, ~std::uint32_t{1});
      mov(getPc(), temp2);

      mov(temp1, static_cast<std::uint32_t>(curPC + isa::kWordSize));
      setRd(temp1);
      break;
    }
    case kLUI: {
      if (insn.rd() != 0) {
        mov(getReg(insn.rd()), insn.imm());
      }
      break;
    }
#define PROT_MAKE_LOAD(OP, MOV_OP, WORD_OP)                                    \
  case k##OP:                                                                  \
    getRs1(temp1);                                                             \
    add(temp1, insn.imm());                                                    \
    MOV_OP(temp1, getMemAccOp(temp2, temp1, WORD_OP));                         \
    setRd(temp1);                                                              \
    break;
      PROT_MAKE_LOAD(LB, movsx, byte);
      PROT_MAKE_LOAD(LBU, movzx, byte);
      PROT_MAKE_LOAD(LH, movsx, word);
      PROT_MAKE_LOAD(LHU, movzx, word);
      PROT_MAKE_LOAD(LW, mov, dword);

#undef PROT_MAKE_LOAD

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

#define PROT_MAKE_STORE(OP, MEM_OP, BITS)                                      \
  case k##OP:                                                                  \
    getRs1(temp1);                                                             \
    add(temp1, insn.imm());                                                    \
    getRs2(temp3);                                                             \
    mov(getMemAccOp(temp2, temp1, MEM_OP), temp3.cvt##BITS());                 \
    break;

      PROT_MAKE_STORE(SB, byte, 8)
      PROT_MAKE_STORE(SH, word, 16)
      PROT_MAKE_STORE(SW, dword, 32)

    case kNumOpcodes:
      throw std::invalid_argument{"Unexpected insn id"};
    }
    curPC += isa::kWordSize;
  }

  if (info.insns.empty() || !isa::changesPC(info.insns.back().opcode())) {
    mov(getPc(), static_cast<std::uint32_t>(curPC));
  }

  add(qword[frame.p[0] + offsetof(CPUState, icount)],
      static_cast<int>(info.insns.size()));

  const bool lastIsJalr =
      !info.insns.empty() && info.insns.back().opcode() == isa::Opcode::kJALR;
  if (!lastIsJalr) {
    bool hasEcall = false;
    for (const auto &in : info.insns) {
      if (in.opcode() == isa::Opcode::kECALL) {
        hasEcall = true;
        break;
      }
    }

    Xbyak::Label skip;
    if (hasEcall) {
      cmp(byte[frame.p[0] + offsetof(CPUState, finished)], 0);
      jne(skip);
    }
    mov(temp1, getPc());
    mov(temp2, temp1);
    shr(temp2, kTbCacheGranularityLog2);
    and_(temp2, static_cast<std::uint32_t>(kTbCacheMask));
    mov(temp3.cvt64(), qword[frame.p[0] + offsetof(CPUState, tb_cache_base)]);
    shl(temp2.cvt64(), 4);
    add(temp3.cvt64(), temp2.cvt64());
    cmp(temp1, dword[temp3.cvt64() + offsetof(TbCacheEntry, gpa)]);
    jne(skip);
    mov(temp2.cvt64(), qword[temp3.cvt64() + offsetof(TbCacheEntry, func)]);
    mov(qword[frame.p[0] + offsetof(CPUState, next_tb)], temp2.cvt64());
    L(skip);
  }

  frame.close();
  ready();
  // Copy data to holder
  return m_holders.emplace_back(std::as_bytes(std::span{getCode(), getSize()}))
      .as<JitFunction>();
} // namespace
} // namespace

std::unique_ptr<Translator> makeXbyak() { return std::make_unique<XByakJit>(); }
} // namespace prot::engine
