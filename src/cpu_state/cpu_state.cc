#include "prot/cpu_state.hh"

#include <ranges>

#include <fmt/core.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>

namespace prot {

enum class NumSysCall : isa::Word {
  kExit = 93,
};

void CPUState::dump(std::ostream &ost) const {
  static constexpr auto kAbiRegs = std::to_array<std::string_view>(
      {"zero", "ra", "sp", "gp", "tp",  "t0",  "t1", "t2", "s0", "s1", "a0",
       "a1",   "a2", "a3", "a4", "a5",  "a6",  "a7", "s2", "s3", "s4", "s5",
       "s6",   "s7", "s8", "s9", "s10", "s11", "t3", "t4", "t5", "t6"});

  static constexpr std::size_t kRegNameSpace =
      std::ranges::max(kAbiRegs | std::views::transform(std::ranges::size)) +
      5; // 4 for xXX/ + 1 for space

  fmt::println(ost, " {:<{}}{:08x}", "pc", kRegNameSpace, pc);
  static constexpr std::size_t kRegsByRow = 4;
  static_assert(kNumRegs % kRegsByRow == 0);

  for (std::size_t i = 0; i < regs.size(); i += 4) {
    auto regDumps =
        std::views::iota(0U, kRegsByRow) |
        std::views::transform([i](auto j) { return i + j; }) |
        std::views::transform([&](auto idx) {
          auto name = fmt::format("x{}/{}", idx, kAbiRegs[idx]);
          return fmt::format("{:<{}}{:08x}", name, kRegNameSpace, regs.at(idx));
        });

    fmt::println(ost, " {}", fmt::join(regDumps, " "));
  }
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
