#ifndef PROT_JIT_LLVMBASEDJIT_HH_INCLUDED
#define PROT_JIT_LLVMBASEDJIT_HH_INCLUDED

#include <llvm/Support/Error.h>
#include <memory>

#include "prot/exec_engine.hh"

namespace prot::engine {  
std::unique_ptr<ExecEngine> makeLLVMBasedJIT();
} // end namespace prot::engine

#endif // PROT_JIT_LLVMBASEDJIT_HH_INCLUDED
