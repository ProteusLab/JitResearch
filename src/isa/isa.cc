#include "prot/isa.hh"

#include <string>
#include <unordered_map>

namespace prot::isa {

static std::unordered_map<Opcode, std::string> opc2str{

    {Opcode::kADD, "ADD"},
    {Opcode::kADDI, "ADDI"},
    {Opcode::kAND, "AND"},
    {Opcode::kANDI, "ANDI"},
    {Opcode::kAUIPC, "AUIPC"},
    {Opcode::kBEQ, "BEQ"},
    {Opcode::kBGE, "BGE"},
    {Opcode::kBGEU, "BGEU"},
    {Opcode::kBLT, "BLT"},
    {Opcode::kBLTU, "BLTU"},
    {Opcode::kBNE, "BNE"},
    {Opcode::kEBREAK, "EBREAK"},
    {Opcode::kECALL, "ECALL"},
    {Opcode::kFENCE, "FENCE"},
    {Opcode::kJAL, "JAL"},
    {Opcode::kJALR, "JALR"},
    {Opcode::kLB, "LB"},
    {Opcode::kLBU, "LBU"},
    {Opcode::kLH, "LH"},
    {Opcode::kLHU, "LHU"},
    {Opcode::kLUI, "LUI"},
    {Opcode::kLW, "LW"},
    {Opcode::kOR, "OR"},
    {Opcode::kORI, "ORI"},
    {Opcode::kPAUSE, "PAUSE"},
    {Opcode::kSB, "SB"},
    {Opcode::kSBREAK, "SBREAK"},
    {Opcode::kSCALL, "SCALL"},
    {Opcode::kSH, "SH"},
    {Opcode::kSLL, "SLL"},
    {Opcode::kSLLI, "SLLI"},
    {Opcode::kSLT, "SLT"},
    {Opcode::kSLTI, "SLTI"},
    {Opcode::kSLTIU, "SLTIU"},
    {Opcode::kSLTU, "SLTU"},
    {Opcode::kSRA, "SRA"},
    {Opcode::kSRAI, "SRAI"},
    {Opcode::kSRL, "SRL"},
    {Opcode::kSRLI, "SRLI"},
    {Opcode::kSUB, "SUB"},
    {Opcode::kSW, "SW"},
    {Opcode::kXOR, "XOR"},
    {Opcode::kXORI, "XORI"},
};

std::string strOpc(Opcode opc) {
    return opc2str[opc];
}

// Automatically Generated Decoder
Instruction Instruction::decode(Word word) {

  Instruction instr{};
  switch ((word >> 0) & 0b111000001111111) {
  case 0b11: {
    //! LB
    //! xxxxxxxxxxxxxxxxx000xxxxx0000011
    instr.m_opc = Opcode::kLB;
    instr.m_rd = std::bit_cast<int32_t>(slice<11, 7>(word) << 0) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<31, 31>(word) << 31) >> 20;
    instr.m_imm |= std::bit_cast<int32_t>(slice<30, 20>(word) << 0) >> 0;
    return instr;
  }
  case 0b1000000000011: {
    //! LH
    //! xxxxxxxxxxxxxxxxx001xxxxx0000011
    instr.m_opc = Opcode::kLH;
    instr.m_rd = std::bit_cast<int32_t>(slice<11, 7>(word) << 0) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<31, 31>(word) << 31) >> 20;
    instr.m_imm |= std::bit_cast<int32_t>(slice<30, 20>(word) << 0) >> 0;
    return instr;
  }
  case 0b10000000000011: {
    //! LW
    //! xxxxxxxxxxxxxxxxx010xxxxx0000011
    instr.m_opc = Opcode::kLW;
    instr.m_rd = std::bit_cast<int32_t>(slice<11, 7>(word) << 0) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<31, 31>(word) << 31) >> 20;
    instr.m_imm |= std::bit_cast<int32_t>(slice<30, 20>(word) << 0) >> 0;
    return instr;
  }
  case 0b100000000000011: {
    //! LBU
    //! xxxxxxxxxxxxxxxxx100xxxxx0000011
    instr.m_opc = Opcode::kLBU;
    instr.m_rd = std::bit_cast<int32_t>(slice<11, 7>(word) << 0) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<31, 31>(word) << 31) >> 20;
    instr.m_imm |= std::bit_cast<int32_t>(slice<30, 20>(word) << 0) >> 0;
    return instr;
  }
  case 0b101000000000011: {
    //! LHU
    //! xxxxxxxxxxxxxxxxx101xxxxx0000011
    instr.m_opc = Opcode::kLHU;
    instr.m_rd = std::bit_cast<int32_t>(slice<11, 7>(word) << 0) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<31, 31>(word) << 31) >> 20;
    instr.m_imm |= std::bit_cast<int32_t>(slice<30, 20>(word) << 0) >> 0;
    return instr;
  }
  case 0b1111: {
    //! FENCE
    //! xxxxxxxxxxxxxxxxx000xxxxx0001111
    instr.m_opc = Opcode::kFENCE;
    instr.m_imm |= std::bit_cast<int32_t>(slice<31, 28>(word) << 0) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<27, 24>(word) << 0) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<23, 20>(word) << 0) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    instr.m_rd = std::bit_cast<int32_t>(slice<11, 7>(word) << 0) >> 0;
    return instr;
  }
  case 0b10011: {
    //! ADDI
    //! xxxxxxxxxxxxxxxxx000xxxxx0010011
    instr.m_opc = Opcode::kADDI;
    instr.m_rd = std::bit_cast<int32_t>(slice<11, 7>(word) << 0) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<31, 31>(word) << 31) >> 20;
    instr.m_imm |= std::bit_cast<int32_t>(slice<30, 20>(word) << 0) >> 0;
    return instr;
  }
  case 0b1000000010011: {
    //! SLLI
    //! 0000000xxxxxxxxxx001xxxxx0010011
    instr.m_opc = Opcode::kSLLI;
    instr.m_rd = std::bit_cast<int32_t>(slice<11, 7>(word) << 0) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    return instr;
  }
  case 0b10000000010011: {
    //! SLTI
    //! xxxxxxxxxxxxxxxxx010xxxxx0010011
    instr.m_opc = Opcode::kSLTI;
    instr.m_rd = std::bit_cast<int32_t>(slice<11, 7>(word) << 0) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<31, 31>(word) << 31) >> 20;
    instr.m_imm |= std::bit_cast<int32_t>(slice<30, 20>(word) << 0) >> 0;
    return instr;
  }
  case 0b11000000010011: {
    //! SLTIU
    //! xxxxxxxxxxxxxxxxx011xxxxx0010011
    instr.m_opc = Opcode::kSLTIU;
    instr.m_rd = std::bit_cast<int32_t>(slice<11, 7>(word) << 0) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<31, 31>(word) << 31) >> 20;
    instr.m_imm |= std::bit_cast<int32_t>(slice<30, 20>(word) << 0) >> 0;
    return instr;
  }
  case 0b100000000010011: {
    //! XORI
    //! xxxxxxxxxxxxxxxxx100xxxxx0010011
    instr.m_opc = Opcode::kXORI;
    instr.m_rd = std::bit_cast<int32_t>(slice<11, 7>(word) << 0) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<31, 31>(word) << 31) >> 20;
    instr.m_imm |= std::bit_cast<int32_t>(slice<30, 20>(word) << 0) >> 0;
    return instr;
  }
  case 0b110000000010011: {
    //! ORI
    //! xxxxxxxxxxxxxxxxx110xxxxx0010011
    instr.m_opc = Opcode::kORI;
    instr.m_rd = std::bit_cast<int32_t>(slice<11, 7>(word) << 0) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<31, 31>(word) << 31) >> 20;
    instr.m_imm |= std::bit_cast<int32_t>(slice<30, 20>(word) << 0) >> 0;
    return instr;
  }
  case 0b111000000010011: {
    //! ANDI
    //! xxxxxxxxxxxxxxxxx111xxxxx0010011
    instr.m_opc = Opcode::kANDI;
    instr.m_rd = std::bit_cast<int32_t>(slice<11, 7>(word) << 0) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<31, 31>(word) << 31) >> 20;
    instr.m_imm |= std::bit_cast<int32_t>(slice<30, 20>(word) << 0) >> 0;
    return instr;
  }
  case 0b100011: {
    //! SB
    //! xxxxxxxxxxxxxxxxx000xxxxx0100011
    instr.m_opc = Opcode::kSB;
    instr.m_imm |= std::bit_cast<int32_t>(slice<31, 31>(word) << 31) >> 20;
    instr.m_imm |= std::bit_cast<int32_t>(slice<30, 25>(word) << 5) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    instr.m_rs2 = std::bit_cast<int32_t>(slice<24, 20>(word) << 0) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<11, 7>(word) << 0) >> 0;
    return instr;
  }
  case 0b1000000100011: {
    //! SH
    //! xxxxxxxxxxxxxxxxx001xxxxx0100011
    instr.m_opc = Opcode::kSH;
    instr.m_imm |= std::bit_cast<int32_t>(slice<31, 31>(word) << 31) >> 20;
    instr.m_imm |= std::bit_cast<int32_t>(slice<30, 25>(word) << 5) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    instr.m_rs2 = std::bit_cast<int32_t>(slice<24, 20>(word) << 0) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<11, 7>(word) << 0) >> 0;
    return instr;
  }
  case 0b10000000100011: {
    //! SW
    //! xxxxxxxxxxxxxxxxx010xxxxx0100011
    instr.m_opc = Opcode::kSW;
    instr.m_imm |= std::bit_cast<int32_t>(slice<31, 31>(word) << 31) >> 20;
    instr.m_imm |= std::bit_cast<int32_t>(slice<30, 25>(word) << 5) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    instr.m_rs2 = std::bit_cast<int32_t>(slice<24, 20>(word) << 0) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<11, 7>(word) << 0) >> 0;
    return instr;
  }
  case 0b1100011: {
    //! BEQ
    //! xxxxxxxxxxxxxxxxx000xxxxx1100011
    instr.m_opc = Opcode::kBEQ;
    instr.m_imm |= std::bit_cast<int32_t>(slice<31, 31>(word) << 31) >> 19;
    instr.m_imm |= std::bit_cast<int32_t>(slice<30, 25>(word) << 5) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    instr.m_rs2 = std::bit_cast<int32_t>(slice<24, 20>(word) << 0) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<11, 8>(word) << 1) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<7, 7>(word) << 11) >> 0;
    return instr;
  }
  case 0b1000001100011: {
    //! BNE
    //! xxxxxxxxxxxxxxxxx001xxxxx1100011
    instr.m_opc = Opcode::kBNE;
    instr.m_imm |= std::bit_cast<int32_t>(slice<31, 31>(word) << 31) >> 19;
    instr.m_imm |= std::bit_cast<int32_t>(slice<30, 25>(word) << 5) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    instr.m_rs2 = std::bit_cast<int32_t>(slice<24, 20>(word) << 0) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<11, 8>(word) << 1) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<7, 7>(word) << 11) >> 0;
    return instr;
  }
  case 0b100000001100011: {
    //! BLT
    //! xxxxxxxxxxxxxxxxx100xxxxx1100011
    instr.m_opc = Opcode::kBLT;
    instr.m_imm |= std::bit_cast<int32_t>(slice<31, 31>(word) << 31) >> 19;
    instr.m_imm |= std::bit_cast<int32_t>(slice<30, 25>(word) << 5) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    instr.m_rs2 = std::bit_cast<int32_t>(slice<24, 20>(word) << 0) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<11, 8>(word) << 1) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<7, 7>(word) << 11) >> 0;
    return instr;
  }
  case 0b101000001100011: {
    //! BGE
    //! xxxxxxxxxxxxxxxxx101xxxxx1100011
    instr.m_opc = Opcode::kBGE;
    instr.m_imm |= std::bit_cast<int32_t>(slice<31, 31>(word) << 31) >> 19;
    instr.m_imm |= std::bit_cast<int32_t>(slice<30, 25>(word) << 5) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    instr.m_rs2 = std::bit_cast<int32_t>(slice<24, 20>(word) << 0) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<11, 8>(word) << 1) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<7, 7>(word) << 11) >> 0;
    return instr;
  }
  case 0b110000001100011: {
    //! BLTU
    //! xxxxxxxxxxxxxxxxx110xxxxx1100011
    instr.m_opc = Opcode::kBLTU;
    instr.m_imm |= std::bit_cast<int32_t>(slice<31, 31>(word) << 31) >> 19;
    instr.m_imm |= std::bit_cast<int32_t>(slice<30, 25>(word) << 5) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    instr.m_rs2 = std::bit_cast<int32_t>(slice<24, 20>(word) << 0) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<11, 8>(word) << 1) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<7, 7>(word) << 11) >> 0;
    return instr;
  }
  case 0b111000001100011: {
    //! BGEU
    //! xxxxxxxxxxxxxxxxx111xxxxx1100011
    instr.m_opc = Opcode::kBGEU;
    instr.m_imm |= std::bit_cast<int32_t>(slice<31, 31>(word) << 31) >> 19;
    instr.m_imm |= std::bit_cast<int32_t>(slice<30, 25>(word) << 5) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    instr.m_rs2 = std::bit_cast<int32_t>(slice<24, 20>(word) << 0) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<11, 8>(word) << 1) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<7, 7>(word) << 11) >> 0;
    return instr;
  }
  }
  switch ((word >> 0) & 0b11111110000000000111000001111111) {
  case 0b101000000010011: {
    //! SRLI
    //! 0000000xxxxxxxxxx101xxxxx0010011
    instr.m_opc = Opcode::kSRLI;
    instr.m_rd = std::bit_cast<int32_t>(slice<11, 7>(word) << 0) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    return instr;
  }
  case 0b1000000000000000101000000010011: {
    //! SRAI
    //! 0100000xxxxxxxxxx101xxxxx0010011
    instr.m_opc = Opcode::kSRAI;
    instr.m_rd = std::bit_cast<int32_t>(slice<11, 7>(word) << 0) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    return instr;
  }
  case 0b110011: {
    //! ADD
    //! 0000000xxxxxxxxxx000xxxxx0110011
    instr.m_opc = Opcode::kADD;
    instr.m_rd = std::bit_cast<int32_t>(slice<11, 7>(word) << 0) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    instr.m_rs2 = std::bit_cast<int32_t>(slice<24, 20>(word) << 0) >> 0;
    return instr;
  }
  case 0b1000000110011: {
    //! SLL
    //! 0000000xxxxxxxxxx001xxxxx0110011
    instr.m_opc = Opcode::kSLL;
    instr.m_rd = std::bit_cast<int32_t>(slice<11, 7>(word) << 0) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    instr.m_rs2 = std::bit_cast<int32_t>(slice<24, 20>(word) << 0) >> 0;
    return instr;
  }
  case 0b10000000110011: {
    //! SLT
    //! 0000000xxxxxxxxxx010xxxxx0110011
    instr.m_opc = Opcode::kSLT;
    instr.m_rd = std::bit_cast<int32_t>(slice<11, 7>(word) << 0) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    instr.m_rs2 = std::bit_cast<int32_t>(slice<24, 20>(word) << 0) >> 0;
    return instr;
  }
  case 0b11000000110011: {
    //! SLTU
    //! 0000000xxxxxxxxxx011xxxxx0110011
    instr.m_opc = Opcode::kSLTU;
    instr.m_rd = std::bit_cast<int32_t>(slice<11, 7>(word) << 0) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    instr.m_rs2 = std::bit_cast<int32_t>(slice<24, 20>(word) << 0) >> 0;
    return instr;
  }
  case 0b100000000110011: {
    //! XOR
    //! 0000000xxxxxxxxxx100xxxxx0110011
    instr.m_opc = Opcode::kXOR;
    instr.m_rd = std::bit_cast<int32_t>(slice<11, 7>(word) << 0) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    instr.m_rs2 = std::bit_cast<int32_t>(slice<24, 20>(word) << 0) >> 0;
    return instr;
  }
  case 0b101000000110011: {
    //! SRL
    //! 0000000xxxxxxxxxx101xxxxx0110011
    instr.m_opc = Opcode::kSRL;
    instr.m_rd = std::bit_cast<int32_t>(slice<11, 7>(word) << 0) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    instr.m_rs2 = std::bit_cast<int32_t>(slice<24, 20>(word) << 0) >> 0;
    return instr;
  }
  case 0b110000000110011: {
    //! OR
    //! 0000000xxxxxxxxxx110xxxxx0110011
    instr.m_opc = Opcode::kOR;
    instr.m_rd = std::bit_cast<int32_t>(slice<11, 7>(word) << 0) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    instr.m_rs2 = std::bit_cast<int32_t>(slice<24, 20>(word) << 0) >> 0;
    return instr;
  }
  case 0b111000000110011: {
    //! AND
    //! 0000000xxxxxxxxxx111xxxxx0110011
    instr.m_opc = Opcode::kAND;
    instr.m_rd = std::bit_cast<int32_t>(slice<11, 7>(word) << 0) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    instr.m_rs2 = std::bit_cast<int32_t>(slice<24, 20>(word) << 0) >> 0;
    return instr;
  }
  case 0b1000000000000000000000000110011: {
    //! SUB
    //! 0100000xxxxxxxxxx000xxxxx0110011
    instr.m_opc = Opcode::kSUB;
    instr.m_rd = std::bit_cast<int32_t>(slice<11, 7>(word) << 0) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    instr.m_rs2 = std::bit_cast<int32_t>(slice<24, 20>(word) << 0) >> 0;
    return instr;
  }
  case 0b1000000000000000101000000110011: {
    //! SRA
    //! 0100000xxxxxxxxxx101xxxxx0110011
    instr.m_opc = Opcode::kSRA;
    instr.m_rd = std::bit_cast<int32_t>(slice<11, 7>(word) << 0) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    instr.m_rs2 = std::bit_cast<int32_t>(slice<24, 20>(word) << 0) >> 0;
    return instr;
  }
  }
  switch ((word >> 0) & 0b1111111) {
  case 0b10111: {
    //! AUIPC
    //! xxxxxxxxxxxxxxxxxxxxxxxxx0010111
    instr.m_opc = Opcode::kAUIPC;
    instr.m_rd = std::bit_cast<int32_t>(slice<11, 7>(word) << 0) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<31, 12>(word) << 12) >> 0;
    return instr;
  }
  case 0b110111: {
    //! LUI
    //! xxxxxxxxxxxxxxxxxxxxxxxxx0110111
    instr.m_opc = Opcode::kLUI;
    instr.m_rd = std::bit_cast<int32_t>(slice<11, 7>(word) << 0) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<31, 12>(word) << 12) >> 0;
    return instr;
  }
  case 0b1100111: {
    //! JALR
    //! xxxxxxxxxxxxxxxxx000xxxxx1100111
    instr.m_opc = Opcode::kJALR;
    instr.m_rd = std::bit_cast<int32_t>(slice<11, 7>(word) << 0) >> 0;
    instr.m_rs1 = std::bit_cast<int32_t>(slice<19, 15>(word) << 0) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<31, 31>(word) << 31) >> 20;
    instr.m_imm |= std::bit_cast<int32_t>(slice<30, 20>(word) << 0) >> 0;
    return instr;
  }
  case 0b1101111: {
    //! JAL
    //! xxxxxxxxxxxxxxxxxxxxxxxxx1101111
    instr.m_opc = Opcode::kJAL;
    instr.m_rd = std::bit_cast<int32_t>(slice<11, 7>(word) << 0) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<31, 31>(word) << 31) >> 11;
    instr.m_imm |= std::bit_cast<int32_t>(slice<30, 21>(word) << 1) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<20, 20>(word) << 11) >> 0;
    instr.m_imm |= std::bit_cast<int32_t>(slice<19, 12>(word) << 12) >> 0;
    return instr;
  }
  }
  switch ((word >> 0) & 0b11111111111111111111111111111111) {
  case 0b1110011: {
    //! ECALL
    //! 00000000000000000000000001110011
    instr.m_opc = Opcode::kECALL;
    return instr;
  }
  case 0b100000000000001110011: {
    //! EBREAK
    //! 00000000000100000000000001110011
    instr.m_opc = Opcode::kEBREAK;
    return instr;
  }
  }

  return Instruction{Opcode::kInvalid};
}
} // namespace prot::isa
