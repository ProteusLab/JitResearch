#include "prot/page_memory.hh"
#include <gtest/gtest.h>

class PageMemTest : public ::testing::Test {
public:
  static constexpr std::uint64_t kPageBits = 12;
  static constexpr std::uint64_t kAddrBits = 16;

protected:
  // RAM -- 64kb, Page -- 4KB
  prot::PageMemory<prot::PageMemConfig<kPageBits, kAddrBits>> m_mem{};
};

TEST_F(PageMemTest, writeBlockTest) {
  const std::byte test_data[] = {std::byte{0xde}, std::byte{0xad},
                                 std::byte{0xbe}, std::byte{0xef}};

  std::byte read_data[sizeof(test_data)];

  constexpr prot::isa::Addr addr = 42;

  m_mem.writeBlock(test_data, addr);
  m_mem.readBlock(addr, read_data);

  EXPECT_TRUE(std::equal(std::begin(test_data), std::end(test_data),
                         std::begin(read_data)));
}

TEST_F(PageMemTest, writeBlockTest2) {
  const std::byte test_data[] = {std::byte{0xde}, std::byte{0xad},
                                 std::byte{0xbe}, std::byte{0xef}};

  constexpr prot::isa::Addr addr = 42;

  m_mem.writeBlock(test_data, addr);

  EXPECT_EQ(m_mem.read<prot::isa::Byte>(addr), 0xde);
  EXPECT_EQ(m_mem.read<prot::isa::Half>(addr), 0xadde);
  EXPECT_EQ(m_mem.read<prot::isa::Word>(addr), 0xefbeadde);
}

TEST_F(PageMemTest, writeBlockTest3) {
  constexpr prot::isa::Addr addr = 42;

  m_mem.write<prot::isa::Byte>(addr + 0, 0xde);
  m_mem.write<prot::isa::Byte>(addr + 1, 0xad);
  m_mem.write<prot::isa::Byte>(addr + 2, 0xef);
  m_mem.write<prot::isa::Byte>(addr + 3, 0xef);

  EXPECT_EQ(m_mem.read<prot::isa::Byte>(addr + 0), 0xde);
  EXPECT_EQ(m_mem.read<prot::isa::Byte>(addr + 1), 0xad);
  EXPECT_EQ(m_mem.read<prot::isa::Byte>(addr + 2), 0xef);
  EXPECT_EQ(m_mem.read<prot::isa::Byte>(addr + 3), 0xef);
}

TEST_F(PageMemTest, crossPageBoundaryTest) {
  constexpr prot::isa::Addr boundary_addr = 4096 - 2;
  const std::byte test_data[] = {std::byte{0x12}, std::byte{0x34},
                                 std::byte{0x56}, std::byte{0x78}};

  m_mem.writeBlock(test_data, boundary_addr);

  std::byte read_data[sizeof(test_data)];
  m_mem.readBlock(boundary_addr, read_data);

  EXPECT_TRUE(std::equal(std::begin(test_data), std::end(test_data),
                         std::begin(read_data)));
}

TEST_F(PageMemTest, pageFaultTest) {
  constexpr prot::isa::Addr unmapped_addr = 0x10000;

  std::byte buffer[4];

  EXPECT_THROW(m_mem.readBlock(unmapped_addr, buffer), prot::PageFault);

  const std::byte test_data[] = {std::byte{0xaa}};
  m_mem.writeBlock(test_data, 0x100);

  std::byte read_byte[1];
  m_mem.readBlock(0x100, read_byte);
  EXPECT_EQ(test_data[0], read_byte[0]);
}

TEST_F(PageMemTest, edgeCasesTest) {
  constexpr prot::isa::Addr end_addr = (1ULL << 16) - 4;
  const std::byte end_data[] = {std::byte{0xfe}, std::byte{0xed},
                                std::byte{0xfa}, std::byte{0xce}};
  m_mem.writeBlock(end_data, end_addr);

  std::byte read_end[4];
  m_mem.readBlock(end_addr, read_end);
  EXPECT_TRUE(std::equal(std::begin(end_data), std::end(end_data),
                         std::begin(read_end)));
}
