#ifndef PROT_JIT_ASMJIT_HH_INCLUDED
#define PROT_JIT_ASMJIT_HH_INCLUDED

#include <memory>

#include "prot/exec_engine.hh"

namespace prot::engine {
std::unique_ptr<ExecEngine> makeAsmJit();
}

#endif // PROT_JIT_ASMJIT_HH_INCLUDED
