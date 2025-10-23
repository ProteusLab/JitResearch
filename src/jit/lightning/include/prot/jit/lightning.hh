#ifndef INCLUDE_PROT_JIT_LIGHTNING_HH_INCLUDED
#define INCLUDE_PROT_JIT_LIGHTNING_HH_INCLUDED

#include <memory>

#include "prot/exec_engine.hh"

namespace prot::engine {
std::unique_ptr<ExecEngine> makeLightning();
}

#endif // INCLUDE_PROT_JIT_LIGHTNING_HH_INCLUDED
