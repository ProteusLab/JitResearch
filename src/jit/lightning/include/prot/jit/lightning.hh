#ifndef INCLUDE_PROT_JIT_LIGHTNING_HH_INCLUDED
#define INCLUDE_PROT_JIT_LIGHTNING_HH_INCLUDED

#include <memory>

#include "prot/jit/base.hh"

namespace prot::engine {
std::unique_ptr<Translator> makeLightning();
}

#endif // INCLUDE_PROT_JIT_LIGHTNING_HH_INCLUDED
