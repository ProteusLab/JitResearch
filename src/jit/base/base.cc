#include "prot/jit/base.hh"

#include <fmt/core.h>
#include <fmt/ostream.h>

#include <cassert>
#include <iostream>
#include <unordered_set>

extern "C" {
#include <sys/mman.h>
}

namespace prot::engine {
void JitEngine::step(CPUState &cpu) {
  cpu.tb_cache_base = m_tbCache.baseAddr();
  while (!cpu.finished) [[likely]] {
    if (m_config.enableDump) {
      cpu.dump(std::cout);
    }

    // collect bb
    const auto pc = cpu.getPC();
    if (m_translator) {
      if (JitFunction fn = m_tbCache.lookup(pc); fn != nullptr) [[likely]] {
        // Block chaining
        do {
          cpu.next_tb = nullptr;
          fn(cpu);
          fn = reinterpret_cast<JitFunction>(const_cast<void *>(cpu.next_tb));
        } while (fn != nullptr);
        continue;
      }
    }

    auto [bbIt, wasNew] = m_cacheBB.try_emplace(pc);
    if (wasNew) [[unlikely]] {
      auto curAddr = bbIt->first;
      auto &bb = bbIt->second;
      bb.startPC = curAddr;

      while (true) {
        auto bytes = cpu.memory->read<isa::Word>(curAddr);
        auto inst = isa::Instruction::decode(bytes);
        if (!inst.has_value()) {
          throw std::runtime_error{fmt::format(
              "Cannot decode bytes: {:#x} on pc: {:#x}", bytes, curAddr)};
        }

        bb.insns.push_back(*inst);

        if (m_config.singleStep || isa::isTerminator(inst->opcode())) {
          break;
        }
        curAddr += isa::kWordSize;
      }
    }
    if (m_translator && bbIt->second.num_exec >= m_config.execThreshold)
        [[likely]] {
      BBInfo superBB = bbIt->second;
      std::unordered_set<isa::Addr> visited{superBB.startPC};

      isa::Addr lastInsnPC =
          superBB.startPC +
          (superBB.insns.size() - 1) * isa::kWordSize;

      for (std::size_t depth = 0; depth < kMaxSuperblockDepth; ++depth) {
        if (superBB.insns.empty() ||
            superBB.insns.back().opcode() != isa::Opcode::kJAL)
          break;

        if (superBB.insns.size() >= kMaxSuperblockInsns)
          break;

        isa::Addr target = lastInsnPC + superBB.insns.back().imm();
        if (visited.contains(target))
          break;

        auto targetIt = m_cacheBB.find(target);
        if (targetIt == m_cacheBB.end())
          break;

        visited.insert(target);
        const auto &tgtBB = targetIt->second;

        superBB.insns.insert(superBB.insns.end(), tgtBB.insns.begin(),
                             tgtBB.insns.end());

        if (!tgtBB.insns.empty())
          lastInsnPC = target + (tgtBB.insns.size() - 1) * isa::kWordSize;
      }

      auto code = m_translator->translate(superBB);
      if (code == nullptr) [[unlikely]] {
        throw std::runtime_error{
            fmt::format("Failed to translate BB on pc: {:#x}", pc)};
      }

      code(cpu);
      m_tbCache.insert(pc, code);
      continue;
    }

    interpret(cpu, bbIt->second);
  }
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
    if (found->second.num_exec >= m_config.execThreshold) {
      return &found->second;
    }
  }

  return nullptr;
}

void CodeHolder::Unmap::operator()(void *ptr) const noexcept {
  [[maybe_unused]] auto res = ::munmap(ptr, m_size);
  assert(res != -1);
}

CodeHolder::CodeHolder(std::span<const std::byte> src)
    : m_data(
          [sz = src.size()] {
            // NOLINTNEXTLINE
            auto *ptr = ::mmap(NULL, sz, PROT_READ | PROT_WRITE | PROT_EXEC,
                               MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
            if (ptr == MAP_FAILED) {
              throw std::runtime_error{
                  fmt::format("Failed to allocate {} bytes for code", sz)};
            }

            return static_cast<std::byte *>(ptr);
          }(),
          Unmap{src.size()}) {
  std::ranges::copy(src, m_data.get());

  if (::mprotect(m_data.get(), m_data.get_deleter().m_size,
                 PROT_READ | PROT_EXEC) == -1) {
    throw std::runtime_error{"Failed to change protection"};
  }
}
} // namespace prot::engine
