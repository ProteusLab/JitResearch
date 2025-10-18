#include "prot/jit/base.hh"

#include <fmt/core.h>

namespace prot::engine {
void JitEngine::step(CPUState &cpu) {
  if (doJIT(cpu)) {
    return;
  }

  // colllect bb
  auto [bbIt, wasNew] = m_cacheBB.try_emplace(cpu.getPC());
  if (wasNew) {
    auto curAddr = bbIt->first;
    auto &bb = bbIt->second;

    while (true) {
      auto bytes = cpu.memory->read<isa::Word>(curAddr);
      auto inst = isa::Instruction::decode(bytes);
      if (!inst.has_value()) {
        throw std::runtime_error{
            fmt::format("Cannot decode bytes: {:#x}", bytes)};
      }

      bb.insns.push_back(*inst);
      if (isa::isTerminator(inst->opcode())) {
        break;
      }
      curAddr += sizeof(isa::Word);
    }
  }

  // interpret(cpu, bbIt->second);
}
void JitEngine::interpret(CPUState &cpu, BBInfo &info) {
  for (const auto &insn : info.insns) {
    execute(cpu, insn);
    cpu.icount++;
  }
  info.num_exec++;
}

auto JitEngine::getBBInfo(isa::Addr pc) const -> const BBInfo * {
  if (const auto found = m_cacheBB.find(pc); found != m_cacheBB.end()) {
    if (found->second.num_exec >= kExecThreshold) {
      return &found->second;
    }
  }

  return nullptr;
}
} // namespace prot::engine
