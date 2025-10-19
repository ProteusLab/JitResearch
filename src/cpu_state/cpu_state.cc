#include "prot/cpu_state.hh"

#include <fmt/core.h>
#include <fmt/ostream.h>

namespace prot {
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
} // namespace prot
