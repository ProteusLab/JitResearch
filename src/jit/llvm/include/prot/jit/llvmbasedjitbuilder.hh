#ifndef PROT_JIT_LLVMBASEDJITBUILDER_HH_INCLUDED
#define PROT_JIT_LLVMBASEDJITBUILDER_HH_INCLUDED

#include <array>
#include <llvm/IR/Function.h>
#include <llvm/Support/Error.h>
#include <llvm/IR/IRBuilder.h>
#include <memory>

#include "prot/exec_engine.hh"
#include "prot/isa.hh"
#include "llvm/IR/DerivedTypes.h"

namespace prot::engine {

struct IRData {
  llvm::Module& Module;
  llvm::IRBuilder<>& Builder;
  llvm::Function* CurrentFunction;
  std::array<llvm::Function*, 8> MemoryFunctions;
};    

void buildInstruction(IRData& data, isa::Instruction insn);
} // end namespace prot::engine

#endif // PROT_JIT_LLVMBASEDJITBUILDER_HH_INCLUDED
