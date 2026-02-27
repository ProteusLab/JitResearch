#include <CLI/CLI.hpp>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <ios>
#include <iterator>
#include <optional>
#include <ranges>
#include <span>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include <fmt/base.h>
#include <fmt/core.h>
#include <fmt/format.h>
#include <fmt/ostream.h>
#include <fmt/ranges.h>

#include <perfcpp/counter_definition.h>
#include <perfcpp/counter_result.h>
#include <perfcpp/event_counter.h>

#include "prot/elf_loader.hh"
#include "prot/hart.hh"
#include "prot/interpreter.hh"
#include "prot/jit/base.hh"
#include "prot/jit/factory.hh"
#include "prot/memory.hh"

namespace {

struct RunConfig {
  std::filesystem::path elfPath;
  prot::isa::Addr stackTop = 0;
  std::string jitBackend;
  size_t jitTranslationThreshold = 0;

  std::vector<std::string> metrics;
  bool dumpHart = false;
};

struct RunResult {
  uint32_t status = 0;
  struct Metrics {
    std::vector<std::pair<std::string, double>> metrics;

    std::optional<double> operator[](std::string_view name) {
      if (auto it = std::ranges::find_if(
              metrics, [&](const auto &s) { return s == name; },
              &decltype(metrics)::value_type::first);
          it != metrics.end()) {
        return it->second;
      }

      return std::nullopt;
    }
  } metrics;
};

RunResult run(const RunConfig &cfg) {
  auto hart = [&] {
    prot::ElfLoader loader{cfg.elfPath};

    bool hasJit = !cfg.jitBackend.empty();

    std::unique_ptr<prot::ExecEngine> engine =
        hasJit ? prot::engine::JitFactory::createEngine(cfg.jitBackend)
               : std::make_unique<prot::engine::Interpreter>();

    if (hasJit) {
      auto *jit = dynamic_cast<prot::engine::JitEngine *>(engine.get());
      jit->setTranslationThreshold(cfg.jitTranslationThreshold);
    }

    prot::Hart hart{prot::memory::makePlain(4ULL << 30U), std::move(engine)};
    hart.load(loader);
    hart.setSP(cfg.stackTop);

    return hart;
  }();

  auto perfMetrics = [&]() {
    auto rng = cfg.metrics | std::views::filter([](const auto &name) {
                 const auto &defs = perf::CounterDefinition::global();
                 return defs.is_metric(name) || !defs.counter(name).empty() ||
                        defs.is_time_event(name);
               }) |
               std::views::common;

    return std::vector<std::string>{rng.begin(), rng.end()};
  }();

  auto eventCounter = perf::EventCounter{};
  if (!eventCounter.add(perfMetrics, perf::EventCounter::Schedule::Group)) {
    throw std::runtime_error(
        fmt::format("Failed to add perf events group: {{{}}}",
                    fmt::join(perfMetrics, ",")));
  }

  eventCounter.start();
  hart.run();
  eventCounter.stop();

  if (cfg.dumpHart) {
    hart.dump(std::cout);
  }

  std::vector<std::pair<std::string, double>> metrics{};

  for (const auto &metric : cfg.metrics) {
    // Dump perf metrics
    if (auto perfMetric = eventCounter.result().get(metric)) {
      metrics.emplace_back(metric, *perfMetric);
    }
    // Dump custom metrics
    else if (metric == "guest-instructions") {
      metrics.emplace_back(metric, hart.getIcount());
    } else if (metric == "jit-threshold") {
      metrics.emplace_back(metric, cfg.jitTranslationThreshold);
    }
  }

  return RunResult{.status = hart.getExitCode(),
                   .metrics = {std::move(metrics)}};
}

} // namespace

int main(int argc, const char *argv[]) try {
  constexpr prot::isa::Addr kDefaultStack = 0x7fff'ffff;
  constexpr size_t kDefaultJitThreshold = 1'000;

  RunConfig cfg{};
  std::filesystem::path statsCsvPath;

  {
    CLI::App app{"App for JIT research from ProteusLab team"};

    app.add_option("elf", cfg.elfPath, "Path to executable ELF file")
        ->required()
        ->check(CLI::ExistingFile);

    app.add_option("--stack-top", cfg.stackTop, "Address of the stack top")
        ->default_val(kDefaultStack)
        ->default_str(fmt::format("{:#x}", kDefaultStack));

    app.add_option("--jit", cfg.jitBackend, "Use JIT & set backend")
        ->check(CLI::IsMember(prot::engine::JitFactory::backends()));

    app.add_option("--jit-threshold", cfg.jitTranslationThreshold,
                   "Execution count threshold for code to be JIT-ed")
        ->default_val(kDefaultJitThreshold);

    app.add_option(
        "--stats", statsCsvPath,
        "Path to CSV stats file. Enables stats collection regime if set");

    app.add_flag("--dump", cfg.dumpHart, "Dump hart values on each run");

    CLI11_PARSE(app, argc, argv);
  }

  bool collectStats = !statsCsvPath.empty();

  // Group related metrics together
  std::vector<std::vector<std::string>> metrics{
      // First group of metrics is always collected
      {
          // Config info
          "jit-threshold",
          // Guest metrics
          "guest-instructions",
          // Perf metrics
          "seconds",
          "instructions",
          "cycles",
          "branches",
          "branch-misses",
      },
      // Cache-related metrics (1)
      {
          "L1-dcache-loads",
          "L1-dcache-load-misses",
          "L1-icache-loads",
          "L1-icache-load-misses",
      },
      // Cache-related metrics (2)
      {
          "iTLB-load-misses",
          "dTLB-load-misses",
      }};

  std::ofstream statsCsv{};
  if (collectStats) {
    std::filesystem::create_directories(statsCsvPath.parent_path());

    bool addHeader = !std::filesystem::exists(statsCsvPath);
    statsCsv.open(statsCsvPath, std::ios_base::app | std::ios_base::out);

    if (addHeader) {
      fmt::println(statsCsv, "{}", fmt::join(metrics | std::views::join, ","));
    }
  }

  auto runNum = 0;
  for (const auto &runMetrics :
       metrics | std::views::take(collectStats ? 0xffU : 1U)) {
    fmt::println("Run #{}", runNum);

    auto runCfg = cfg;
    runCfg.metrics = runMetrics;
    auto res = run(runCfg);

    if (runNum == 0) {
      auto seconds = res.metrics["seconds"].value();
      auto guestInstrs = res.metrics["guest-instructions"].value();

      fmt::println("  time: {:.2f}s", seconds);
      auto guestIPS = guestInstrs / seconds;
      fmt::println("  MIPS: {:.2f}", guestIPS / 1'000'000);
    }

    if (collectStats) {
      fmt::print(statsCsv, "{}",
                 fmt::join(res.metrics.metrics | std::views::values, ","));
    }

    fmt::println("  status = {}", res.status);
    if (res.status != 0) {
      fmt::println("Run failed, aborting");
      return res.status;
    }

    ++runNum;
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
