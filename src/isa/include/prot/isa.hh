#ifndef PROT_ISA_HH_INCLUDED
#define PROT_ISA_HH_INCLUDED

#include <cstdint>
#include <limits>

#include <bit>
#include <optional>
#include <stdexcept>

static_assert(std::endian::native == std::endian::little,
              "Sorry, little endian machines are only supported");
static_assert(-1U == ~0U, "Sorry, two's complement is required");

namespace prot {
consteval std::size_t toBits(std::size_t bytes) {
  return bytes * std::numeric_limits<unsigned char>::digits;
}

template <typename T> consteval std::size_t sizeofBits() {
  return toBits(sizeof(T));
}

template <typename T>
constexpr auto toUnderlying(T val)
  requires std::is_enum_v<T>
{
  return static_cast<std::underlying_type_t<T>>(val);
}

namespace isa {

consteval std::uint32_t ones(std::size_t Num) {
  if (Num == sizeofBits<std::uint32_t>()) {
    return ~std::uint32_t{};
  }
  return (std::uint32_t{1} << Num) - std::uint32_t{1};
}

consteval std::uint32_t getMask(std::size_t Msb, std::size_t Lsb) {
  return ones(Msb - Lsb + 1) << Lsb;
}

template <std::size_t Msb, std::size_t Lsb>
constexpr std::uint32_t slice(std::uint32_t word) {
  static_assert(Msb >= Lsb, "Error : illegal bits range");
  return (word & getMask(Msb, Lsb)) >> Lsb;
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

constexpr bool signedLess(isa::Word lhs, isa::Word rhs) {
  using Signed = std::make_signed_t<isa::Word>;

  return static_cast<Signed>(lhs) < static_cast<Signed>(rhs);
}

template <std::size_t NewSize, std::size_t OldSize>
constexpr Word signExtend(Word val) {
  static_assert(NewSize >= OldSize, "Trying to sign extend to smaller size");
  static_assert(OldSize > 0, "Initial size must be non-zero");
  static_assert(NewSize <= sizeofBits<isa::Word>(),
                "newSize is out of bits range");

  if constexpr (NewSize == OldSize) {
    return val;
  } else {
    Word zeroed = slice<OldSize - 1, 0>(val);
    constexpr Word mask = static_cast<Word>(1) << (OldSize - 1);
    Word res = (zeroed ^ mask) - mask;

    return slice<NewSize - 1, 0>(res);
  }
}

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

  kNumOpcodes,
};

constexpr bool isTerminator(Opcode opc) {
  if (opc == Opcode::kNumOpcodes) {
    throw std::invalid_argument{"Unexpected opcode"};
  }
  switch (opc) {
  case Opcode::kBEQ:
  case Opcode::kBGE:
  case Opcode::kBGEU:
  case Opcode::kBLT:
  case Opcode::kBLTU:
  case Opcode::kBNE:
  case Opcode::kEBREAK:
  case Opcode::kECALL:
  case Opcode::kFENCE:
  case Opcode::kJAL:
  case Opcode::kJALR:
    return true;
  default:
    break;
  }
  return false;
}

constexpr bool changesPC(Opcode opc) {
  if (opc == Opcode::kNumOpcodes) {
    throw std::invalid_argument{"Unexpected opcode"};
  }
  switch (opc) {
  case Opcode::kBEQ:
  case Opcode::kBGE:
  case Opcode::kBGEU:
  case Opcode::kBLT:
  case Opcode::kBLTU:
  case Opcode::kBNE:
  case Opcode::kJAL:
  case Opcode::kJALR:
    return true;
  default:
    return false;
  }
}

struct Instruction final {
  using enum Opcode;

  explicit Instruction(Opcode opc) : m_opc(opc) {}

  [[nodiscard]] auto opcode() const { return m_opc; }

  static std::optional<Instruction> decode(Word word);

  [[nodiscard]] Operand rd() const { return m_rd; }
  [[nodiscard]] Operand rs1() const { return m_rs1; }
  [[nodiscard]] Operand rs2() const { return m_rs2; }
  [[nodiscard]] Imm imm() const { return m_imm; }

private:
  Instruction() = default;

  Opcode m_opc{};

  Operand m_rs1{};
  Operand m_rs2{};
  Operand m_rd{};

  Imm m_imm{};
};

} // namespace isa
} // namespace prot
#endif // PROT_ISA_HH_INCLUDED
