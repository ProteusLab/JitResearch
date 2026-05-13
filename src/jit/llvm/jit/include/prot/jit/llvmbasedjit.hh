#ifndef PROT_JIT_LLVMBASEDJIT_HH_INCLUDED
#define PROT_JIT_LLVMBASEDJIT_HH_INCLUDED

#include <llvm/Support/Error.h>
#include <memory>

#include "prot/jit/base.hh"

namespace prot::engine {
std::unique_ptr<Translator> makeLLVMBasedJIT();
} // end namespace prot::engine

#endif // PROT_JIT_LLVMBASEDJIT_HH_INCLUDED
