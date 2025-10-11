#ifndef PROT_CPU_STATE_HH_INCLUDED
#define PROT_CPU_STATE_HH_INCLUDED

#include <array>

#include "prot/isa.hh"

namespace prot {

struct Memory;

struct CPUState final {
  static constexpr std::size_t kNumRegs = 32;

  std::array<isa::Word, kNumRegs> regs{};
  isa::Word pc{};
  Memory *memory{nullptr};
  bool finished{false};
  isa::Icount icount{0};

  explicit CPUState(Memory *mem) : memory(mem) {}

  void setPC(isa::Word newPC) { pc = newPC; }
  isa::Word getPC() const { return pc; }

  isa::Word getReg(std::size_t id) const { return regs[id]; }
  void setReg(std::size_t id, isa::Word val) {
    if (id != 0) {
      regs[id] = val;
    }
  }

  void dump(std::ostream &ost) const;
};
} // namespace prot

#endif // PROT_CPU_STATE_HH_INCLUDED
