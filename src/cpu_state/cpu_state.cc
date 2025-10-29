#include "prot/cpu_state.hh"

#include <ranges>

#include <fmt/core.h>
#include <fmt/ostream.h>

namespace prot {

enum class NumSysCall : isa::Word {
  kExit = 93,
};

void CPUState::dump(std::ostream &ost) const {
  fmt::println(ost, "---CPU STATE DUMP---");
  fmt::println(ost, "Icount = {}", icount);
  fmt::println(ost, "PC = {:#x}", pc);

  static constexpr std::size_t kRegPerRow = 5;
  static constexpr std::size_t kNumRows =
      (kNumRegs + kRegPerRow - 1) / kRegPerRow;
  static_assert(kNumRows * kRegPerRow >= kNumRegs);

  for (auto i : std::views::iota(0U, kNumRows)) {
    for (auto j : std::views::iota(0U, kRegPerRow)) {
      const auto effIdx = (i * kRegPerRow) + j;
      if (effIdx >= regs.size()) {
        break;
      }
      fmt::print(ost, "X[{:02}] = {:#010x} ", effIdx, regs[effIdx]);
    }
    ost << "\n";
  }

  fmt::println(ost, "---CPU STATE DUMP END---");
}

void CPUState::doExit(isa::Word code) {
  finished = true;
  fmt::println("***********************");
  fmt::println("Exiting with code {}...", code);
  fmt::println("Icount: {}", fmt::group_digits(icount));
  fmt::println("***********************");
  m_ExitCode = code;
}

isa::Word CPUState::getSysCallArg(std::size_t num) const {
  static constexpr std::size_t kFirstArg = 10;
  static constexpr std::size_t kMaxArgs = 7;
  if (num >= kMaxArgs) {
    throw std::invalid_argument{
        fmt::format("Too high sys call arg number: {}", num)};
  }

  return regs[kFirstArg + num];
}

void CPUState::emulateSysCall() {
  using enum NumSysCall;
  switch (const auto num = static_cast<NumSysCall>(getSysCallNum())) {
  case kExit:
    doExit(getSysCallArg(0));
    return;
  default:
    throw std::runtime_error{
        fmt::format("Unknown syscall w/ num {}", toUnderlying(num))};
  }
}
} // namespace prot
