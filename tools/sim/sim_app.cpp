#include <CLI/CLI.hpp>

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

    CLI11_PARSE(app, argc, argv);
  }

  auto hart = [&] {
    prot::ElfLoader loader{elfPath};

    std::unique_ptr<prot::ExecEngine> engine =
        !jitBackend.empty() ? prot::engine::JitFactory::createEngine(jitBackend)
                            : std::make_unique<prot::engine::Interpreter>();

    prot::Hart hart{prot::memory::makePaged(12), std::move(engine)};
    hart.load(loader);
    hart.setSP(stackTop);

    return hart;
  }();

  hart.dump(std::cout);
  hart.run();
  hart.dump(std::cout);
  fmt::println("Finish execution");
  return hart.getExitCode();
} catch (const std::exception &ex) {
  fmt::println(std::cerr, "Caught an exception of type {}, message: {}",
               typeid(ex).name(), ex.what());
  return EXIT_FAILURE;
} catch (...) {
  fmt::println(std::cerr, "Unkown exception was caught");
  return EXIT_FAILURE;
}
