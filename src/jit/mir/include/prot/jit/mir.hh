#ifndef PROT_JIT_MIR_HH_INCLUDED
#define PROT_JIT_MIR_HH_INCLUDED

#include <memory>

#include "prot/jit/base.hh"

namespace prot::engine {
std::unique_ptr<Translator> makeMirJit();
}

#endif // PROT_JIT_MIR_HH_INCLUDED
