#ifndef PROT_JIT_TPDE_HH_INCLUDED
#define PROT_JIT_TPDE_HH_INCLUDED

#include <memory>

#include "prot/jit/base.hh"

namespace prot::engine {
std::unique_ptr<Translator> makeTPDE();
}

#endif // PROT_JIT_TPDE_HH_INCLUDED
