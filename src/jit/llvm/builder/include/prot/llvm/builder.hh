#ifndef INCLUDE_PROT_LLVM_BUILDER_HH_INCLUDED
#define INCLUDE_PROT_LLVM_BUILDER_HH_INCLUDED

#include <array>
#include <llvm/IR/Function.h>
#include <llvm/IR/IRBuilder.h>
#include <llvm/Support/Error.h>

#include "prot/isa.hh"

namespace prot::ll {

const std::unordered_map<std::string_view, void *> &getFuncMapper();

// Chaining strategy for the generated block function.
//   None     - block ends with `ret void` (dispatcher handles the next TB).
//   MustTail - block ends with an inline TB-cache probe + guaranteed tail call
//              to the successor (requires a JIT that honors `musttail`;
//              TPDE does NOT, so it must use None).
enum class ChainMode { None, MustTail };

std::pair<std::unique_ptr<llvm::LLVMContext>, std::unique_ptr<llvm::Module>>
translate(const std::string &name, const std::vector<isa::Instruction> &insns,
          isa::Addr startPC, ChainMode chainMode);

} // namespace prot::ll

#endif // INCLUDE_PROT_LLVM_BUILDER_HH_INCLUDED
