#include "prot/memory.hh"

#include <cassert>
#include <concepts>
#include <fmt/core.h>
#include <ranges>
#include <vector>

namespace prot::memory {
namespace {

struct PagedMemConfig {
  explicit PagedMemConfig(std::size_t pageBits)
      : m_pageBits(pageBits), m_offsetBits(sizeofBits<isa::Addr>() - pageBits) {
    if (pageBits >= sizeofBits<isa::Addr>()) {
      throw std::invalid_argument{
          fmt::format("Too many pages bits {}", pageBits)};
    }
  }

  [[nodiscard]] auto pageBits() const { return m_pageBits; }
  [[nodiscard]] auto pageMask() const {
    return ((1U << m_pageBits) - 1) << m_offsetBits;
  }
  [[nodiscard]] auto pageSize() const { return 1U << m_offsetBits; }
  [[nodiscard]] auto offsetMask() const { return pageSize() - 1; }
  [[nodiscard]] auto offsetBits() const { return m_offsetBits; }

  [[nodiscard]] std::size_t numPages() const { return 1U << m_pageBits; }

  [[nodiscard]] std::size_t getOffset(isa::Addr addr) const {
    return addr & offsetMask();
  }
  [[nodiscard]] std::size_t getPage(isa::Addr addr) const {
    return (addr & pageMask()) >> m_offsetBits;
  }

  [[nodiscard]] isa::Addr getAddr(std::size_t page, std::size_t offset) const {
    assert(page < numPages());
    assert(offset <= offsetMask());
    return (page << m_offsetBits) | offset;
  }

private:
  std::size_t m_pageBits{};
  std::size_t m_offsetBits{};
};

class PagedMem : public Memory, private PagedMemConfig {
  struct LocInfo final {
    std::size_t page{};
    std::size_t offset{};
    std::size_t size{};
  };

public:
  explicit PagedMem(std::size_t pageBits)
      : PagedMemConfig{pageBits}, m_storage(numPages()) {}

  void writeBlock(std::span<const std::byte> src, isa::Addr addr) override {
    pageWalk(addr, src.size(), [src, this, start = addr](const LocInfo &info) {
      assert(info.offset + info.size <= pageSize());
      auto &page = m_storage[info.page];
      auto addr = getAddr(info.page, info.offset);
      if (page.empty()) {
        page.resize(pageSize());
      }

      assert(page.size() == pageSize());

      std::ranges::copy(src.subspan(addr - start, info.size),
                        page.data() + info.offset);
    });
  }

  void readBlock(isa::Addr addr, std::span<std::byte> dest) const override {
    pageWalk(addr, dest.size(),
             [dest, this, start = addr](const LocInfo &info) {
               assert(info.offset + info.size <= pageSize());
               const auto &page = m_storage[info.page];
               auto addr = getAddr(info.page, info.offset);
               if (page.empty()) {
                 throw std::runtime_error{fmt::format(
                     "Trying to read range [{:#x}, {:#x}) which is located "
                     "outside of allocated memory",
                     addr, addr + info.size)};
               }

               assert(page.size() == pageSize());

               std::ranges::copy_n(page.data() + info.offset, info.size,
                                   dest.data() + (addr - start));
             });
  }

private:
  template <typename Self, std::regular_invocable<LocInfo> Op>
  static void pageWalk(Self &&self, isa::Addr addr, std::size_t size, Op op) {
    assert(addr + size >= addr);
    if (size == 0) {
      return;
    }
    std::size_t bytesLeft = size;
    const auto firstPage = self.getPage(addr);
    const auto lastPage = self.getPage(addr + size - 1) + 1;
    assert(firstPage <= lastPage);

    for (auto page : std::views::iota(firstPage, lastPage)) {
      std::size_t offset = page == firstPage ? self.getOffset(addr) : 0;
      std::size_t size = std::min(self.pageSize() - offset, bytesLeft);

      op(LocInfo{
          .page = page,
          .offset = offset,
          .size = size,
      });

      bytesLeft -= size;
    }
  }

  template <std::regular_invocable<LocInfo> Op>
  void pageWalk(isa::Addr addr, std::size_t size, Op op) const {
    pageWalk(*this, addr, size, std::move(op));
  }

  template <std::regular_invocable<LocInfo> Op>
  void pageWalk(isa::Addr addr, std::size_t size, Op op) {
    pageWalk(*this, addr, size, std::move(op));
  }

  using Page = std::vector<std::byte>;
  std::vector<Page> m_storage;
};
} // namespace

std::unique_ptr<Memory> makePaged(std::size_t pageBits) {
  return std::make_unique<PagedMem>(pageBits);
}
} // namespace prot::memory
