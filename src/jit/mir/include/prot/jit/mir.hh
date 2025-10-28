#ifndef PROT_JIT_MIR_HH_INCLUDED
#define PROT_JIT_MIR_HH_INCLUDED

#include <memory>

#include "prot/exec_engine.hh"

namespace prot::engine {
std::unique_ptr<ExecEngine> makeMirJit();
}

#endif // PROT_JIT_MIR_HH_INCLUDED
