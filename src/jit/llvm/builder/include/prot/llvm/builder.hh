#ifndef INCLUDE_PROT_LLVM_BUILDER_HH_INCLUDED
#define INCLUDE_PROT_LLVM_BUILDER_HH_INCLUDED

#include <array>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/Error.h>

#include "prot/isa.hh"

namespace prot::ll {

const std::unordered_map<std::string_view, void *> &getFuncMapper();

std::pair<std::unique_ptr<llvm::LLVMContext>, std::unique_ptr<llvm::Module>>
translate(const std::string &name, const std::vector<isa::Instruction> &insns);

} // namespace prot::ll

#endif // INCLUDE_PROT_LLVM_BUILDER_HH_INCLUDED
