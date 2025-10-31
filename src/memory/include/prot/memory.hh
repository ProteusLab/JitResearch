#ifndef PROT_MEMORY_HH_INCLUDED
#define PROT_MEMORY_HH_INCLUDED

#include <array>
#include <memory>
#include <span>

#include "prot/isa.hh"

namespace prot {
struct Memory {

  Memory() = default;
  Memory(const Memory &) = delete;
  Memory &operator=(const Memory &) = delete;
  Memory(Memory &&) = delete;
  Memory &operator=(Memory &&) = delete;
  virtual ~Memory() = default;

  virtual void writeBlock(std::span<const std::byte> src, isa::Addr addr) = 0;
  virtual void readBlock(isa::Addr addr, std::span<std::byte> dest) const = 0;

  void fillBlock(isa::Addr addr, std::byte value, std::size_t count) {
    for (std::size_t i = 0; i < count; ++i) {
      writeBlock({&value, 1}, addr + i);
    }
  }

  template <std::unsigned_integral T>
  [[nodiscard]] T read(isa::Addr addr) const {
    if constexpr (std::same_as<T, std::uint8_t>) {
      return read8(addr);
    }
    if constexpr (std::same_as<T, std::uint16_t>) {
      return read16(addr);
    }
    if constexpr (std::same_as<T, std::uint32_t>) {
      return read32(addr);
    }

    std::array<std::byte, sizeof(T)> buf;
    readBlock(addr, buf);
    return std::bit_cast<T>(buf);
  }

  template <std::unsigned_integral T> void write(isa::Addr addr, T val) {
    if constexpr (std::same_as<T, std::uint8_t>) {
      return write8(addr, val);
    }
    if constexpr (std::same_as<T, std::uint16_t>) {
      return write16(addr, val);
    }
    if constexpr (std::same_as<T, std::uint32_t>) {
      return write32(addr, val);
    }
    const auto &buf = std::bit_cast<std::array<std::byte, sizeof(T)>>(val);
    writeBlock(buf, addr);
  }

  virtual std::uint8_t read8(isa::Addr addr) const {
    return read<std::uint8_t>(addr);
  }
  virtual std::uint16_t read16(isa::Addr addr) const {
    return read<std::uint16_t>(addr);
  }
  virtual std::uint32_t read32(isa::Addr addr) const {
    return read<std::uint32_t>(addr);
  }
  virtual void write8(isa::Addr addr, std::uint8_t val) {
    write<std::uint8_t>(addr, val);
  }
  virtual void write16(isa::Addr addr, std::uint16_t val) {
    write<std::uint16_t>(addr, val);
  }
  virtual void write32(isa::Addr addr, std::uint32_t val) {
    write<std::uint32_t>(addr, val);
  }
};

namespace memory {
std::unique_ptr<Memory> makePlain(std::size_t size, isa::Addr start = 0);
std::unique_ptr<Memory> makePaged(std::size_t pageBits);
} // namespace memory
} // namespace prot

#endif // PROT_MEMORY_HH_INCLUDED
