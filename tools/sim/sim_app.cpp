#include <CLI/CLI.hpp>

#include <chrono>
#include <filesystem>

#include <fmt/core.h>
#include <fmt/ostream.h>

#include "prot/elf_loader.hh"
#include "prot/hart.hh"
#include "prot/interpreter.hh"
#include "prot/jit/factory.hh"
#include "prot/memory.hh"

int main(int argc, const char *argv[]) try {

  std::filesystem::path elfPath;
  constexpr prot::isa::Addr kDefaultStack = 0x7fffffff;
  prot::isa::Addr stackTop{};
  std::string jitBackend{};
  prot::engine::JitEngine::Config jitConfig{};

  {
    CLI::App app{"App for JIT research from ProteusLab team"};

    app.add_option("elf", elfPath, "Path to executable ELF file")
        ->required()
        ->check(CLI::ExistingFile);

    app.add_option("--stack-top", stackTop, "Address of the stack top")
        ->default_val(kDefaultStack)
        ->default_str(fmt::format("{:#x}", kDefaultStack));

    auto *jit =
        app.add_option("--jit", jitBackend, "Use JIT & set backend")
            ->check(CLI::IsMember(prot::engine::JitFactory::backends()));
    auto *jitOpts = app.add_option_group("JIT options")->needs(jit);

    jitOpts->add_flag("--single-step", jitConfig.singleStep,
                      "Set single step (by instruction) JIT-simulation mode");
    jitOpts
        ->add_option("--exec-threshold", jitConfig.execThreshold,
                     "Specify amount of BB execs before translation starts")
        ->default_val(10)
        ->capture_default_str();

    jitOpts->add_flag("--dump-cpu", jitConfig.enableDump,
                      "Enable dump of CPU state before each TB");

    CLI11_PARSE(app, argc, argv);
  }
  const bool jitEnabled = !jitBackend.empty();

  auto hart = [&] {
    prot::ElfLoader loader{elfPath};

    auto engine = [&]() -> std::unique_ptr<prot::ExecEngine> {
      if (jitEnabled) {
        return prot::engine::JitFactory::createEngine(jitBackend, jitConfig);
      }
      return std::make_unique<prot::engine::Interpreter>();
    }();
    auto mem = prot::memory::makePlain(4ULL << 30U);
    prot::Hart hart{std::move(mem), std::move(engine)};
    hart.load(loader);
    hart.setSP(stackTop);

    return hart;
  }();

  auto start = std::chrono::high_resolution_clock::now();
  hart.run();
  auto end = std::chrono::high_resolution_clock::now();
  hart.dump(std::cout);
  std::chrono::duration<double> duration = end - start;
  fmt::println("icount: {}", hart.getIcount());
  fmt::println("time: {}s", duration.count());
  if (jitEnabled) {
    fmt::println("threshold: {}", jitConfig.execThreshold);
  }
  fmt::println("mips: {}", hart.getIcount() / (duration.count() * 1000000));
  return hart.getExitCode();
} catch (const std::exception &ex) {
  fmt::println(std::cerr, "Caught an exception of type {}, message: {}",
               typeid(ex).name(), ex.what());
  return EXIT_FAILURE;
} catch (...) {
  fmt::println(std::cerr, "Unkown exception was caught");
  return EXIT_FAILURE;
}
