#ifndef PROT_ISA_HH_INCLUDED
#define PROT_ISA_HH_INCLUDED

#include <cstdint>
#include <limits>

namespace prot::isa {

consteval std::size_t toBits(std::size_t bytes) {
  return bytes * std::numeric_limits<unsigned char>::digits;
}

template <typename T> consteval std::size_t sizeofBits() {
  return toBits(sizeof(T));
}

// RISCV-32-I
using Icount = std::uintmax_t;
using DWord = std::uint64_t;
using Word = std::uint32_t;
using Addr = Word;
using Half = std::uint16_t;
using Byte = std::uint8_t;

enum class Opcode { kInvalid };

struct Instruction final {
  using enum Opcode;

  explicit Instruction(Opcode opc) : m_opc(opc) {}

  [[nodiscard]] auto opcode() const { return m_opc; }

  static Instruction decode(Word bytes) {
    return Instruction{Opcode::kInvalid};
  }

private:
  Opcode m_opc{Opcode::kInvalid};
};

} // namespace prot::isa

#endif // PROT_ISA_HH_INCLUDED
