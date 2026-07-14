#include "prot/jit/base.hh"

#include <cassert>
#include <filesystem>
#include <iostream>

#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <sys/mman.h>
#include <unistd.h>

#include <fmt/core.h>
#include <fmt/ostream.h>
#include <fmt/std.h>

namespace prot::engine {
namespace {

int openPerfCounter() {
  struct perf_event_attr attr = {};
  attr.type = PERF_TYPE_HARDWARE;
  attr.config = PERF_COUNT_HW_CPU_CYCLES;
  attr.size = sizeof(attr);
  attr.pinned = 1;
  attr.exclude_kernel = 1;
  attr.disabled = 1;
  return static_cast<int>(syscall(__NR_perf_event_open, &attr, 0, -1, -1, 0));
}

std::uintmax_t readPerfCounter(int fd) {
  std::uint64_t val = 0;
  if (fd >= 0)
    ::read(fd, &val, sizeof(val));
  return val;
}

} // namespace

void JitEngine::step(CPUState &cpu) {
  if (m_perfExec < 0) {
    m_perfExec = openPerfCounter();
    m_perfTrans = openPerfCounter();
    m_perfInterp = openPerfCounter();
  }
  if (m_perfExec >= 0)
    ioctl(m_perfExec, PERF_EVENT_IOC_ENABLE, 0);
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
      if (m_perfExec >= 0)
        ioctl(m_perfExec, PERF_EVENT_IOC_DISABLE, 0);
      if (m_perfTrans >= 0)
        ioctl(m_perfTrans, PERF_EVENT_IOC_ENABLE, 0);
      auto code = m_translator->translate(bbIt->second);
      if (m_perfTrans >= 0)
        ioctl(m_perfTrans, PERF_EVENT_IOC_DISABLE, 0);
      if (m_perfExec >= 0)
        ioctl(m_perfExec, PERF_EVENT_IOC_ENABLE, 0);
      if (code == nullptr) [[unlikely]] {
        throw std::runtime_error{
            fmt::format("Failed to translate BB on pc: {:#x}", pc)};
      }

      code(cpu);
      m_tbCache.insert(pc, code);
      continue;
    }

    if (m_perfExec >= 0)
      ioctl(m_perfExec, PERF_EVENT_IOC_DISABLE, 0);
    if (m_perfInterp >= 0)
      ioctl(m_perfInterp, PERF_EVENT_IOC_ENABLE, 0);
    interpret(cpu, bbIt->second);
    if (m_perfInterp >= 0)
      ioctl(m_perfInterp, PERF_EVENT_IOC_DISABLE, 0);
    if (m_perfExec >= 0)
      ioctl(m_perfExec, PERF_EVENT_IOC_ENABLE, 0);
  }
  if (m_perfExec >= 0)
    ioctl(m_perfExec, PERF_EVENT_IOC_DISABLE, 0);
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

JitEngine::~JitEngine() {
  m_execTicks = readPerfCounter(m_perfExec);
  m_transTicks = readPerfCounter(m_perfTrans);
  m_interpTicks = readPerfCounter(m_perfInterp);
  if (m_perfExec >= 0) {
    close(m_perfExec);
    close(m_perfTrans);
    close(m_perfInterp);
  }
  std::filesystem::path jsonPath =
      std::filesystem::current_path() / m_config.statsFile;
  std::ofstream json(jsonPath);
  fmt::println(json, R"(
{{
    "exec_ticks": {},
    "translate_ticks": {},
    "interp_ticks": {}
}}
)",
               m_execTicks, m_transTicks, m_interpTicks);
  if (json) {
    fmt::println(std::cerr, "Stats were written to file: {}", jsonPath);
  }
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
