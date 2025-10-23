#include <CLI/CLI.hpp>

#include <filesystem>

#include <fmt/core.h>
#include <fmt/ostream.h>

#include "prot/elf_loader.hh"
#include "prot/hart.hh"
#include "prot/interpreter.hh"
#include "prot/jit/factory.hh"
#include "prot/jit/llvmbasedjit.hh"
#include "prot/memory.hh"

int main(int argc, const char *argv[]) try {

  std::filesystem::path elfPath;
  constexpr prot::isa::Addr kDefaultStack = 0x7fffffff;
  prot::isa::Addr stackTop{};
  bool jit{};
  std::string jitBackend{};

  {
    CLI::App app{"App for JIT research from ProteusLab team"};

    app.add_option("elf", elfPath, "Path to executable ELF file")
        ->required()
        ->check(CLI::ExistingFile);

    app.add_option("--stack-top", stackTop, "Address of the stack top")
        ->default_val(kDefaultStack)
        ->default_str(fmt::format("{:#x}", kDefaultStack));

    app.add_flag("--jit", jit, "Use jit")->default_val(false);

    app.add_option("--jit-backend", jitBackend, "JIT backend")
        ->check(CLI::IsMember({prot::engine::JitFactory::kXbyakJitName,
                               prot::engine::JitFactory::kAsmJitName,
                               prot::engine::JitFactory::kLLVMJitName}))
        ->default_val(prot::engine::JitFactory::kXbyakJitName)
        ->needs("--jit");

    CLI11_PARSE(app, argc, argv);
  }

  auto hart = [&] {
    prot::ElfLoader loader{elfPath};

    std::unique_ptr<prot::ExecEngine> engine;
    if (jit) {
      try {
        engine = prot::engine::JitFactory::createEngine(jitBackend);
      } catch (const std::exception &e) {
        std::cerr << "Failed to create JIT engine: " << e.what()
                  << ", falling back to interpreter\n";
        engine = std::make_unique<prot::engine::Interpreter>();
      }
    } else {
      engine = std::make_unique<prot::engine::Interpreter>();
    }

    prot::Hart hart{prot::memory::makePaged(12), std::move(engine)};
    hart.load(loader);
    hart.setSP(stackTop);

    return hart;
  }();

  hart.dump(std::cout);
  hart.run();
  hart.dump(std::cout);
  fmt::println("Finish execution");
} catch (const std::exception &ex) {
  fmt::println(std::cerr, "Caught an exception of type {}, message: {}",
               typeid(ex).name(), ex.what());
  return EXIT_FAILURE;
} catch (...) {
  fmt::println(std::cerr, "Unkown exception was caught");
  return EXIT_FAILURE;
}
