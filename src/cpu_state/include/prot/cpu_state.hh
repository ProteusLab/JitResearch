#ifndef PROT_CPU_STATE_HH_INCLUDED
#define PROT_CPU_STATE_HH_INCLUDED

#include <array>
#include <ostream>

#include "prot/isa.hh"

namespace prot {

struct Memory;

struct CPUState final {
  static constexpr std::size_t kNumRegs = 32;

  std::array<isa::Word, kNumRegs> regs{};
  isa::Word pc{};
  bool finished{false};
  Memory *memory{nullptr};
  isa::Icount icount{0};
  isa::Word m_ExitCode{0};

  explicit CPUState(Memory *mem) : memory(mem) {}

  void setPC(isa::Word newPC) { pc = newPC; }
  isa::Word getPC() const { return pc; }

  isa::Word getReg(std::size_t id) const { return regs[id]; }
  void setReg(std::size_t id, isa::Word val) {
    if (id != 0) {
      regs[id] = val;
    }
  }

  [[nodiscard]] isa::Word getSysCallNum() const { return getReg(17); }
  [[nodiscard]] isa::Word getSysCallArg(std::size_t num) const;
  [[nodiscard]] isa::Word getSysCallRet() const { return getReg(10); }
  [[nodiscard]] isa::Word getExitCode() const { return m_ExitCode; }

  void emulateSysCall();

  void dump(std::ostream &ost) const;

private:
  void doExit(isa::Word code);
};
} // namespace prot

#endif // PROT_CPU_STATE_HH_INCLUDED
