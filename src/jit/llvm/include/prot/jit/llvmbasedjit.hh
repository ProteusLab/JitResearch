#ifndef PROT_JIT_LLVMBASEDJIT_HH_INCLUDED
#define PROT_JIT_LLVMBASEDJIT_HH_INCLUDED

#include <llvm/Support/Error.h>
#include <memory>

#include "prot/exec_engine.hh"

namespace prot::engine {  
llvm::Expected<std::unique_ptr<ExecEngine>> makeLLVMBasedJIT(int argc, const char** argv);
} // end namespace prot::engine

#endif // PROT_JIT_LLVMBASEDJIT_HH_INCLUDED
