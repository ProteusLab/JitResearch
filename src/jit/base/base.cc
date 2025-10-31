#include "prot/jit/base.hh"

#include <fmt/core.h>

#include <cassert>

extern "C" {
#include <sys/mman.h>
}

namespace prot::engine {
void JitEngine::step(CPUState &cpu) {
  while (!cpu.finished) {
    if (doJIT(cpu)) {
      continue;
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
        curAddr += isa::kWordSize;
      }
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
    if (found->second.num_exec >= kExecThreshold) {
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
