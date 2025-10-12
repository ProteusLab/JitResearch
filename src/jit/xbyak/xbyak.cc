#include "prot/jit/xbyak.hh"
#include "prot/jit/base.hh"

#include <xbyak/xbyak.h>
#include <xbyak/xbyak_util.h>

#include <cassert>
#include <span>

#include <sys/mman.h>

#include <fmt/core.h>

namespace prot::engine {
namespace {
struct Unmap {
  std::size_t m_size = 0;

public:
  explicit Unmap(std::size_t size) noexcept : m_size(size) {}

  void operator()(void *ptr) noexcept {
    [[maybe_unused]] auto res = munmap(ptr, m_size);
    assert(res != -1);
  }
};

using JitFunction = void (*)(CPUState &);

class CodeHolder final {
public:
  explicit CodeHolder(std::span<const std::byte> src)
      : m_data(
            [sz = src.size()] {
              auto *ptr = mmap(NULL, sz, PROT_READ | PROT_WRITE | PROT_EXEC,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
              if (ptr == MAP_FAILED) {
                throw std::runtime_error{
                    fmt::format("Failed to allocate {} bytes for code", sz)};
              }

              return static_cast<std::byte *>(ptr);
            }(),
            Unmap{src.size()}) {
    std::ranges::copy(src, m_data.get());
    if (!Xbyak::CodeArray::protect(m_data.get(), m_data.get_deleter().m_size,
                                   Xbyak::CodeArray::PROTECT_RE)) {
      throw std::runtime_error{"Failed to change protection"};
    }
  }

  void operator()(CPUState &state) { asFunc()(state); }

private:
  JitFunction asFunc() const {
    return reinterpret_cast<JitFunction>(m_data.get());
  }
  std::unique_ptr<std::byte, Unmap> m_data;
};

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

CodeHolder XByakJit::translate(const BBInfo &info) {
  reset(); // XByak specific (CodeGenerator is about a PAGE size!!, so reuse it)
  Xbyak::util::StackFrame frame{this, 3, 3};

  [[maybe_unused]] auto temp1 = frame.t[0].cvt32();
  [[maybe_unused]] auto temp2 = frame.t[1].cvt32();
  [[maybe_unused]] auto temp3 = frame.t[2].cvt32();

  auto get_reg = [&frame](std::size_t regId) {
    return frame.p[0] + sizeof(isa::Word) * regId;
  };

  for (const auto &insn : info.insns) {
    auto get_rs1 = [&](Xbyak::Reg32 reg) {
      mov(reg, dword[get_reg(insn.rs1())]);
    };
    auto get_rs2 = [&](Xbyak::Reg32 reg) {
      mov(reg, dword[get_reg(insn.rs2())]);
    };

    auto set_rd = [&](Xbyak::Reg32 reg) {
      mov(dword[get_reg(insn.rd())], reg);
    };

    switch (insn.opcode()) {
    case isa::Opcode::kADD:
    case isa::Opcode::kADDI: {
      auto val = temp1;
      get_rs1(val);
      add(val, insn.imm());
      set_rd(val);
      break;
    }
    case isa::Opcode::kAND:
    case isa::Opcode::kANDI:
    case isa::Opcode::kAUIPC:
    case isa::Opcode::kBEQ:
    case isa::Opcode::kBGE:
    case isa::Opcode::kBGEU:
    case isa::Opcode::kBLT:
    case isa::Opcode::kBLTU:
    case isa::Opcode::kBNE:
    case isa::Opcode::kEBREAK:
    case isa::Opcode::kECALL: {
      // set finished
      mov(byte[frame.p[0] + (CPUState::kNumRegs + 1) * sizeof(isa::Word)], 1);
      break;
    }
    case isa::Opcode::kFENCE:
    case isa::Opcode::kJAL:
    case isa::Opcode::kJALR:
    case isa::Opcode::kLB:
    case isa::Opcode::kLBU:
    case isa::Opcode::kLH:
    case isa::Opcode::kLHU:
    case isa::Opcode::kLUI:
    case isa::Opcode::kLW:
    case isa::Opcode::kOR:
    case isa::Opcode::kORI:
    case isa::Opcode::kPAUSE:
    case isa::Opcode::kSB:
    case isa::Opcode::kSBREAK:
    case isa::Opcode::kSCALL:
    case isa::Opcode::kSH:
    case isa::Opcode::kSLL:
    case isa::Opcode::kSLLI:
    case isa::Opcode::kSLT:
    case isa::Opcode::kSLTI:
    case isa::Opcode::kSLTIU:
    case isa::Opcode::kSLTU:
    case isa::Opcode::kSRA:
    case isa::Opcode::kSRAI:
    case isa::Opcode::kSRL:
    case isa::Opcode::kSRLI:
    case isa::Opcode::kSUB:
    case isa::Opcode::kSW: {
      auto addr = frame.p[1];
      auto val = frame.p[2];
      get_rs1(addr.cvt32());
      add(addr.cvt32(), insn.imm()); // calc addr
      get_rs2(val.cvt32());

      push(frame.p[0]);
      mov(frame.t[0],
          reinterpret_cast<std::uintptr_t>(&storeHelper<isa::Word>));
      call(frame.t[0]);
      pop(frame.p[0]);
      break;
    }
    case isa::Opcode::kXOR:
    case isa::Opcode::kXORI:
    case isa::Opcode::kNumOpcodes:
      break;
    }
  }
  frame.close();
  ready();
  fmt::println("CODE DUMP");
  dump();
  fmt::println("CODE DUMP END");

  // Copy data to holder

  return CodeHolder{std::as_bytes(std::span{getCode(), getSize()})};
}
} // namespace

std::unique_ptr<ExecEngine> makeXbyak() { return std::make_unique<XByakJit>(); }
} // namespace prot::engine
