#include <gtest/gtest.h>

#include "prot/isa.hh"

namespace prot::test {

TEST(decoder, LB) {
  prot::isa::Word word = 0b11110000111101010'000'11100'0000011;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kLB);
}

TEST(decoder, LH) { // xxxxxxxxxxxxxxxxx'001'xxxxx'0000011
  prot::isa::Word word = 0b11110000111101010'001'11100'0000011;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kLH);
}

TEST(decoder, LW) {
  prot::isa::Word word = 0b11110000111101010'010'11100'0000011;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kLW);
}

TEST(decoder, LBU) {
  prot::isa::Word word = 0b11110000111101010'100'11100'0000011;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kLBU);
}

TEST(decoder, LHU) {
  prot::isa::Word word = 0b11110000111101010'101'11100'0000011;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kLHU);
}

TEST(decoder, FENCE) {
  prot::isa::Word word = 0b11110000111101010'000'11100'0001111;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kFENCE);
}

TEST(decoder, ADDI) {
  prot::isa::Word word = 0b11110000111101010'000'11100'0010011;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kADDI);
}

TEST(decoder, SLTI) {
  prot::isa::Word word = 0b11110000111101010'010'11100'0010011;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kSLTI);
}

TEST(decoder, SLTIU) {
  prot::isa::Word word = 0b11110000111101010'011'11100'0010011;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kSLTIU);
}

TEST(decoder, XORI) {
  prot::isa::Word word = 0b11110000111101010'100'11100'0010011;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kXORI);
}

TEST(decoder, ORI) {
  prot::isa::Word word = 0b11110000111101010'110'11100'0010011;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kORI);
}

TEST(decoder, ANDI) {
  prot::isa::Word word = 0b11110000111101010'111'11100'0010011;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kANDI);
}

TEST(decoder, AUIPC) {
  prot::isa::Word word = 0b11110000111101010111110100010111;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kAUIPC);
}

TEST(decoder, SB) {
  prot::isa::Word word = 0b11110000111101010'000'11100'0100011;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kSB);
}

TEST(decoder, SH) {
  prot::isa::Word word = 0b11110000111101010'001'11100'0100011;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kSH);
}

TEST(decoder, SW) {
  prot::isa::Word word = 0b11110000111101010'010'11100'0100011;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kSW);
}

TEST(decoder, ADD) {
  prot::isa::Word word = 0b0000000'1010101010'000'11100'0110011;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kADD);
}

TEST(decoder, SLL) {
  prot::isa::Word word = 0b0000000'1010101010'001'11100'0110011;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kSLL);
}

TEST(decoder, SLT) {
  prot::isa::Word word = 0b0000000'1010101010'010'11100'0110011;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kSLT);
}

TEST(decoder, SLTU) {
  prot::isa::Word word = 0b0000000'1010101010'011'11100'0110011;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kSLTU);
}

TEST(decoder, XOR) {
  prot::isa::Word word = 0b0000000'1010101010'100'11100'0110011;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kXOR);
}

TEST(decoder, SRL) {
  prot::isa::Word word = 0b0000000'1010101010'101'11100'0110011;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kSRL);
}

TEST(decoder, AND) {
  prot::isa::Word word = 0b0000000'1010101010'111'11100'0110011;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kAND);
}

TEST(decoder, SUB) {
  prot::isa::Word word = 0b0100000'1010101010'000'11100'0110011;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kSUB);
}

TEST(decoder, SRA) {
  prot::isa::Word word = 0b0100000'1010101010'101'11100'0110011;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kSRA);
}

TEST(decoder, LUI) {
  prot::isa::Word word = 0b1111000011111110000001111'0110111;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kLUI);
}

TEST(decoder, BEQ) {
  prot::isa::Word word = 0b11110101010111111'000'11100'1100011;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kBEQ);
}

TEST(decoder, BNE) {
  prot::isa::Word word = 0b11110101010111111'001'11100'1100011;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kBNE);
}

TEST(decoder, BLT) {
  prot::isa::Word word = 0b11110101010111111'100'11100'1100011;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kBLT);
}

TEST(decoder, BGE) {
  prot::isa::Word word = 0b11110101010111111'101'11100'1100011;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kBGE);
}

TEST(decoder, BLTU) {
  prot::isa::Word word = 0b11110101010111111'110'11100'1100011;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kBLTU);
}

TEST(decoder, BGEU) {
  prot::isa::Word word = 0b11110101010111111'111'11100'1100011;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kBGEU);
}

TEST(decoder, JALR) {
  prot::isa::Word word = 0b11110101010111111'000'11100'1100111;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kJALR);
}

TEST(decoder, JAL) {
  prot::isa::Word word = 0b1111000011110000111100001'1101111;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kJAL);
}

// imm tests
//=---------

TEST(decoder, I_imm) {
  prot::isa::Word word = 0b11110000111101010'111'11100'0010011;
  //
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).imm(), 0b111111111111111111111'11100001111);
}

TEST(decoder, S_imm) {
  // 7th bit
  prot::isa::Word word = 0b0000000'0111101010'010'00001'0100011;
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).imm(), 0b01);
  // 11-8
  word = 0b0000000'0111101010'010'11110'0100011;
  instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).imm(), 0b1111'0);
  // 30-25
  word = 0b0111111'0111101010'010'00000'0100011;
  instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).imm(), 0b011111100000);
  // 31
  word = 0b1000000'0111101010'010'00000'0100011;
  instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).imm(),
            static_cast<int32_t>(0b111111111111111111111'00000000000));
}

TEST(decoder, B_imm) {
  // 7
  prot::isa::Word word = 0b0000000'1010111111'000'00001'1100011;
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).imm(), 0b0100000000000);
  // 11-8
  word = 0b0000000'0111101010'000'11110'1100011;
  instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).imm(), 0b1111'0);
  // 30-25
  word = 0b0111111'0111101010'000'00000'1100011;
  instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).imm(), 0b011111100000);
  // 31
  word = 0b1000000'0111101010'000'00000'1100011;
  instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).imm(),
            static_cast<int32_t>(0b11111111111111111111'000000000000));
}

TEST(decoder, U_imm) {
  // 31 - 12
  prot::isa::Word word = 0b11110000111101010111'11010'0010111;
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).imm(), 0b11110000111101010111'000000000000);
}

TEST(decoder, J_imm) {
  // 30-21
  prot::isa::Word word = 0b0'1110000111'0'00000000'00001'1101111;
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).imm(), 0b011100001110);
  // 20
  word = 0b0'0000000000'1'00000000'00001'1101111;
  instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).imm(), 0b1'00000000000);
  // 19-12
  word = 0b0'0000000000'0'10001111'00001'1101111;
  instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).imm(), 0b10001111'000000000000);
  // 31
  word = 0b1'0000000000'0'00000000'00001'1101111;
  instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).imm(),
            static_cast<int32_t>(0b111111111111'00000000000000000000));
}

TEST(decoder, ADDI_negimm) {
  prot::isa::Word word = 0b111111111111'10101'000'11001'0010011;
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kADDI);
  ASSERT_EQ((*instr).imm(), -1);
}

TEST(decoder, LUI_large_imm) {
  prot::isa::Word word = 0b11111111111111111111'11001'0110111;
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kLUI);
  ASSERT_EQ((*instr).imm(), 0xFFFFF000);
}

TEST(decoder, R_type_fields) {
  // ADD x5, x10, x21
  prot::isa::Word word = 0b0000000'10101'01010'000'00101'0110011;
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kADD);
  ASSERT_EQ((*instr).rd(), 5);
  ASSERT_EQ((*instr).rs1(), 10);
  ASSERT_EQ((*instr).rs2(), 21);
}

TEST(decoder, I_type_fields) {
  // ADDI x7, x15, 123
  prot::isa::Word word = 0b000001111011'01111'000'00111'0010011;
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kADDI);
  ASSERT_EQ((*instr).rd(), 7);
  ASSERT_EQ((*instr).rs1(), 15);
  ASSERT_EQ((*instr).imm(), 123);
}

TEST(decoder, S_type_fields) {
  // SW x12, offset(x20)
  prot::isa::Word word = 0b0000000'01100'10100'010'00000'0100011;
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kSW);
  ASSERT_EQ((*instr).rs1(), 20);
  ASSERT_EQ((*instr).rs2(), 12);
}

TEST(decoder, B_type_fields) {
  // BEQ x8, x9, offset
  prot::isa::Word word = 0b0000000'01001'01000'000'00000'1100011;
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kBEQ);
  ASSERT_EQ((*instr).rs1(), 8);
  ASSERT_EQ((*instr).rs2(), 9);
}

TEST(decoder, J_type_fields) {
  // JAL x1, offset
  prot::isa::Word word = 0b00000000000000000000'00001'1101111;
  auto instr = prot::isa::Instruction::decode(word);
  ASSERT_EQ((*instr).opcode(), prot::isa::Opcode::kJAL);
  ASSERT_EQ((*instr).rd(), 1);
}
} // namespace prot::test
