#include "prot/cpu_state.hh"

#include <fmt/core.h>
#include <fmt/ostream.h>

namespace prot {
void CPUState::dump(std::ostream &ost) const {
  fmt::println(ost, "---CPU STATE DUMP---");
  fmt::println(ost, "Icount = {}", icount);
  fmt::println(ost, "PC = {:#x}", pc);

  for (std::size_t i = 0; i < regs.size(); ++i) {
    fmt::println(ost, "X[{}] = {:#x}", i, regs[i]);
  }

  fmt::println(ost, "---CPU STATE DUMP END---");
}
} // namespace prot
