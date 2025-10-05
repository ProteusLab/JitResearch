#include <CLI/CLI.hpp>

int main(int argc, const char *argv[]) {
  {
    CLI::App app{"App for JIT research from ProteusLab team"};
    CLI11_PARSE(app, argc, argv);
  }
}
