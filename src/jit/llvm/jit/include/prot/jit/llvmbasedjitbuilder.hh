#ifndef PROT_JIT_LLVMBASEDJITBUILDER_HH_INCLUDED
#define PROT_JIT_LLVMBASEDJITBUILDER_HH_INCLUDED

#include <array>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/Error.h>

#include "prot/isa.hh"

namespace prot::engine {

struct IRData {
  llvm::Module &Module;
  llvm::IRBuilder<> &Builder;
  llvm::Function *CurrentFunction;
  std::array<llvm::Function *, 9> MemoryFunctions;
};

void buildInstruction(IRData &data, isa::Instruction insn);
} // end namespace prot::engine

#endif // PROT_JIT_LLVMBASEDJITBUILDER_HH_INCLUDED
