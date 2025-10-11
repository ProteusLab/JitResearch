#ifndef PROT_INTERPRETER_HH_INCLUDED
#define PROT_INTERPRETER_HH_INCLUDED

#include <memory>

#include "prot/exec_engine.hh"

namespace prot::engine {
std::unique_ptr<ExecEngine> makeInterpreter();
} // namespace prot::engine

#endif // PROT_INTERPRETER_HH_INCLUDED
