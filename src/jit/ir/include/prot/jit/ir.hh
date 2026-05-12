#ifndef PROT_JIT_IR_HH_INCLUDED
#define PROT_JIT_IR_HH_INCLUDED

#include <memory>

#include "prot/jit/base.hh"

namespace prot::engine {
std::unique_ptr<Translator> makeIrJit();
}

#endif // PROT_JIT_IR_HH_INCLUDED
