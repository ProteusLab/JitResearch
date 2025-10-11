#include "prot/isa.hh"
#include "prot/memory.hh"

#include <fmt/core.h>

#include <algorithm>
#include <cassert>
#include <vector>

namespace prot::memory {
namespace {
class PlainMemory : public Memory {
public:
  explicit PlainMemory(std::size_t size, isa::Addr start)
      : m_data(size), m_start(start) {
    if (m_data.size() + m_start < m_start) {
      throw std::invalid_argument{
          fmt::format("Size {} or start addr {:#x} is too high", size, start)};
    }
  }

  void allocateBlock(isa::Addr /*addr*/, std::size_t /*size*/,
                     Perms /*perms*/) override {
    // Do nothing for now
  }

  void writeBlock(std::span<const std::byte> src, isa::Addr addr) override {
    checkRange(addr, src.size());
    std::ranges::copy(src, translateAddr(addr));
  }

  void readBlock(isa::Addr addr, std::span<std::byte> dest) const override {
    checkRange(addr, dest.size());
    std::ranges::copy_n(translateAddr(addr), dest.size(), dest.begin());
  }

private:
  [[nodiscard]] std::size_t addrToOffset(isa::Addr addr) const {
    assert(addr >= m_start && addr <= m_start + m_data.size());
    return addr - m_start;
  }

  [[nodiscard]] std::byte *translateAddr(isa::Addr addr) {
    return m_data.data() + addrToOffset(addr);
  }

  [[nodiscard]] const std::byte *translateAddr(isa::Addr addr) const {
    return m_data.data() + addrToOffset(addr);
  }

  void checkRange(isa::Addr addr, std::size_t size) const {
    assert(addr + size >= addr);
    if (addr < m_start) {
      throw std::invalid_argument{
          fmt::format("Addr {:#x} is too low (start: {:#x})", addr, m_start)};
    }

    if (addr + size > m_data.size() + m_start) {
      throw std::invalid_argument{
          fmt::format("Addr {:#x} is too high (end: {:#x})", addr,
                      m_start + m_data.size())};
    }
  }

  std::vector<std::byte> m_data;
  isa::Addr m_start{};
};
} // namespace

std::unique_ptr<Memory> makePlain(std::size_t size, isa::Addr start /* = 0 */) {
  return std::make_unique<PlainMemory>(size, start);
}
} // namespace prot::memory
