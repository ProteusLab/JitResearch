#ifndef PROT_MEMORY_HH_INCLUDED
#define PROT_MEMORY_HH_INCLUDED

#include <array>
#include <span>

#include "prot/isa.hh"

namespace prot {
struct Memory {
  Memory(const Memory &) = delete;
  Memory &operator=(const Memory &) = delete;
  Memory(Memory &&) = delete;
  Memory &operator=(Memory &&) = delete;
  virtual ~Memory() = default;

  virtual void writeBlock(std::span<const std::byte> src, isa::Addr addr) = 0;
  virtual void readBlock(isa::Addr addr, std::span<std::byte> dest) const = 0;

  template <std::unsigned_integral T>
  [[nodiscard]] T read(isa::Addr addr) const {
    std::array<std::byte, sizeof(T)> buf;
    readBlock(addr, buf);
    return std::bit_cast<T>(buf);
  }

  template <std::unsigned_integral T> void write(isa::Addr addr, T val) {
    const auto &buf = std::bit_cast<std::array<std::byte, sizeof(T)>>(val);
    writeBlock(buf, addr);
  }
};
} // namespace prot

#endif // PROT_MEMORY_HH_INCLUDED
