#include <CLI/CLI.hpp>

#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <ranges>
#include <string>
#include <utility>
#include <vector>

#include <fmt/base.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>

#include <perfcpp/counter_result.h>
#include <perfcpp/event_counter.h>

#include "prot/elf_loader.hh"
#include "prot/hart.hh"
#include "prot/interpreter.hh"
#include "prot/jit/factory.hh"
#include "prot/memory.hh"

namespace {

struct RunConfig {
  const std::filesystem::path &elfPath;
  prot::isa::Addr stackTop = 0x7fffffff;
  const std::string &jitBackend;
  const std::vector<std::string> &perfEvents;
  bool dumpHart = false;
};

struct RunResult {
  uint32_t status = 0;
  uint64_t guestIc = 0;
  perf::CounterResult perfRes;
};

RunResult run(const RunConfig &cfg) {
  auto hart = [&] {
    prot::ElfLoader loader{cfg.elfPath};

    std::unique_ptr<prot::ExecEngine> engine =
        !cfg.jitBackend.empty()
            ? prot::engine::JitFactory::createEngine(cfg.jitBackend)
            : std::make_unique<prot::engine::Interpreter>();

    prot::Hart hart{prot::memory::makePlain(4ULL << 30U), std::move(engine)};
    hart.load(loader);
    hart.setSP(cfg.stackTop);

    return hart;
  }();

  auto eventCounter = perf::EventCounter{};
  eventCounter.add(cfg.perfEvents);

  eventCounter.start();
  hart.run();
  eventCounter.stop();

  if (cfg.dumpHart) {
    hart.dump(std::cout);
  }

  return RunResult{.status = hart.getExitCode(),
                   .guestIc = hart.getIcount(),
                   .perfRes = eventCounter.result()};
}

} // namespace

int main(int argc, const char *argv[]) try {
  std::filesystem::path elfPath;
  constexpr prot::isa::Addr kDefaultStack = 0x7fffffff;
  prot::isa::Addr stackTop{};
  std::string jitBackend{};
  std::filesystem::path statsCsvPath;
  bool dumpHart = false;

  {
    CLI::App app{"App for JIT research from ProteusLab team"};

    app.add_option("elf", elfPath, "Path to executable ELF file")
        ->required()
        ->check(CLI::ExistingFile);

    app.add_option("--stack-top", stackTop, "Address of the stack top")
        ->default_val(kDefaultStack)
        ->default_str(fmt::format("{:#x}", kDefaultStack));

    app.add_option("--jit", jitBackend, "Use JIT & set backend")
        ->check(CLI::IsMember(prot::engine::JitFactory::backends()));

    app.add_option(
        "--stats", statsCsvPath,
        "Path to CSV stats file. Enables stats collection regime if set");

    app.add_flag("--dump", dumpHart, "Dump hart values on each run");

    CLI11_PARSE(app, argc, argv);
  }

  bool collectStats = !statsCsvPath.empty();

  // Group related counters together
  // clang-format off
  std::vector<std::vector<std::string>> counters {
    {
      "seconds",
      "instructions",
      "cycles",
      "branches",
      "branch-misses",
    },
    {
      "L1-dcache-loads",
      "L1-dcache-load-misses",
      "L1-icache-loads",
      "L1-icache-load-misses",
      "iTLB-load-misses",
      "dTLB-load-misses",
    },
  };
  // clang-format on

  std::ofstream statsCsv{};
  if (collectStats) {
    std::filesystem::create_directories(statsCsvPath.parent_path());

    bool addHeader = !std::filesystem::exists(statsCsvPath);
    statsCsv.open(statsCsvPath, std::ios_base::app | std::ios_base::out);

    if (addHeader) {
      fmt::println(statsCsv, "{}", fmt::join(counters | std::views::join, ","));
    }
  }

  auto runNum = 0;
  for (const auto &runCounters :
       counters | std::views::take(collectStats ? 0xffU : 1U)) {
    fmt::println("Run #{}", runNum++);

    auto res = run({
        .elfPath = elfPath,
        .stackTop = stackTop,
        .jitBackend = jitBackend,
        .perfEvents = runCounters,
        .dumpHart = dumpHart,
    });

    auto seconds = res.perfRes["seconds"];
    auto cycles = res.perfRes["cycles"];
    auto hostInstrs = res.perfRes["instructions"];

    if (seconds) {
      fmt::println("  time: {:.2f}s", *seconds);
    }

    if (seconds && cycles && hostInstrs) {
      auto guestIPS = res.guestIc / *seconds;
      fmt::println("  MIPS: {:.2f}", guestIPS / 1'000'000);
    }

    if (collectStats) {
      fmt::print(statsCsv, "{}",
                 fmt::join(res.perfRes | std::views::values, ","));
    }

    fmt::println("  status = {}", res.status);
    if (res.status != 0) {
      fmt::println("Run failed, aborting");
      return res.status;
    }
  }

  if (collectStats) {
    statsCsv << "\n";
  }

  return 0;
} catch (const std::exception &ex) {
  fmt::println(std::cerr, "Caught an exception of type {}, message: {}",
               typeid(ex).name(), ex.what());
  return EXIT_FAILURE;
} catch (...) {
  fmt::println(std::cerr, "Unkown exception was caught");
  return EXIT_FAILURE;
}
