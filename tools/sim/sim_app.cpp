#include <CLI/CLI.hpp>

#include <filesystem>

#include <fmt/core.h>
#include <fmt/ostream.h>

#include "prot/elf_loader.hh"
#include "prot/hart.hh"
#include "prot/interpreter.hh"
#include "prot/memory.hh"

int main(int argc, const char *argv[]) try {

  std::filesystem::path elfPath;
  constexpr prot::isa::Addr kDefaultStack = 0x7fffffff;
  prot::isa::Addr stackTop{};
  {
    CLI::App app{"App for JIT research from ProteusLab team"};

    app.add_option("elf", elfPath, "Path to executable ELF file")
        ->required()
        ->check(CLI::ExistingFile);

    app.add_option("--stack-top", stackTop, "Address of the stack top")
        ->default_val(kDefaultStack)
        ->default_str(fmt::format("{:#x}", kDefaultStack));

    CLI11_PARSE(app, argc, argv);
  }

  auto hart = [&] {
    prot::ElfLoader loader{elfPath};
    prot::Hart hart{prot::memory::makePlain(0x80000000, 0x10000),
                    prot::engine::makeInterpreter()};
    hart.load(loader);
    hart.setSP(stackTop);

    return hart;
  }();

  hart.dump(std::cout);
  hart.run();
  hart.dump(std::cout);
} catch (const std::exception &ex) {
  fmt::println(std::cerr, "Caught an exception of type {}, message: {}",
               typeid(ex).name(), ex.what());
  return EXIT_FAILURE;
} catch (...) {
  fmt::println(std::cerr, "Unkown exception was caught");
  return EXIT_FAILURE;
}
