#ifndef PROT_PAGE_HH_INCLUDED
#define PROT_PAGE_HH_INCLUDED

#include <concepts>
#include <cstdint>
#include <stdexcept>
#include <sys/types.h>
#include <type_traits>
#include <vector>

#include "prot/isa.hh"

namespace prot {
template <std::uint64_t PageBits> class PageConfig {
public:
  static constexpr std::uint64_t getPageBits() { return kPageBits; }
  static constexpr std::uint64_t getPageSize() { return kPageSize; }
  static constexpr std::uint64_t getOffsetMask() { return kOffsetMask; }
  static constexpr std::uint64_t getIdMask() { return kIdMask; }

  static std::uint64_t getId(isa::Addr addr) {
    return (addr & kIdMask) >> kPageBits;
  }

  static isa::Addr getOffset(isa::Addr addr) { return addr & kOffsetMask; }

private:
  static constexpr std::uint64_t kPageBits{PageBits};
  static constexpr std::uint64_t kPageSize{1ULL << kPageBits};
  static constexpr std::uint64_t kOffsetMask{kPageSize - 1};
  static constexpr std::uint64_t kIdMask{~kOffsetMask};
};

template <typename T>
concept PageConfigConc = requires(T, isa::Addr addr) {
  { T::getPageSize() } -> std::same_as<std::uint64_t>;
  { T::getId(addr) } -> std::same_as<std::uint64_t>;
  { T::getOffset(addr) } -> std::same_as<isa::Addr>;
};

class PageFault : public std::out_of_range {
public:
  explicit PageFault(isa::Addr addr)
      : std::out_of_range("Page fault at address: 0x" + std::to_string(addr)),
        m_addr(addr) {}

  isa::Addr addr() const { return m_addr; }

private:
  isa::Addr m_addr;
};

template <PageConfigConc PageConfigT> class Page final {
public:
  Page() : m_data(PageConfigT::getPageSize()) {}

public:
  void writeBlock(const std::byte *src, isa::Addr offset, std::size_t size) {
    if (offset >= PageConfigT::getPageSize())
      throw std::out_of_range{"Offset out of page"};

    std::copy_n(src, size, m_data.data() + offset);
  }

  void readBlock(isa::Addr offset, std::size_t size, std::byte *dst) const {
    if (offset >= PageConfigT::getPageSize())
      throw std::out_of_range{"Offset out of page"};

    std::copy_n(m_data.data() + offset, size, dst);
  }

  auto at(isa::Addr addr) const { return m_data.at(addr); }
  auto getSize() const { return PageConfigT::getPageSize(); }

private:
  std::vector<std::byte> m_data{};
};
} // namespace prot

#endif // PROT_PAGE_HH_INCLUDED
