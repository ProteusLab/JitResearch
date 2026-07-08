#include "prot/jit/base.hh"

#include <fmt/core.h>
#include <fmt/ostream.h>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <iostream>

extern "C" {
#include <sys/mman.h>
}

extern "C" prot::engine::JitFunction tbCacheLookup(
    const prot::engine::JitEngine::TbCache *cache,
    std::uint32_t gpa) {
  return cache->lookup(gpa);
}

namespace prot::engine {

JitFunction JitEngine::glueBlock(JitFunction originalFunc) {
  constexpr std::size_t pcOffset = offsetof(CPUState, pc);
  constexpr std::size_t finishedOffset = offsetof(CPUState, finished);
  const auto originalAddr = reinterpret_cast<std::uint64_t>(originalFunc);
  const auto cacheAddr = reinterpret_cast<std::uint64_t>(&m_tbCache);
  const auto lookupAddr = reinterpret_cast<std::uint64_t>(&tbCacheLookup);

  // --- layout (total: 65 bytes) ---
  // [0]           push rdi                           (1 B)
  // [1]           mov rax, originalFunc              (10 B)
  // [11]          call rax                           (2 B)
  // [13]          pop rdi                            (1 B)
  // [14]          test byte [rdi + finished], 0xFF   (7 B)
  // [21]          jne .ret                           (2 B)
  // [23]          mov esi, [rdi + pc]                (6 B)
  // [29]          push rdi                           (1 B)
  // [30]          mov rdi, cacheAddr                 (10 B)
  // [40]          mov rax, lookupAddr                (10 B)
  // [50]          call rax                           (2 B)
  // [52]          test rax, rax                      (3 B)
  // [55]          jz .cleanup                        (2 B)
  // [57]          pop rdi                            (1 B)
  // [58]          jmp rax                            (2 B)
  // [60] .cleanup: add rsp, 8                        (4 B)
  // [64] .ret:    ret                                (1 B)

  constexpr std::size_t totalSize = 65;
  std::vector<std::byte> code(totalSize);

  auto emit8 = [&](std::size_t off, std::uint8_t v) {
    code[off] = static_cast<std::byte>(v);
  };
  auto emit32 = [&](std::size_t off, std::uint32_t v) {
    emit8(off + 0, static_cast<std::uint8_t>(v & 0xFF));
    emit8(off + 1, static_cast<std::uint8_t>((v >> 8) & 0xFF));
    emit8(off + 2, static_cast<std::uint8_t>((v >> 16) & 0xFF));
    emit8(off + 3, static_cast<std::uint8_t>((v >> 24) & 0xFF));
  };
  auto emit64 = [&](std::size_t off, std::uint64_t v) {
    emit32(off + 0, static_cast<std::uint32_t>(v));
    emit32(off + 4, static_cast<std::uint32_t>(v >> 32));
  };

  // [0]: push rdi    ; save &cpu
  emit8(0, 0x57);

  // [1]: mov rax, originalFunc
  emit8(1, 0x48);
  emit8(2, 0xB8);
  emit64(3, originalAddr);

  // [11]: call rax   ; call original function (uses standard prologue/epilogue)
  emit8(11, 0xFF);
  emit8(12, 0xD0);

  // [13]: pop rdi    ; restore &cpu (original may have clobbered rdi)
  emit8(13, 0x5F);

  // [14]: test byte [rdi + finishedOffset], 0xFF  ; check cpu.finished
  emit8(14, 0xF6);
  emit8(15, 0x87);
  emit32(16, static_cast<std::uint32_t>(finishedOffset));
  emit8(20, 0xFF);

  // [21]: jne .ret   ; if finished, skip chain and return to step
  emit8(21, 0x75);
  emit8(22, static_cast<std::uint8_t>(64 - 23)); // rel8 = 41

  // [23]: mov esi, [rdi + pcOffset]  ; 2nd arg for tbCacheLookup
  emit8(23, 0x8B);
  emit8(24, 0xB7);
  emit32(25, static_cast<std::uint32_t>(pcOffset));

  // [29]: push rdi   ; re-save &cpu
  emit8(29, 0x57);

  // [30]: mov rdi, cacheAddr  ; 1st arg for tbCacheLookup
  emit8(30, 0x48);
  emit8(31, 0xBF);
  emit64(32, cacheAddr);

  // [40]: mov rax, lookupAddr
  emit8(40, 0x48);
  emit8(41, 0xB8);
  emit64(42, lookupAddr);

  // [50]: call rax   ; tbCacheLookup(cache, cpu.pc)
  emit8(50, 0xFF);
  emit8(51, 0xD0);

  // [52]: test rax, rax
  emit8(52, 0x48);
  emit8(53, 0x85);
  emit8(54, 0xC0);

  // [55]: jz .cleanup ; if not found, discard &cpu and return
  emit8(55, 0x74);
  emit8(56, static_cast<std::uint8_t>(60 - 57)); // rel8 = 3

  // [57]: pop rdi    ; restore &cpu for next block
  emit8(57, 0x5F);

  // [58]: jmp rax    ; tail-call to next block
  emit8(58, 0xFF);
  emit8(59, 0xE0);

  // [60]: .cleanup: add rsp, 8  ; discard saved &cpu from above
  emit8(60, 0x48);
  emit8(61, 0x83);
  emit8(62, 0xC4);
  emit8(63, 0x08);

  // [64]: .ret: ret  ; return to step function
  emit8(64, 0xC3);

  auto holder = CodeHolder(std::span<const std::byte>(code));
  auto func = holder.as<JitFunction>();
  m_glueBlocks.push_back(std::move(holder));
  return func;
}

void JitEngine::step(CPUState &cpu) {
  while (!cpu.finished) [[likely]] {
    if (m_config.enableDump) {
      cpu.dump(std::cout);
    }

    // colllect bb
    const auto pc = cpu.getPC();
    if (m_translator) {
      if (const auto found = m_tbCache.lookup(pc); found != nullptr)
          [[likely]] {
        found(cpu);
        continue;
      }
    }

    auto [bbIt, wasNew] = m_cacheBB.try_emplace(pc);
    if (wasNew) [[unlikely]] {
      auto curAddr = bbIt->first;
      auto &bb = bbIt->second;

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
      auto code = m_translator->translate(bbIt->second);
      if (code == nullptr) [[unlikely]] {
        throw std::runtime_error{
            fmt::format("Failed to translate BB on pc: {:#x}", pc)};
      }

      code(cpu);
      m_tbCache.insert(pc, glueBlock(code));
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
