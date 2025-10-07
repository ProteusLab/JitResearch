#ifndef PROT_ISA_HH_INCLUDED
#define PROT_ISA_HH_INCLUDED

#include <cstdint>
#include <limits>

#include <bit>
#include <bitset>
#include <optional>

namespace prot::isa {

consteval std::size_t toBits(std::size_t bytes) {
  return bytes * std::numeric_limits<unsigned char>::digits;
}

template <typename T> consteval std::size_t sizeofBits() {
  return toBits(sizeof(T));
}

constexpr std::uint32_t ones(uint32_t num) { return (1 << num) - 1; }

template <std::size_t Msb, std::size_t Lsb> constexpr std::uint32_t getMask() {
  static_assert(Msb >= Lsb, "Error : illegal bits range");
  return ones(Msb - Lsb + 1) << Lsb;
}

template <std::size_t Msb, std::size_t Lsb>
constexpr std::uint32_t slice(std::uint32_t word) {
  static_assert(Msb >= Lsb, "Error : illegal bits range");
  return (word & getMask<Msb, Lsb>()) >> Lsb;
}

// RISCV-32-I
using Icount = std::uintmax_t;
using DWord = std::uint64_t;
using Word = std::uint32_t;
using Addr = Word;
using Half = std::uint16_t;
using Byte = std::uint8_t;
using Imm = std::int32_t;
using Operand = std::uint8_t;

enum class Opcode : uint16_t {
  kADD,
  kADDI,
  kAND,
  kANDI,
  kAUIPC,
  kBEQ,
  kBGE,
  kBGEU,
  kBLT,
  kBLTU,
  kBNE,
  kEBREAK,
  kECALL,
  kFENCE,
  kJAL,
  kJALR,
  kLB,
  kLBU,
  kLH,
  kLHU,
  kLUI,
  kLW,
  kOR,
  kORI,
  kPAUSE,
  kSB,
  kSBREAK,
  kSCALL,
  kSH,
  kSLL,
  kSLLI,
  kSLT,
  kSLTI,
  kSLTIU,
  kSLTU,
  kSRA,
  kSRAI,
  kSRL,
  kSRLI,
  kSUB,
  kSW,
  kXOR,
  kXORI,
};

struct Instruction final {
  using enum Opcode;

  explicit Instruction(Opcode opc) : m_opc(opc) {}

  [[nodiscard]] auto opcode() const { return m_opc; }

  static std::optional<Instruction> decode(Word word);

private:
  Instruction() = default;

  Opcode m_opc{};

  Operand m_rs1{};
  Operand m_rs2{};
  Operand m_rd{};

  Imm m_imm{};
};

} // namespace prot::isa

#endif // PROT_ISA_HH_INCLUDED
